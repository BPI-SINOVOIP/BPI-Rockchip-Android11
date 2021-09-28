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
* @file ihevce_encode_header_sei_vui.h
*
* @brief
*  This file contains structures and interface prototypes for header vui/sei
*  encoding
*
* @author
*  ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_ENCODE_HEADER_SEI_VUI_H_
#define _IHEVCE_ENCODE_HEADER_SEI_VUI_H_

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/**
******************************************************************************
 *  @brief   Macro to calculate the CRC for a bit index
******************************************************************************
 */
#define CALC_CRC_BIT_LEVEL(u4_crc_val, u1_cur_val, bit_idx)                                        \
    {                                                                                              \
        UWORD32 u4_bit_val, u4_crc_msb;                                                            \
        u4_crc_msb = (u4_crc_val >> 15) & 1;                                                       \
        u4_bit_val = (u1_cur_val >> (7 - bit_idx)) & 1;                                            \
        u4_crc_val = (((u4_crc_val << 1) + u4_bit_val) & 0xffff) ^ (u4_crc_msb * 0x1021);          \
    }

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{
    /* SEI PREFIX */
    IHEVCE_SEI_BUF_PERIOD_T = 0,
    IHEVCE_SEI_PIC_TIMING_T,
    IHEVCE_SEI_PAN_SCAN_RECT_T,
    IHEVCE_SEI_FILLER_PAYLOAD_T,
    IHEVCE_SEI_USER_DATA_REGISTERED_ITU_T_T35_T,
    IHEVCE_SEI_USER_DATA_UNREGISTERED_T,
    IHEVCE_SEI_RECOVERY_POINT_T = 6,
    IHEVCE_SEI_SCENE_INFO_T = 9,
    IHEVCE_SEI_FULL_FRAME_SNAPSHOT_T = 15,
    IHEVCE_SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START_T = 16,
    IHEVCE_SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END_T = 17,
    IHEVCE_SEI_FILM_GRAIN_CHARACTERISTICS_T = 19,
    IHEVCE_SEI_POST_FILTER_HINT_T = 22,
    IHEVCE_SEI_TONE_MAPPING_INFO_T = 23,
    IHEVCE_SEI_FRAME_PACKING_ARRANGEMENT_T = 45,
    IHEVCE_SEI_DISPLAY_ORIENTATION_T = 47,
    IHEVCE_SEI_SOP_DESCRIPTION_T = 128,
    IHEVCE_SEI_ACTIVE_PARAMETER_SETS_T = 129,
    IHEVCE_SEI_DECODING_UNIT_INFO_T = 130,
    IHEVCE_SEI_TL0_INDEX_T = 131,
    IHEVCE_SEI_DECODED_PICTURE_HASH_T = 132, /* SEI SUFFIX */
    IHEVCE_SEI_SCALABLE_NESTING_T = 133,
    IHEVCE_SEI_REGION_REFRESH_INFO_T = 134,
    IHEVCE_SEI_MASTERING_DISP_COL_VOL_T = 137,
    IHEVCE_SEI_CONTENT_LIGHT_LEVEL_DATA_T = 144,

    /* SIE SUFFIX/PREFIX REST OF THE SEI  */
    IHEVCE_SEI_RESERVED_SEI_MESSAGE_T
} IHEVCE_SEI_TYPE;

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

WORD32 ihevce_generate_sub_layer_hrd_params(
    bitstrm_t *ps_bitstrm,
    sub_lyr_hrd_params_t *ps_sub_lyr_hrd_params,
    hrd_params_t *ps_hrd_params,
    WORD32 cpb_cnt_minus1);

WORD32
    ihevce_generate_hrd_params(bitstrm_t *ps_bitstrm, hrd_params_t *ps_hrd_params, sps_t *ps_sps);

WORD32 ihevce_generate_vui(bitstrm_t *ps_bitstrm, sps_t *ps_sps, vui_t s_vui);

WORD32 ihevce_put_buf_period_sei_params(
    buf_period_sei_params_t *ps_bp_sei, vui_t *ps_vui_params, bitstrm_t *ps_bitstrm);

WORD32 ihevce_put_active_parameter_set_sei_params(
    active_parameter_set_sei_param_t *ps_act_sei, bitstrm_t *ps_bitstrm);

WORD32 ihevce_put_recovery_point_sei_params(
    recovery_point_sei_params_t *ps_rp_sei, bitstrm_t *ps_bitstrm);

WORD32 ihevce_put_pic_timing_sei_params(
    pic_timing_sei_params_t *ps_pt_sei, vui_t *ps_vui_params, bitstrm_t *ps_bitstrm);

WORD32 ihevce_put_sei_msg(
    IHEVCE_SEI_TYPE e_payload_type,
    sei_params_t *ps_sei_params,
    vui_t *ps_vui_params,
    bitstrm_t *ps_bitstrm,
    UWORD32 i4_registered_user_data_length,
    UWORD8 *pu1_user_data_registered);

WORD32 ihevce_generate_sei(
    bitstrm_t *ps_bitstrm,
    sei_params_t *ps_sei_params,
    vui_t *ps_vui_params,
    WORD32 insert_per_cra,
    WORD32 nal_unit_header,
    UWORD32 u4_num_sei_payloads,
    sei_payload_t *ps_sei_payload);

WORD32 ihevce_populate_recovery_point_sei(
    sei_params_t *ps_sei, ihevce_vui_sei_params_t *ps_vui_sei_prms);

WORD32 ihevce_populate_mastering_disp_col_vol_sei(
    sei_params_t *ps_sei, ihevce_out_strm_params_t *ps_out_strm_prms);

WORD32 ihevce_populate_picture_timing_sei(
    sei_params_t *ps_sei,
    vui_t *ps_vui,
    ihevce_src_params_t *ps_src_params,
    WORD32 u4_bottom_field_flag);

WORD32 ihevce_populate_buffering_period_sei(
    sei_params_t *ps_sei, vui_t *ps_vui, sps_t *ps_sps, ihevce_vui_sei_params_t *ps_vui_sei_prms);

WORD32 ihevce_populate_active_parameter_set_sei(sei_params_t *ps_sei, vps_t *ps_vps, sps_t *ps_sps);

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
    WORD32 i4_frame_pos_y);

WORD32 ihevce_populate_vui(
    vui_t *ps_vui,
    sps_t *ps_sps,
    ihevce_src_params_t *ps_src_params,
    ihevce_vui_sei_params_t *ps_vui_sei_prms,
    WORD32 i4_resolution_id,
    ihevce_tgt_params_t *ps_tgt_params,
    ihevce_static_cfg_params_t *ps_stat_prms,
    WORD32 i4_bitrate_instance_id);

#endif  // _IHEVCE_ENCODE_HEADER_SEI_VUI_H_
