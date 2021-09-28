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
* \file frame_info_collector.c
*
* \brief
*    This file contain frame info initialize function
*
* \date
*
* \author
*    ittiam
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

/* User include files */
#include "ittiam_datatypes.h"
#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "trace_support.h"
#include "assert.h"

/*
******************************************************************************
* \if Function name : init_frame_info
*
* \brief
*    this function initializes the frame info structs
*
* \param[in]
*            *ps_finfo      -> frame level info
*
* \return
*    status
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void init_frame_info(frame_info_t *ps_frame_info)
{
    ps_frame_info->i8_frame_num = -1;
    ps_frame_info->e_pic_type = BUF_PIC;
    ps_frame_info->f_8bit_q_scale = -1;
    ps_frame_info->f_8bit_q_scale_without_offset = -1;
    ps_frame_info->f_hbd_q_scale = -1;
    ps_frame_info->f_hbd_q_scale_without_offset = -1;
    ps_frame_info->i4_scene_type = -1;
    ps_frame_info->i4_rc_hevc_qp = -1;
    ps_frame_info->i8_cl_sad = -1;
    ps_frame_info->i8_header_bits = -1;
    ps_frame_info->i8_tex_bits = -1;
    ps_frame_info->i4_poc = -1;
    ps_frame_info->i8_L1_ipe_raw_sad = -1;
    ps_frame_info->i8_L1_me_sad = -1;
    ps_frame_info->i4_num_entries = 0;
    ps_frame_info->i8_est_texture_bits = -1;
    ps_frame_info->i4_lap_complexity_q7 = -1;
    ps_frame_info->i4_lap_f_sim = -1;
    ps_frame_info->i4_lap_var = -1;
    ps_frame_info->i8_frame_acc_coarse_me_cost = -1;
    ps_frame_info->i_to_avg_bit_ratio = -1;
    ps_frame_info->i4_num_scd_in_lap_window = -1;
    ps_frame_info->i4_num_frames_b4_scd = -1;
    ps_frame_info->i1_is_complexity_based_bits_reset = -1;
}
