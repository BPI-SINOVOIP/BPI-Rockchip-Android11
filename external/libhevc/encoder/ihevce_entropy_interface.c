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
* @file ihevce_entropy_interface.c
*
* @brief
*    This file contains function definitions for entropy interface related to
*    memory init and process apis
*
* @author
*    Ittiam
*
* List of Functions
*   ihevce_entropy_get_num_mem_recs()
*   ihevce_entropy_size_of_out_buffer()
*   ihevce_entropy_get_mem_recs()
*   ihevce_entropy_init()
*   ihevce_entropy_encode_frame()
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
#include "ihevce_entropy_structs.h"
#include "ihevce_entropy_interface.h"
#include "ihevce_encode_header.h"
#include "ihevce_encode_header_sei_vui.h"
#include "ihevce_trace.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Number of memory records are returned for entropy module
*
*  @par   Description
*
*  @return      number of memory records
*
******************************************************************************
*/
WORD32 ihevce_entropy_get_num_mem_recs(void)
{
    return (NUM_ENTROPY_MEM_RECS);
}

/**
******************************************************************************
*
*  @brief Estimated bitstream buffer size basing on input dimensions
*
*  @par   Description
*
*  @return      bitstream buffer size
*
******************************************************************************
*/
WORD32 ihevce_entropy_size_of_out_buffer(frm_proc_ent_cod_ctxt_t *ps_curr_inp)
{
    WORD32 i4_size;

    i4_size = (WORD32)(
        ps_curr_inp->ps_sps->i2_pic_height_in_luma_samples *
        ps_curr_inp->ps_sps->i2_pic_width_in_luma_samples);

    return (i4_size);
}

