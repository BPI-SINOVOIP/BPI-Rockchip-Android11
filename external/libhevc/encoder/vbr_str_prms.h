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
* \file vbr_str_prms.h
*
* \brief
*    This file contains all the necessary declarations for
*    vbr params functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _VBR_STR_PRMS_H_
#define _VBR_STR_PRMS_H_

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct
{
    UWORD32 u4_num_pics_in_delay_prd[MAX_PIC_TYPE];
    UWORD32 u4_pic_num;
    UWORD32 u4_intra_prd_pos_in_tgt_ticks;
    UWORD32 u4_cur_pos_in_src_ticks;
    UWORD32 u4_intra_frame_int;
    UWORD32 u4_src_ticks;
    UWORD32 u4_tgt_ticks;
    UWORD32 u4_frms_in_delay_prd;
} vbr_str_prms_t;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
void init_vbv_str_prms(
    vbr_str_prms_t *p_vbr_str_prms,
    UWORD32 u4_intra_frm_interval,
    UWORD32 u4_src_ticks,
    UWORD32 u4_tgt_ticks,
    UWORD32 u4_frms_in_delay_period);

void change_vsp_ifi(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_intra_frame_int);
void change_vsp_tgt_ticks(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_tgt_ticks);
void change_vsp_src_ticks(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_src_ticks);
void change_vsp_fidp(vbr_str_prms_t *p_vbr_str_prms, UWORD32 u4_frms_in_delay_period);

#endif
