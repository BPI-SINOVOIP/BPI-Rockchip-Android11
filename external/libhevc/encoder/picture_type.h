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
* \file picture_type.h
*
* \brief
*    This file contains all the necessary declarations for
*    picture info handling functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _PIC_HANDLING_H_
#define _PIC_HANDLING_H_

/* Basic Understanding:
 * add_pic_to_stack(_re_enc):
 *  This functions converts the input (or display) order to encoding order
 *
 * */
/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct pic_handling_t *pic_handling_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
WORD32 pic_handling_num_fill_use_free_memtab(
    pic_handling_handle *pps_pic_handling, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);
void init_pic_handling(
    pic_handling_handle ps_pic_handling,
    WORD32 i4_intra_frm_int,
    WORD32 i4_max_inter_frm_int,
    WORD32 i4_is_gop_closed,
    WORD32 i4_idr_period,
    WORD32 i4_num_active_pic_type,
    WORD32 i4_field_pic);

void add_pic_to_stack(
    pic_handling_handle ps_pic_handling, WORD32 i4_enc_pic_id, WORD32 i4_rc_in_pic);
WORD32 add_pic_to_stack_re_enc(
    pic_handling_handle ps_pic_handling, WORD32 i4_enc_pic_id, picture_type_e e_pic_type);

void get_pic_from_stack(
    pic_handling_handle ps_pic_handling,
    WORD32 *pi4_pic_id,
    WORD32 *pi4_pic_disp_order_no,
    picture_type_e *pe_pic_type,
    WORD32 *pi4_is_scd);

WORD32 is_last_frame_in_gop(pic_handling_handle ps_pic_handling);
void flush_frame_from_pic_stack(pic_handling_handle ps_pic_handling);

/* NITT TBR The below two functions should be made a single function */
void skip_encoded_frame(pic_handling_handle ps_pic_handling, picture_type_e e_pic_type);
void update_pic_handling(
    pic_handling_handle ps_pic_handling,
    picture_type_e e_pic_type,
    WORD32 i4_is_non_ref_pic,
    WORD32 i4_is_scd);

/* Function returns the number of frames that have been encoded in the GOP in
 * which the force I frame takes impact */
WORD32 pic_type_get_frms_in_gop_force_I_frm(pic_handling_handle ps_pic_handling);
void set_force_I_frame_flag(pic_handling_handle ps_pic_handling);
WORD32 get_forced_I_frame_cur_frm_flag(pic_handling_handle ps_pic_handling);
void reset_forced_I_frame_cur_frm_flag(pic_handling_handle ps_pic_handling);

/* Normal get functions */
WORD32 pic_type_get_inter_frame_interval(pic_handling_handle ps_pic_handling);

WORD32 pic_type_get_intra_frame_interval(pic_handling_handle ps_pic_handling);

WORD32 pic_type_get_disp_order_no(pic_handling_handle ps_pic_handling);

WORD32 pic_type_get_field_pic(pic_handling_handle ps_pic_handling);

WORD32 pic_type_is_gop_closed(pic_handling_handle ps_pic_handling);

void pic_handling_register_new_int_frm_interval(
    pic_handling_handle ps_pic_handling, WORD32 i4_intra_frm_int);

void pic_handling_register_new_inter_frm_interval(
    pic_handling_handle ps_pic_handling, WORD32 i4_inter_frm_int);

WORD32 pic_type_get_rem_frms_in_gop(pic_handling_handle ps_pic_handling);

void pic_type_get_frms_in_gop(
    pic_handling_handle ps_pic_handling, WORD32 ai4_frms_in_gop[MAX_PIC_TYPE]);

void pic_type_get_actual_frms_in_gop(
    pic_handling_handle ps_pic_handling, WORD32 ai4_frms_in_gop[MAX_PIC_TYPE]);

void pic_type_update_frms_in_gop(
    pic_handling_handle ps_pic_handling, WORD32 ai4_frms_in_gop[MAX_PIC_TYPE]);

WORD32 get_default_intra_period(pic_handling_handle ps_pic_handling);

WORD32 pic_type_get_actual_intra_frame_interval(pic_handling_handle ps_pic_handling);
#endif /* _PIC_HANDLING_H_ */