/**
******************************************************************************
*
*  @brief Populates Memory requirements of the entropy module
*
*  @par   Description
*
*  @param[inout]   ps_mem_tab
*  pointer to memory descriptors table
*
*  @param[in]      ps_init_prms
*  Create time static parameters
*
*  @param[in]      i4_mem_space
*  memspace in whihc memory request should be done
*
*  @return      number of memory requirements filled
*
******************************************************************************
*/
WORD32 ihevce_entropy_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id)
{
    /* memories should be requested assuming worst case requirememnts */
    WORD32 max_width = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
    WORD32 max_height = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
    WORD32 max_align_width = ALIGN64(max_width);
    WORD32 max_align_height = ALIGN64(max_height);

    /* Module context structure */
    ps_mem_tab[ENTROPY_CTXT].i4_mem_size = sizeof(entropy_context_t);
    ps_mem_tab[ENTROPY_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[ENTROPY_CTXT].i4_mem_alignment = 64;

    /* top row cu skip flags (1 bit per 8x8CU)  */
    ps_mem_tab[ENTROPY_TOP_SKIP_FLAGS].i4_mem_size = max_align_width >> 6;
    ps_mem_tab[ENTROPY_TOP_SKIP_FLAGS].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[ENTROPY_TOP_SKIP_FLAGS].i4_mem_alignment = 64;

    /* top row CU Depth (1 byte per 8x8CU) */
    ps_mem_tab[ENTROPY_TOP_CU_DEPTH].i4_mem_size = (max_align_width >> 3);
    ps_mem_tab[ENTROPY_TOP_CU_DEPTH].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[ENTROPY_TOP_CU_DEPTH].i4_mem_alignment = 64;

    /* Dummy_buffer to handle first pass MBR case*/
    ps_mem_tab[ENTROPY_DUMMY_OUT_BUF].i4_mem_size = (max_align_width * max_align_height * 2);
    ps_mem_tab[ENTROPY_DUMMY_OUT_BUF].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[ENTROPY_DUMMY_OUT_BUF].i4_mem_alignment = 64;

    return (NUM_ENTROPY_MEM_RECS);
}

/**
******************************************************************************
*
*  @brief Intialization of entropy module
*
*  @par   Description
*  pointers of the memory requests done in ihevce_entropy_get_mem_recs() are
*  used to initialized the entropy module and the handle is returned
*
*  @param[inout]   ps_mem_tab
*  pointer to memory descriptors table
*
*  @param[in]      ps_init_prms
*  Create time static parameters
*
*  @return
*  Handle of the entropy module returned as void ptr
*
******************************************************************************
*/
void *ihevce_entropy_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    void *pv_tile_params_base,
    WORD32 i4_res_id)
{
    entropy_context_t *ps_entropy_ctxt;

    /* Entropy state structure */
    ps_entropy_ctxt = (entropy_context_t *)ps_mem_tab[ENTROPY_CTXT].pv_base;
    memset(ps_entropy_ctxt, 0, sizeof(entropy_context_t));

    ps_entropy_ctxt->pu1_skip_cu_top = (UWORD8 *)ps_mem_tab[ENTROPY_TOP_SKIP_FLAGS].pv_base;
    ps_entropy_ctxt->pu1_cu_depth_top = (UWORD8 *)ps_mem_tab[ENTROPY_TOP_CU_DEPTH].pv_base;
    ps_entropy_ctxt->pv_dummy_out_buf = ps_mem_tab[ENTROPY_DUMMY_OUT_BUF].pv_base;
    ps_entropy_ctxt->i4_bitstream_buf_size = ps_mem_tab[ENTROPY_DUMMY_OUT_BUF].i4_mem_size;

    /* perform all one time initialisation here */
    /*************************************************************************/
    /* Note pu1_cbf_cb, pu1_cbf_cr initialization are done with array idx 1  */
    /* This is because these flags are accessed as pu1_cbf_cb[tfr_depth - 1] */
    /* without cheking for tfr_depth= 0                                      */
    /*************************************************************************/
    ps_entropy_ctxt->apu1_cbf_cb[0] = &ps_entropy_ctxt->au1_cbf_cb[0][1];
    ps_entropy_ctxt->apu1_cbf_cr[0] = &ps_entropy_ctxt->au1_cbf_cr[0][1];
    ps_entropy_ctxt->apu1_cbf_cb[1] = &ps_entropy_ctxt->au1_cbf_cb[1][1];
    ps_entropy_ctxt->apu1_cbf_cr[1] = &ps_entropy_ctxt->au1_cbf_cr[1][1];

    memset(ps_entropy_ctxt->au1_cbf_cb, 0, (MAX_TFR_DEPTH + 1) * 2 * sizeof(UWORD8));

    /* register codec level */
    ps_entropy_ctxt->i4_codec_level =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_res_id].i4_codec_level;

    /*  Flag to enable/disable insertion of SPS, VPS & PPS at every CRA frame   */
    ps_entropy_ctxt->i4_sps_at_cdr_enable = ps_init_prms->s_out_strm_prms.i4_sps_at_cdr_enable;

    /* Store Tile params base into entropy context */
    ps_entropy_ctxt->pv_tile_params_base = pv_tile_params_base;

    ps_entropy_ctxt->pv_sys_api = (void *)&ps_init_prms->s_sys_api;

    ps_entropy_ctxt->i4_slice_segment_mode = ps_init_prms->s_slice_params.i4_slice_segment_mode;

    /* Set slice segment length */
    if((ps_entropy_ctxt->i4_slice_segment_mode == 1) ||
       (ps_entropy_ctxt->i4_slice_segment_mode == 2))
    {
        ps_entropy_ctxt->i4_slice_segment_max_length =
            ps_init_prms->s_slice_params.i4_slice_segment_argument;
    }
    else
    {
        ps_entropy_ctxt->i4_slice_segment_max_length = 0;
    }

    /* return the handle to caller */
    return ((void *)ps_entropy_ctxt);
}

