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
* @file ihevce_encode_header.h
*
* @brief
*  This file contains structures and interface prototypes for header encoding
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENCODE_HEADER_H_
#define _IHEVCE_ENCODE_HEADER_H_

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief   Macro to put a code with specified number of bits into the
 *           bitstream
******************************************************************************
 */
#define PUT_BITS(ps_bitstrm, code_val, code_len, ret_val)                                          \
    ret_val |= ihevce_put_bits((ps_bitstrm), (code_val), (code_len))

/**
******************************************************************************
 *  @brief   Macro to put a code with specified number of bits into the
 *           bitstream using 0th order exponential Golomb encoding for
 *           signed numbers
******************************************************************************
 */
#define PUT_BITS_UEV(ps_bitstrm, code_val, ret_val)                                                \
    ret_val |= ihevce_put_uev((ps_bitstrm), (code_val))

/**
******************************************************************************
 *  @brief   Macro to put a code with specified number of bits into the
 *           bitstream using 0th order exponential Golomb encoding for
 *           signed numbers
******************************************************************************
 */
#define PUT_BITS_SEV(ps_bitstrm, code_val, ret_val)                                                \
    ret_val |= ihevce_put_sev((ps_bitstrm), (code_val))

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

WORD32 ihevce_generate_nal_unit_header(
    bitstrm_t *ps_bitstrm, WORD32 nal_unit_type, WORD32 nuh_temporal_id);

WORD32 ihevce_generate_aud(bitstrm_t *ps_bitstrm, WORD32 pic_type);

WORD32 ihevce_generate_eos(bitstrm_t *ps_bitstrm);

WORD32 ihevce_generate_vps(bitstrm_t *ps_bitstrm, vps_t *ps_vps);

WORD32 ihevce_generate_sps(bitstrm_t *ps_bitstrm, sps_t *ps_sps);

WORD32 ihevce_generate_pps(bitstrm_t *ps_bitstrm, pps_t *ps_pps);

WORD32 ihevce_generate_slice_header(
    bitstrm_t *ps_bitstrm,
    WORD8 i1_nal_unit_type,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    bitstrm_t *ps_dup_bit_strm_ent_offset,
    UWORD32 *pu4_first_slice_start_offset,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 i4_next_slice_seg_x,
    WORD32 i4_next_slice_seg_y);

WORD32 ihevce_populate_vps(
    enc_ctxt_t *ps_enc_ctxt,
    vps_t *ps_vps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_resolution_id);

WORD32 ihevce_populate_sps(
    enc_ctxt_t *ps_enc_ctxt,
    sps_t *ps_sps,
    vps_t *ps_vps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_resolution_id);

WORD32 ihevce_populate_pps(
    pps_t *ps_pps,
    sps_t *ps_sps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id,
    ihevce_tile_params_t *ps_tile_params_base,
    WORD32 *pi4_column_width_array,
    WORD32 *pi4_row_height_array);

WORD32 ihevce_populate_slice_header(
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    WORD32 nal_unit_type,
    WORD32 slice_type,
    WORD32 ctb_x,
    WORD32 ctb_y,
    WORD32 poc,
    WORD32 cur_slice_qp,
    WORD32 max_merge_candidates,
    WORD32 i4_rc_pass_num,
    WORD32 i4_quality_preset,
    WORD32 stasino_enabled);

WORD32 ihevce_insert_entry_offset_slice_header(
    bitstrm_t *ps_bitstrm,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    UWORD32 u4_first_slice_start_offset);

#endif  // _IHEVCE_ENCODE_HEADER_H_
