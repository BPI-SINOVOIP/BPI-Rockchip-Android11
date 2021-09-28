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
* \file cbr_buffer_control.h
*
* \brief
*    This file contains all the necessary declarations for
*    cbr_buffer_control functions
*
* \date
* 06/05/2008
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef CBR_BUFFER_CONTROL_H
#define CBR_BUFFER_CONTROL_H

/*****************************************************************************/
/*  Macros                                                           */
/*****************************************************************************/
/* Macro for clipping a number between to extremes */
#define CLIP(Number, Max, Min)                                                                     \
    if((Number) > (Max))                                                                           \
        (Number) = (Max);                                                                          \
    else if((Number) < (Min))                                                                      \
        (Number) = (Min);
/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct cbr_buffer_t *cbr_buffer_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
WORD32 cbr_buffer_num_fill_use_free_memtab(
    cbr_buffer_handle *pps_cbr_buffer, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);
/* Initialize the cbr Buffer*/
void init_cbr_buffer(
    cbr_buffer_handle ps_cbr_buffer,
    WORD32 i4_buffer_delay,
    WORD32 i4_tgt_frm_rate,
    UWORD32 u4_bit_rate,
    UWORD32 *u4_num_pics_in_delay_prd,
    UWORD32 u4_vbv_buf_size,
    UWORD32 u4_intra_frm_int,
    rc_type_e u4_rc_type,
    UWORD32 u4_peak_bit_rate,
    UWORD32 u4_num_frames_in_delay,
    float f_max_peak_rate_dur,
    LWORD64 i8_num_frames_to_encode,
    WORD32 i4_inter_frm_int,
    WORD32 i4_cbr_rc_pass,
    WORD32 i4_capped_vbr_flag);

/* Check for tgt bits with in CBR buffer*/
WORD32 cbr_buffer_constraint_check(
    cbr_buffer_handle ps_cbr_buffer,
    WORD32 i4_tgt_bits,
    picture_type_e e_pic_type,
    WORD32 *pi4_max_tgt_bits,
    WORD32 *pi4_min_tgt_bits);

/* Get the buffer status with the current consumed bits*/
vbv_buf_status_e get_cbr_buffer_status(
    cbr_buffer_handle ps_cbr_buffer,
    WORD32 i4_tot_consumed_bits,
    WORD32 *pi4_num_bits_to_prevent_overflow,
    picture_type_e e_pic_type,
    rc_type_e e_rc_type);

/* Update the CBR buffer at the end of the VOP*/
void update_cbr_buffer(
    cbr_buffer_handle ps_cbr_buffer, WORD32 i4_tot_consumed_bits, picture_type_e e_pic_type);

/*Get the bits needed to stuff in case of Underflow*/
WORD32 get_cbr_bits_to_stuff(
    cbr_buffer_handle ps_cbr_buffer, WORD32 i4_tot_consumed_bits, picture_type_e e_pic_type);
WORD32 get_cbr_buffer_delay(cbr_buffer_handle ps_cbr_buffer);
WORD32 get_cbr_buffer_size(cbr_buffer_handle ps_cbr_buffer);
WORD32 get_cbr_ebf(cbr_buffer_handle ps_cbr_buffer);
WORD32 get_cbr_max_ebf(cbr_buffer_handle ps_cbr_buffer);
void update_cbr_buf_mismatch_bit(cbr_buffer_handle ps_cbr_buffer, WORD32 i4_error_bits);

WORD32 get_error_bits_for_desired_buf(
    cbr_buffer_handle ps_cbr_buffer, WORD32 i4_lap_complexity_q7, WORD32 i4_bit_alloc_period);

WORD32 get_buf_max_drain_rate(cbr_buffer_handle ps_cbr_buffer);

WORD32 vbr_stream_buffer_constraint_check(
    cbr_buffer_handle ps_cbr_buffer,
    WORD32 i4_tgt_bits,
    picture_type_e e_pic_type,
    WORD32 *pi4_max_tgt_bits,
    WORD32 *pi4_min_tgt_bits);

void change_cbr_vbv_bit_rate(
    cbr_buffer_handle ps_cbr_buffer, WORD32 *i4_bit_rate, WORD32 i4_peak_bitrate);
void change_cbr_vbv_tgt_frame_rate(cbr_buffer_handle ps_cbr_buffer, WORD32 i4_tgt_frm_rate);
void change_cbr_vbv_num_pics_in_delay_period(
    cbr_buffer_handle ps_cbr_buffer, UWORD32 *u4_num_pics_in_delay_prd);
void change_cbr_buffer_delay(cbr_buffer_handle ps_cbr_buffer, WORD32 i4_buffer_delay);
void set_cbr_ebf(cbr_buffer_handle ps_cbr_buffer, WORD32 i32_init_ebf);
LWORD64 get_num_frms_encoded(cbr_buffer_handle ps_cbr_buffer);

LWORD64 get_num_frms_to_encode(cbr_buffer_handle ps_cbr_buffer);

WORD32 get_vbv_buffer_based_excess(
    cbr_buffer_handle ps_cbr_buffer,
    float f_complexity_peak_rate,
    float f_cur_bits_complexity,
    WORD32 bit_alloc_period,
    WORD32 i4_num_gops_for_excess);

rc_type_e get_rc_type(cbr_buffer_handle ps_cbr_buffer);

void cbr_modify_ebf_estimate(cbr_buffer_handle ps_cbr_buffer, WORD32 i4_bit_error);

UWORD32 cbr_get_delay_frames(cbr_buffer_handle ps_cbr_buffer);
#endif /* CBR_BUFFER_CONTROL_H */