/**
******************************************************************************
*
*  @brief entry point for entropy coding of a frame
*
*  @par   Description
*  This function generates nal headers like SPS/PPS/slice header and call the
*  slice data entropy coding function
*
*  @param[in]   ps_enc_ctxt
*  pointer to encoder context (handle)
*
*  @param[out]   ps_curr_out
*  pointer to output data buffer context where bitstream is generated
*
*  @param[out]   ps_curr_inp
*  pointer to entropy input params context
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_entropy_encode_frame(
    void *pv_entropy_hdl,
    iv_output_data_buffs_t *ps_curr_out,
    frm_proc_ent_cod_ctxt_t *ps_curr_inp,
    WORD32 i4_out_buf_size)
{
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 tile_ctr, total_tiles = 1;
    entropy_context_t *ps_entropy_ctxt = (entropy_context_t *)pv_entropy_hdl;

    /* current frame slice type and nal type */
    WORD32 slice_type = ps_curr_inp->s_slice_hdr.i1_slice_type;

    /* current frame slice type and nal type */
    WORD32 nal_type = ps_curr_inp->i4_slice_nal_type;

    /* read vps, sps and pps from input params */
    vps_t *ps_vps = ps_curr_inp->ps_vps;
    sps_t *ps_sps = ps_curr_inp->ps_sps;
    pps_t *ps_pps = ps_curr_inp->ps_pps;
    sei_params_t *ps_sei = &ps_curr_inp->s_sei;
    ihevce_tile_params_t *ps_tile_params_base;
    WORD32 out_buf_size = i4_out_buf_size;

    /* Headers are repeated once per IDR. Should be changed to every CRA */
    WORD32 insert_vps_sps_pps =
        ((slice_type == ISLICE) &&
         (((NAL_IDR_N_LP == nal_type) || (NAL_CRA == nal_type)) || (NAL_IDR_W_LP == nal_type)));

    WORD32 insert_per_cra =
        ((slice_type == ISLICE) &&
         (((NAL_IDR_N_LP == nal_type) || (NAL_CRA == nal_type)) || (NAL_IDR_W_LP == nal_type)));
    bitstrm_t *ps_bitstrm = &ps_entropy_ctxt->s_bit_strm;

    ULWORD64 u8_bits_slice_header_prev;

    WORD32 i4_slice_segment_max_length_bckp;
    WORD32 i4_max_num_slices;

    ihevce_sys_api_t *ps_sys_api = (ihevce_sys_api_t *)ps_entropy_ctxt->pv_sys_api;

#if POPULATE_NAL_OFFSET
    ULWORD64 u8_bitstream_base = (ULWORD64)ps_curr_out->pv_bitstream_bufs;
#endif
    if(0 == ps_entropy_ctxt->i4_sps_at_cdr_enable)
    {
        insert_vps_sps_pps =
            ((slice_type == ISLICE) && ((NAL_IDR_N_LP == nal_type) || (NAL_IDR_W_LP == nal_type)));
    }
    /* intialize vps, sps, pps, sei and slice header in entropy context */
    ps_entropy_ctxt->ps_vps = ps_vps;
    ps_entropy_ctxt->ps_sps = ps_sps;
    ps_entropy_ctxt->ps_pps = ps_pps;
    ps_entropy_ctxt->ps_sei = ps_sei;
    ps_entropy_ctxt->ps_slice_hdr = &ps_curr_inp->s_slice_hdr;
    ps_entropy_ctxt->i4_is_cu_cbf_zero = 1;

    ps_entropy_ctxt->ps_pic_level_info = &ps_curr_inp->s_pic_level_info;

    /* intialize the frame level ctb pointer for current slice */
    ps_entropy_ctxt->ps_frm_ctb = ps_curr_inp->ps_frm_ctb_data;

    /* Initiallizing to indicate the start of frame */
    ps_entropy_ctxt->i4_next_slice_seg_x = 0;
    ps_entropy_ctxt->i4_next_slice_seg_y = 0;

    /* enable the residue encode flag */
    ps_entropy_ctxt->i4_enable_res_encode = 1;

    /* Initialize the bitstream engine */
    ret |= ihevce_bitstrm_init(ps_bitstrm, (UWORD8 *)ps_curr_out->pv_bitstream_bufs, out_buf_size);

    /* Reset Bitstream NAL counter */
    ps_bitstrm->i4_num_nal = 0;

    /*PIC INFO: Store the Bits before slice header is encoded*/
    u8_bits_slice_header_prev = (ps_bitstrm->u4_strm_buf_offset * 8);

    /* generate AUD if enabled from the application */
    if(1 == ps_curr_inp->i1_aud_present_flag)
    {
        UWORD8 u1_pic_type;

        switch(slice_type)
        {
        case ISLICE:
            u1_pic_type = 0;
            break;
        case PSLICE:
            u1_pic_type = 1;
            break;
        default:
            u1_pic_type = 2;
            break;
        }

        ret |= ihevce_generate_aud(ps_bitstrm, u1_pic_type);
    }

    if(insert_vps_sps_pps)
    {
        /* generate vps */
        ret |= ihevce_generate_vps(ps_bitstrm, ps_entropy_ctxt->ps_vps);

        /* generate sps */
        ret |= ihevce_generate_sps(ps_bitstrm, ps_entropy_ctxt->ps_sps);

        /* generate pps */
        ret |= ihevce_generate_pps(ps_bitstrm, ps_entropy_ctxt->ps_pps);
    }

    /* generate sei */
    if(1 == ps_entropy_ctxt->ps_sei->i1_sei_parameters_present_flag)
    {
        WORD32 i4_insert_prefix_sei =
            ps_entropy_ctxt->ps_sei->i1_buf_period_params_present_flag ||
            ps_entropy_ctxt->ps_sei->i1_pic_timing_params_present_flag ||
            ps_entropy_ctxt->ps_sei->i1_recovery_point_params_present_flag ||
            ps_entropy_ctxt->ps_sei->i4_sei_mastering_disp_colour_vol_params_present_flags ||
            ps_curr_inp->u4_num_sei_payload || ps_curr_inp->s_sei.i1_sei_cll_enable;

        if(i4_insert_prefix_sei)
        {
            ret |= ihevce_generate_sei(
                ps_bitstrm,
                ps_entropy_ctxt->ps_sei,
                &ps_entropy_ctxt->ps_sps->s_vui_parameters,
                insert_per_cra,
                NAL_PREFIX_SEI,
                ps_curr_inp->u4_num_sei_payload,
                &ps_curr_inp->as_sei_payload[0]);
        }
    }

    /*PIC INFO: Populate slice header bits */
    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_slice_header +=
        (ps_bitstrm->u4_strm_buf_offset * 8) - u8_bits_slice_header_prev;

    ps_tile_params_base = (ihevce_tile_params_t *)ps_entropy_ctxt->pv_tile_params_base;

    ps_curr_out->i4_bytes_generated = 0;  //Init

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

    total_tiles = ps_tile_params_base->i4_num_tiles;

    /* frame level NUM slices related params initialisations */
    {
        WORD32 codec_level_index = ihevce_get_level_index(ps_entropy_ctxt->i4_codec_level);

        i4_max_num_slices = g_as_level_data[codec_level_index].i4_max_slices_per_picture;
        ps_entropy_ctxt->i4_num_slice_seg = 0;
    }

    /* back up slice arg length before pic encoding */
    i4_slice_segment_max_length_bckp = ps_entropy_ctxt->i4_slice_segment_max_length;

    for(tile_ctr = 0; tile_ctr < total_tiles; tile_ctr++)
    {
        WORD32 i4_end_of_slice = 0;

        /* Loop over all the slice segments */
        while(0 == i4_end_of_slice)
        {
            WORD32 i4_bytes_generated, i4_slice_header_bits;

            /*PIC INFO: Store the Bits before slice header is encoded*/
            u8_bits_slice_header_prev = (ps_bitstrm->u4_strm_buf_offset * 8);

            /* generate slice header */
            ret |= ihevce_generate_slice_header(
                ps_bitstrm,
                nal_type,
                ps_entropy_ctxt->ps_slice_hdr,
                ps_entropy_ctxt->ps_pps,
                ps_entropy_ctxt->ps_sps,
                &ps_entropy_ctxt->s_dup_bit_strm_ent_offset,
                &ps_entropy_ctxt->s_cabac_ctxt.u4_first_slice_start_offset,
                (ps_tile_params_base + tile_ctr),
                ps_entropy_ctxt->i4_next_slice_seg_x,
                ps_entropy_ctxt->i4_next_slice_seg_y);

            i4_slice_header_bits =
                (ps_bitstrm->u4_strm_buf_offset * 8) - (WORD32)u8_bits_slice_header_prev;

            /* Update slice segment length with bytes in slice header */
            if(2 == ps_entropy_ctxt->i4_slice_segment_mode)
            {
                ps_entropy_ctxt->i4_slice_seg_len = (i4_slice_header_bits / 8);
            }
            else  //Initiallize to zero
            {
                ps_entropy_ctxt->i4_slice_seg_len = 0;
            }

            /*PIC INFO: Populate slice header bits */
            ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_slice_header +=
                i4_slice_header_bits;

            /* check if number of slices generated in is MAX -1 as per codec_level */
            if(ps_entropy_ctxt->i4_num_slice_seg == (i4_max_num_slices - 1))
            {
                /* i4_slice_segment_max_length is set to a huge positive value          */
                /* so that remaining CTBS in the picture gets encoded as a single slice */
                ps_entropy_ctxt->i4_slice_segment_max_length = 0x7FFFFFFF;
            }

            /* encode the slice data */
            ret |= ihevce_encode_slice_data(
                ps_entropy_ctxt, (ps_tile_params_base + tile_ctr), &i4_end_of_slice);

            /* increment the number of slices generated */
            ps_entropy_ctxt->i4_num_slice_seg++;

            if(1 == ps_pps->i1_entropy_coding_sync_enabled_flag)
            {
                /*after encoding is done each slice offset is available. Enter these offset in slice header*/
                ihevce_insert_entry_offset_slice_header(
                    &ps_entropy_ctxt->s_dup_bit_strm_ent_offset,
                    ps_entropy_ctxt->ps_slice_hdr,
                    ps_entropy_ctxt->ps_pps,
                    ps_entropy_ctxt->s_cabac_ctxt.u4_first_slice_start_offset);
            }

            /* compute the bytes generated and return */
            if(1 == ps_pps->i1_entropy_coding_sync_enabled_flag)
            {
                i4_bytes_generated = ps_entropy_ctxt->s_dup_bit_strm_ent_offset.u4_strm_buf_offset;
            }
            else
            {
                i4_bytes_generated = ps_entropy_ctxt->s_cabac_ctxt.u4_strm_buf_offset;
            }

            /* Updating bytes generated and Updating strm_buffer pointer */
            ps_curr_out->i4_bytes_generated += i4_bytes_generated;

            /* Re-Initialize the bitstream engine after each tile or slice */
            ihevce_bitstrm_init(
                ps_bitstrm, (ps_bitstrm->pu1_strm_buffer + i4_bytes_generated), out_buf_size);
        }
    }

    /* Max slices related warning prints based on last slice status */
    if(ps_entropy_ctxt->i4_num_slice_seg == i4_max_num_slices)
    {
        if(ps_entropy_ctxt->i4_slice_seg_len >= i4_slice_segment_max_length_bckp)
        {
            if(1 == ps_entropy_ctxt->i4_slice_segment_mode)
            {
                ps_sys_api->ihevce_printf(
                    ps_sys_api->pv_cb_handle,
                    "IHEVCE_WARNING: Last slice contains %d CTBs exceeds %d (Max limit of CTBs "
                    "configured). As per codec_level max number of slices per frame is %d\n",
                    ps_entropy_ctxt->i4_slice_seg_len,
                    i4_slice_segment_max_length_bckp,
                    i4_max_num_slices);
            }
            else if(2 == ps_entropy_ctxt->i4_slice_segment_mode)
            {
                ps_sys_api->ihevce_printf(
                    ps_sys_api->pv_cb_handle,
                    "IHEVCE_WARNING: Last slice contains %d Bytes exceeds %d (Max limit of Bytes "
                    "configured). As per codec_level max number of slices per frame is %d\n",
                    ps_entropy_ctxt->i4_slice_seg_len,
                    i4_slice_segment_max_length_bckp,
                    i4_max_num_slices);
            }
        }
    }

    /* restore slice arg length after pic encoding */
    ps_entropy_ctxt->i4_slice_segment_max_length = i4_slice_segment_max_length_bckp;

    /* ---------------------- Initialize VCL NAL Size/offsets ---------------------------*/
    {
        WORD32 vcl_start = ps_curr_out->i4_num_non_vcl_prefix_nals;
        WORD32 num_vcl_nals = ps_bitstrm->i4_num_nal - vcl_start;
        WORD32 ctr = 0;

        ASSERT(num_vcl_nals > 0);
        ASSERT(num_vcl_nals <= MAX_NUM_VCL_NALS_PER_AU);

        ps_curr_out->i4_num_vcl_nals = num_vcl_nals;
        for(ctr = 0; ctr < MIN(num_vcl_nals, MAX_NUM_VCL_NALS_PER_AU); ctr++)
        {
            /* NAL offset is derive by subtracting Bistream base from NAL start pointer */
            ULWORD64 u8_cur_nal_start = (ULWORD64)ps_bitstrm->apu1_nal_start[ctr + vcl_start];

#if POPULATE_NAL_SIZE

            /* ----------Populate NAL Size  -------------*/
            if((ctr + 1) < num_vcl_nals)
            {
                ULWORD64 u8_next_nal_start =
                    (ULWORD64)ps_bitstrm->apu1_nal_start[ctr + vcl_start + 1];
                ps_curr_out->ai4_size_vcl_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            else
            {
                ULWORD64 u8_next_nal_start =
                    (ULWORD64)ps_bitstrm->pu1_strm_buffer + ps_bitstrm->u4_strm_buf_offset;
                ps_curr_out->ai4_size_vcl_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            ASSERT(ps_curr_out->ai4_size_vcl_nals[ctr] > 0);

#elif POPULATE_NAL_OFFSET

            /* ----------Populate NAL Offset  -------------*/

            ASSERT(u8_cur_nal_start >= u8_bitstream_base);
            ps_curr_out->ai4_off_vcl_nals[ctr] = (UWORD32)(u8_cur_nal_start - u8_bitstream_base);

            if(ctr)
            {
                /* sanity check on increasing NAL offsets */
                ASSERT(ps_curr_out->ai4_off_vcl_nals[ctr] > ps_curr_out->ai4_off_vcl_nals[ctr - 1]);
            }
#endif /* POPULATE_NAL_SIZE */
        }
    }

    /* generate suffix sei */
    if(1 == ps_entropy_ctxt->ps_sei->i1_sei_parameters_present_flag)
    {
        /* Insert hash SEI */
        if(0 != ps_entropy_ctxt->ps_sei->i1_decoded_pic_hash_sei_flag)
        {
            ret |= ihevce_generate_sei(
                ps_bitstrm,
                ps_entropy_ctxt->ps_sei,
                &ps_entropy_ctxt->ps_sps->s_vui_parameters,
                insert_per_cra,
                NAL_SUFFIX_SEI,
                ps_curr_inp->u4_num_sei_payload,
                &ps_curr_inp->as_sei_payload[0]);
        }

        /* Updating bytes generated */
        ps_curr_out->i4_bytes_generated += ps_bitstrm->u4_strm_buf_offset;
    }

    /* generate end of sequence nal */
    if((1 == ps_curr_inp->i1_eos_present_flag) && (ps_curr_inp->i4_is_end_of_idr_gop == 1))
    {
        ret |= ihevce_generate_eos(ps_bitstrm);
        /* Updating bytes generated */
        ps_curr_out->i4_bytes_generated += ps_bitstrm->u4_strm_buf_offset;
    }

    /* ------------------- Initialize non-VCL suffix NAL Size/offsets -----------------------*/
    {
        WORD32 non_vcl_suffix_start =
            ps_curr_out->i4_num_non_vcl_prefix_nals + ps_curr_out->i4_num_vcl_nals;
        WORD32 num_non_vcl_suffix_nals = ps_bitstrm->i4_num_nal - non_vcl_suffix_start;
        WORD32 ctr = 0;

        ASSERT(num_non_vcl_suffix_nals >= 0);
        ASSERT(num_non_vcl_suffix_nals <= MAX_NUM_SUFFIX_NALS_PER_AU);

        ps_curr_out->i4_num_non_vcl_suffix_nals = num_non_vcl_suffix_nals;
        for(ctr = 0; ctr < MIN(num_non_vcl_suffix_nals, MAX_NUM_SUFFIX_NALS_PER_AU); ctr++)
        {
            /* NAL offset is derive by subtracting Bistream base from NAL start pointer */
            ULWORD64 u8_cur_nal_start =
                (ULWORD64)ps_bitstrm->apu1_nal_start[ctr + non_vcl_suffix_start];

#if POPULATE_NAL_SIZE

            /* ----------Populate NAL Size  -------------*/
            if((ctr + 1) < num_non_vcl_suffix_nals)
            {
                ULWORD64 u8_next_nal_start =
                    (ULWORD64)ps_bitstrm->apu1_nal_start[ctr + non_vcl_suffix_start + 1];
                ps_curr_out->ai4_size_non_vcl_suffix_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            else
            {
                ULWORD64 u8_next_nal_start =
                    (ULWORD64)ps_bitstrm->pu1_strm_buffer + ps_bitstrm->u4_strm_buf_offset;
                ps_curr_out->ai4_size_non_vcl_suffix_nals[ctr] =
                    (UWORD32)(u8_next_nal_start - u8_cur_nal_start);
            }
            ASSERT(ps_curr_out->ai4_size_non_vcl_suffix_nals[ctr] > 0);

#elif POPULATE_NAL_OFFSET

            /* ----------Populate NAL Offset  -------------*/

            ASSERT(u8_cur_nal_start >= u8_bitstream_base);
            ps_curr_out->ai4_off_non_vcl_suffix_nals[ctr] =
                (UWORD32)(u8_cur_nal_start - u8_bitstream_base);

            if(ctr)
            {
                /* sanity check on increasing NAL offsets */
                ASSERT(
                    ps_curr_out->ai4_off_non_vcl_suffix_nals[ctr] >
                    ps_curr_out->ai4_off_non_vcl_suffix_nals[ctr - 1]);
            }
#endif /* POPULATE_NAL_SIZE */
        }
    }

    /*PIC INFO: Populatinf Ref POC, weights and offset*/
    {
        WORD32 i;
        ps_entropy_ctxt->ps_pic_level_info->i1_num_ref_idx_l0_active =
            ps_entropy_ctxt->ps_slice_hdr->i1_num_ref_idx_l0_active;
        ps_entropy_ctxt->ps_pic_level_info->i1_num_ref_idx_l1_active =
            ps_entropy_ctxt->ps_slice_hdr->i1_num_ref_idx_l1_active;
        for(i = 0; i < ps_entropy_ctxt->ps_slice_hdr->i1_num_ref_idx_l0_active; i++)
        {
            ps_entropy_ctxt->ps_pic_level_info->i4_ref_poc_l0[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_rplm.i4_ref_poc_l0[i];
            ps_entropy_ctxt->ps_pic_level_info->i1_list_entry_l0[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_rplm.i1_list_entry_l0[i];
            ps_entropy_ctxt->ps_pic_level_info->i2_luma_weight_l0[i] =
                (DOUBLE)ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i2_luma_weight_l0[i] /
                (1 << ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i1_luma_log2_weight_denom);
            ps_entropy_ctxt->ps_pic_level_info->i2_luma_offset_l0[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i2_luma_offset_l0[i];
        }
        for(i = 0; i < ps_entropy_ctxt->ps_slice_hdr->i1_num_ref_idx_l1_active; i++)
        {
            ps_entropy_ctxt->ps_pic_level_info->i4_ref_poc_l1[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_rplm.i4_ref_poc_l1[i];
            ps_entropy_ctxt->ps_pic_level_info->i1_list_entry_l1[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_rplm.i1_list_entry_l1[i];
            ps_entropy_ctxt->ps_pic_level_info->i2_luma_weight_l1[i] =
                (DOUBLE)ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i2_luma_weight_l1[i] /
                (1 << ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i1_luma_log2_weight_denom);
            ps_entropy_ctxt->ps_pic_level_info->i2_luma_offset_l1[i] =
                ps_entropy_ctxt->ps_slice_hdr->s_wt_ofst.i2_luma_offset_l1[i];
        }
    }

    /* attach the time stamp of the input to output */
    ps_curr_out->i4_out_timestamp_low = ps_curr_inp->i4_inp_timestamp_low;

    ps_curr_out->i4_out_timestamp_high = ps_curr_inp->i4_inp_timestamp_high;

    /*attach the app frame info of this buffer */
    ps_curr_out->pv_app_frm_ctxt = ps_curr_inp->pv_app_frm_ctxt;

    /* frame never skipped for now */
    ps_curr_out->i4_frame_skipped = 0;

    /* update error code and return */
    ps_curr_out->i4_process_error_code = ret;

    switch(slice_type)
    {
    case ISLICE:
        if((nal_type == NAL_IDR_N_LP) || (NAL_IDR_W_LP == nal_type))
        {
            ps_curr_out->i4_encoded_frame_type = IV_IDR_FRAME;
        }
        else
        {
            ps_curr_out->i4_encoded_frame_type = IV_I_FRAME;
        }
        break;
    case PSLICE:
        ps_curr_out->i4_encoded_frame_type = IV_P_FRAME;
        break;
    case BSLICE:
        ps_curr_out->i4_encoded_frame_type = IV_B_FRAME;
        break;
    }

    if(IHEVCE_SUCCESS == ret)
    {
        ps_curr_out->i4_process_ret_sts = IV_SUCCESS;
    }
    else
    {
        ps_curr_out->i4_process_ret_sts = IV_FAIL;
    }

    return (ret);
}
