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
* \file bit_allocation.c
*
* \brief
*    This file contain bit processing functions
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
#include <assert.h>
#include <stdarg.h>
#include <math.h>

/* User include files */
#include "ittiam_datatypes.h"
#include "mem_req_and_acq.h"
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "fixed_point_error_bits.h"
#include "cbr_buffer_control.h"
#include "rc_rd_model.h"
#include "est_sad.h"
#include "cbr_buffer_control.h"
#include "picture_type.h"
#include "bit_allocation.h"
#include "trace_support.h"
#include "rc_frame_info_collector.h"
#include "rate_control_api.h"

/** Macros **/
#define MIN(x, y) ((x) < (y)) ? (x) : (y)
#define MAX(x, y) ((x) < (y)) ? (y) : (x)

/* State structure for bit allocation */
typedef struct
{
    WORD32 i4_rem_bits_in_period;
    /* Storing inputs */
    WORD32 i4_tot_frms_in_gop;
    WORD32 i4_num_intra_frm_interval;
    WORD32 i4_bits_per_frm;
} rem_bit_in_prd_t;

typedef struct bit_allocation_t
{
    rem_bit_in_prd_t s_rbip;
    WORD32
        i2_K[MAX_PIC_TYPE]; /* A universal constant giving the relative complexity between pictures */
    WORD32 i4_prev_frm_header_bits[MAX_PIC_TYPE]; /* To get a estimate of the header bits consumed */
    WORD32 ai4_prev_frm_tot_bits[MAX_PIC_TYPE];
    WORD32 ai4_prev_frm_tot_est_bits[MAX_PIC_TYPE];
    WORD32 i4_bits_per_frm;
    WORD32 i4_num_gops_in_period;
    WORD32
    i4_actual_num_gops_in_period; /* Num gops as set by rate control module */
    WORD32 i4_saved_bits;
    WORD32 i4_max_bits_per_frm[MAX_NUM_DRAIN_RATES];
    WORD32 i4_min_bits_per_frm;
    /* Error bits module */
    error_bits_handle ps_error_bits;
    /* Storing frame rate */
    WORD32 i4_frame_rate;
    WORD32 i4_bit_rate;
    WORD32 ai4_peak_bit_rate[MAX_NUM_DRAIN_RATES];
    WORD32 i4_max_tex_bits_for_i;
    WORD32 i4_pels_in_frame;
    /* Errors within GOP and across GOP */
    WORD32 i4_gop_level_bit_error;
    WORD32 i4_frame_level_bit_error;
    WORD32 ai4_cur_frm_est_tex_bits[MAX_NUM_FRAME_PARALLEL];
    WORD32 ai4_cur_frm_est_hdr_bits[MAX_NUM_FRAME_PARALLEL];
    WORD32 i4_buffer_based_bit_error;
    WORD32 i4_bits_from_buffer_in_cur_gop;
    WORD32 i4_excess_bits_from_buffer;

    WORD32 i4_is_hbr;
    WORD32 i4_rem_frame_in_period;
    /*HEVC_RC : this will be updated by rc_interface.c to have number of SCD in lap window.
        Ni will be incremented using this to bring down buffer level and also back to back scd within lap window*/
    WORD32 i4_num_scd_in_lap_window;
    WORD32 i4_num_frm_b4_scd;
    WORD32 i4_num_active_pic_type;
    WORD32 i4_lap_window;
    WORD32 i4_field_pic;
    WORD32 i4_ba_rc_pass;
    void *pv_gop_stat;
    LWORD64 i8_cur_gop_num;
    LWORD64
    i8_frm_num_in_gop; /*TBD: For enc loop parallel this variable needs to maintained outside rate control since qp will not be queried in actual bitstream order*/
    float af_sum_weigh[MAX_PIC_TYPE][3];
    LWORD64 i8_cur_gop_bit_consumption; /*To calculate the deviaiton in 2 pass*/
    //LWORD64 i8_2pass_gop_error_accum;
    LWORD64
    i8_2pass_alloc_per_frm_bits; /*Per frame bits allocated to GOP in 2 pass*/
    //LWORD64 i8_2pass_alloc_per_frm_bits_next_gop;

    float f_min_complexity_cross_peak_rate; /*complexity of gop beyond which it is clipped to peak rate in 2ns pass*/
    WORD32 i4_next_sc_i_in_rc_look_ahead;
    /*The peak factor is multiplied to get the total bits for a gop based on squashing function*/
    float f_cur_peak_factor_2pass;
    LWORD64 i8_total_bits_allocated;
    WORD32 i4_luma_pels;
    WORD32 i4_num_gop;
    /*The bitrate will keep changing in 2 pass due to error*/
    LWORD64 i8_current_bitrate_2_pass;
    /*i4_flag_no_more_set_rbip - once we have reached the end of total number of frames to be encoded in
    2nd pass sliding window bit allocation there is no need to set rbip again*/
    WORD32 i4_flag_no_more_set_rbip;
    /*i8_vbv_based_excess_for_segment will be distributed across the complex segments depending on the
    ratio of current complexity to f_sum_complexity_segment_cross_peak*/
    float f_sum_complexity_segment_cross_peak;
    /*(i8_vbv_based_excess_for_segment)Buffer play excess is calculated for the entire segment of complex
    content which may consist of multiple gop's*/
    //LWORD64 i8_vbv_based_excess_for_segment;
    /*I frame bit allocation during scene cuts is dependent on f_curr_i_to_sum which will signal
    the complexity difference between current i to future i's if present in the same default gop*/
    float f_curr_i_to_sum;
    float f_curr_by_sum_subgop;
    WORD32 ai4_pic_types_in_subgop[MAX_PIC_TYPE];
    WORD32 i4_use_subgop_bit_alloc_flag;
    WORD32 i4_num_frames_since_last_I_frame;
    LWORD64 i8_first_pic_bits_pictype[MAX_PIC_TYPE];
    LWORD64 i8_avg_bits_pictype[MAX_PIC_TYPE];
    WORD32 i4_avg_qscale_gop_first_pass;
    WORD32 i4_fp_bit_alloc_in_sp;
    WORD32 i4_frame_level_error_ctr_update_rc;
    float f_qscale_max_clip_in_second_pass;
    float f_average_qscale_1st_pass;
    float f_max_average_qscale_1st_pass;
    LWORD64 i8_bit_consumption_so_far;
    WORD32 i4_total_2pass_frames;
    LWORD64 i8_2pass_avg_bit_rate;
    WORD32 i4_br_id;
} bit_allocation_t;

static WORD32 get_actual_num_frames_in_gop(pic_handling_handle ps_pic_handling)
{
    WORD32 i4_tot_frms_in_gop = 0, i;
    WORD32 ai4_actual_frms_in_gop[MAX_PIC_TYPE];
    memset(ai4_actual_frms_in_gop, 0, MAX_PIC_TYPE * sizeof(WORD32));
    pic_type_get_actual_frms_in_gop(ps_pic_handling, ai4_actual_frms_in_gop);
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        i4_tot_frms_in_gop += ai4_actual_frms_in_gop[i];
    }
    return (i4_tot_frms_in_gop);
}

float get_cur_peak_factor_2pass(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_cur_peak_factor_2pass);
}
float get_cur_min_complexity_factor_2pass(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_min_complexity_cross_peak_rate);
}
void set_2pass_total_gops(bit_allocation_t *ps_bit_allocation, WORD32 i4_num_gop)
{
    ps_bit_allocation->i4_num_gop = i4_num_gop;
}
#if NON_STEADSTATE_CODE
/* Module for accessing remaining bits in period */
/*****************************************************************************
  Function Name : init_rbip
  Description   : Initalises the remaining bits in period structure.
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
                  num_intra_frm_interval - num of I frm intervals in this period
                  i4_bits_per_frm - num bits per frm
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static void init_rbip(
    rem_bit_in_prd_t *ps_rbip,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_bits_per_frm,
    WORD32 i4_num_intra_frm_interval)
{
    WORD32 i4_tot_frms_in_gop = get_actual_num_frames_in_gop(ps_pic_handling);
    /* WORD32 i4_frm_correction_for_open_gop = 0; */
    /* If the GOP is open, then we need to subtract the num_b_frames for the first gop */
    /*if(!pic_type_is_gop_closed(ps_pic_handling))
    {
        i4_frm_correction_for_open_gop = pic_type_get_inter_frame_interval(ps_pic_handling)-1;
    }*/
    ps_rbip->i4_rem_bits_in_period =
        i4_bits_per_frm *
        (i4_tot_frms_in_gop * i4_num_intra_frm_interval /*- i4_frm_correction_for_open_gop*/);

    /* Store the total number of frames in GOP value which is
     * used from module A */
    ps_rbip->i4_tot_frms_in_gop = i4_tot_frms_in_gop;
    ps_rbip->i4_num_intra_frm_interval = i4_num_intra_frm_interval;
    ps_rbip->i4_bits_per_frm = i4_bits_per_frm;
}
#endif /* #if NON_STEADSTATE_CODE */

/*****************************************************************************
  Function Name : check_update_rbip
  Description   : Function for updating rbip.
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static void check_update_rbip(rem_bit_in_prd_t *ps_rbip, pic_handling_handle ps_pic_handling)
{
    /* NOTE: Intra frame interval changes aafter the first I frame that is encoded in a GOP */
    WORD32 i4_new_tot_frms_in_gop = get_actual_num_frames_in_gop(ps_pic_handling);
    if(i4_new_tot_frms_in_gop != ps_rbip->i4_tot_frms_in_gop)
    {
        WORD32 i4_num_frames_in_period = ps_rbip->i4_num_intra_frm_interval *
                                         (i4_new_tot_frms_in_gop - ps_rbip->i4_tot_frms_in_gop);
        overflow_avoided_summation(
            &ps_rbip->i4_rem_bits_in_period, (ps_rbip->i4_bits_per_frm * i4_num_frames_in_period));
    }
    /* Updated the new values */
    ps_rbip->i4_tot_frms_in_gop = i4_new_tot_frms_in_gop;
}
/*****************************************************************************
  Function Name : ret_rbip_default_preenc
  Description   : Function for calculating bits in period.
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static WORD32
    ret_rbip_default_preenc(rem_bit_in_prd_t *ps_rbip, pic_handling_handle ps_pic_handling)
{
    WORD32 i4_bits_in_period =
        pic_type_get_intra_frame_interval(ps_pic_handling) * ps_rbip->i4_bits_per_frm;
    return (i4_bits_in_period);
}
/*****************************************************************************
  Function Name : update_rbip
  Description   : Function for updating rbip.
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static WORD32 update_rbip(
    rem_bit_in_prd_t *ps_rbip, pic_handling_handle ps_pic_handling, WORD32 i4_num_of_bits)
{
    check_update_rbip(ps_rbip, ps_pic_handling);
    overflow_avoided_summation(&ps_rbip->i4_rem_bits_in_period, i4_num_of_bits);
    return (ps_rbip->i4_rem_bits_in_period);
}
/*****************************************************************************
  Function Name : get_rbip_and_num_frames
  Description   : Update rbip and get number of frames.
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
                  pi4_num_frames  - pointer to update number of frmes
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static LWORD64 get_rbip_and_num_frames(
    rem_bit_in_prd_t *ps_rbip,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_num_of_bits,
    WORD32 *pi4_num_frames)
{
    check_update_rbip(ps_rbip, ps_pic_handling);
    overflow_avoided_summation(&ps_rbip->i4_rem_bits_in_period, i4_num_of_bits);
    *pi4_num_frames = ps_rbip->i4_tot_frms_in_gop;
    return (ps_rbip->i4_rem_bits_in_period);
}
/*****************************************************************************
  Function Name : set_rbip
  Description   : Set rbip
  Inputs        : ps_rbip         -remaining bits in period structure
                  i4_error_bits   - Error bits
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static WORD32 set_rbip(rem_bit_in_prd_t *ps_rbip, WORD32 i4_error_bits)
{
    ps_rbip->i4_rem_bits_in_period = (ps_rbip->i4_bits_per_frm * ps_rbip->i4_tot_frms_in_gop *
                                      ps_rbip->i4_num_intra_frm_interval) +
                                     i4_error_bits;

    return ps_rbip->i4_rem_bits_in_period;
}

/*****************************************************************************
  Function Name : multi_pass_set_rbip
  Description   : 2 pass set RBIP, since the gop bits shall not depend on bitrate or framerate,
                  GOP bits is directly obtained from first pass data
  Inputs        : ps_rbip         -remaining bits in period structure
                  ps_pic_handling - Pic handling structure
                  i4_cur_gop_bits - bits allocated for the curr gop
                  i4_tot_frm_in_cur_gop - frames in the gop
Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static void multi_pass_set_rbip(
    rem_bit_in_prd_t *ps_rbip,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_cur_gop_bits,
    WORD32 i4_tot_frm_in_cur_gop)
{
    WORD32 i4_num_frames_in_gop = get_actual_num_frames_in_gop(ps_pic_handling);
    ps_rbip->i4_rem_bits_in_period =
        (WORD32)((LWORD64)i4_cur_gop_bits * i4_num_frames_in_gop / i4_tot_frm_in_cur_gop);
    ps_rbip->i4_tot_frms_in_gop = i4_num_frames_in_gop;
    ps_rbip->i4_bits_per_frm = ps_rbip->i4_rem_bits_in_period / i4_num_frames_in_gop;
}
static void change_rbip(
    rem_bit_in_prd_t *ps_rbip, WORD32 i4_new_bits_per_frm, WORD32 i4_new_num_intra_frm_interval)
{
    if(i4_new_bits_per_frm != ps_rbip->i4_bits_per_frm)
    {
        WORD32 i4_rem_frms_in_period =
            (ps_rbip->i4_num_intra_frm_interval) * ps_rbip->i4_tot_frms_in_gop;
        overflow_avoided_summation(
            &ps_rbip->i4_rem_bits_in_period,
            ((i4_new_bits_per_frm - ps_rbip->i4_bits_per_frm) * i4_rem_frms_in_period));
    }
    if(i4_new_num_intra_frm_interval != ps_rbip->i4_num_intra_frm_interval)
    {
        overflow_avoided_summation(
            &ps_rbip->i4_rem_bits_in_period,
            (i4_new_bits_per_frm * ps_rbip->i4_tot_frms_in_gop *
             (i4_new_num_intra_frm_interval - ps_rbip->i4_num_intra_frm_interval)));
    }
    /* Update the new value */
    ps_rbip->i4_num_intra_frm_interval = i4_new_num_intra_frm_interval;
    ps_rbip->i4_bits_per_frm = i4_new_bits_per_frm;
}

#if NON_STEADSTATE_CODE
WORD32 bit_allocation_num_fill_use_free_memtab(
    bit_allocation_t **pps_bit_allocation, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static bit_allocation_t s_bit_allocation_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_bit_allocation) = &s_bit_allocation_temp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx],
            sizeof(bit_allocation_t),
            MEM_TAB_ALIGNMENT,
            PERSISTENT,
            DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_bit_allocation, e_func_type);
    }
    i4_mem_tab_idx++;

    i4_mem_tab_idx += error_bits_num_fill_use_free_memtab(
        &pps_bit_allocation[0]->ps_error_bits, &ps_memtab[i4_mem_tab_idx], e_func_type);

    return (i4_mem_tab_idx);
}
#endif /* #if NON_STEADSTATE_CODE */

/*****************************************************************************
  Function Name : get_bits_based_on_complexity
  Description   : function calculates the bits to be allocated for the current
                  picture type given the relative complexity between different
                  picture types
  Inputs        : i4_bits_in_period
                  pi4_frms_in_period      - num frames of each pictype in the period
                  pvq_complexity_estimate - complexity for each pictype
                  e_pic_type              - current picture type
                  i4_call_type
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
static WORD32 get_bits_based_on_complexity(
    bit_allocation_t *ps_bit_allocation,
    WORD32 i4_bits_in_period,
    WORD32 *pi4_frms_in_period,
    number_t *pvq_complexity_estimate,
    picture_type_e e_pic_type,
    WORD32 i4_call_type)
{
    WORD32 i, i4_estimated_bits;
    number_t vq_bits_in_period, vq_frms_in_period[MAX_PIC_TYPE], vq_comp_coeff,
        vq_est_texture_bits_for_frm;
    WORD32 i4_num_scd_in_LAP_window = ps_bit_allocation->i4_num_scd_in_lap_window;
    WORD32 i4_active_pic_types = ps_bit_allocation->i4_num_active_pic_type,
           i4_field_pic = ps_bit_allocation->i4_field_pic;
    float af_sum_weigh[MAX_PIC_TYPE][3];

    memmove(af_sum_weigh, ps_bit_allocation->af_sum_weigh, ((sizeof(float)) * MAX_PIC_TYPE * 3));

    /** Increment I frame count if there is scene cut in LAP window*/
    i4_num_scd_in_LAP_window = 0;
    pi4_frms_in_period[I_PIC] += i4_num_scd_in_LAP_window;
    /* Converting inputs to var_q format */
    SET_VAR_Q(vq_bits_in_period, i4_bits_in_period, 0);
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        SET_VAR_Q(vq_frms_in_period[i], pi4_frms_in_period[i], 0);
    }
    /******************************************************************
    Estimated texture bits =
            (remaining bits) * (cur frm complexity)
            ---------------------------------------
    (num_i_frm*i_frm_complexity) + (num_p_frm*pfrm_complexity) + (b_frm * b_frm_cm)
    *******************************************************************/
    /*Taking the numerator weight into account*/
    if(i4_call_type == 1)
    {
        trace_printf("1 CUrr / avg %f", af_sum_weigh[e_pic_type][0]);
    }
    if(af_sum_weigh[e_pic_type][0] > 4.0f)
        af_sum_weigh[e_pic_type][0] = 4.0f;
    if(af_sum_weigh[e_pic_type][0] < 0.3f)
        af_sum_weigh[e_pic_type][0] = 0.3f;
    if(i4_call_type == 1)
    {
        trace_printf("2 CUrr / avg %f", af_sum_weigh[e_pic_type][0]);
    }

    if((ps_bit_allocation->i4_ba_rc_pass != 2) || (i4_call_type == 0) ||
       (ps_bit_allocation->i4_fp_bit_alloc_in_sp == 0))
    {
        convert_float_to_fix(af_sum_weigh[e_pic_type][0], &vq_comp_coeff);
        mult32_var_q(vq_bits_in_period, vq_comp_coeff, &vq_bits_in_period);
        mult32_var_q(vq_bits_in_period, pvq_complexity_estimate[e_pic_type], &vq_bits_in_period);
    }
    else
    {
        WORD32 i4_frame_num = (WORD32)ps_bit_allocation->i8_frm_num_in_gop, i4_offset;
        gop_level_stat_t *ps_gop;
        LWORD64 i8_firs_pass_tot_bits;
        float f_bits_cur_pic, f_offset;
        ps_gop =
            (gop_level_stat_t *)ps_bit_allocation->pv_gop_stat + ps_bit_allocation->i8_cur_gop_num;
        i8_firs_pass_tot_bits = ps_gop->ai8_tex_bits_consumed[i4_frame_num] +
                                ps_gop->ai8_head_bits_consumed[i4_frame_num];
        i4_offset = (ps_gop->ai4_q6_frame_offsets[i4_frame_num] * 1000) >> QSCALE_Q_FAC;
        f_offset = ((float)i4_offset) / 1000;
        f_bits_cur_pic =
            (float)(i8_firs_pass_tot_bits * ps_gop->ai4_first_pass_qscale[i4_frame_num]) /
            (ps_bit_allocation->i4_avg_qscale_gop_first_pass * f_offset);
        convert_float_to_fix(f_bits_cur_pic, &vq_comp_coeff);
        mult32_var_q(vq_bits_in_period, vq_comp_coeff, &vq_bits_in_period);

        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            number_t temp;
            convert_float_to_fix((float)ps_bit_allocation->i8_avg_bits_pictype[i], &temp);
            pvq_complexity_estimate[i] = temp;
        }
    }

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        /*For 2nd pass we will be reducing the num of pictures as they are coded so we dont want to replace 0's*/
        if(af_sum_weigh[i][1] == 0.0 &&
           !((i4_call_type == 1) && (ps_bit_allocation->i4_ba_rc_pass == 2)))
            af_sum_weigh[i][1] = (float)pi4_frms_in_period[i];

        convert_float_to_fix(af_sum_weigh[i][1], &vq_comp_coeff);
        mult32_var_q(vq_comp_coeff, pvq_complexity_estimate[i], &vq_frms_in_period[i]);
    }

    /* changed the index range from active_pic to max_pic*/
    if(i4_field_pic)
    {
        for(i = 1; i < i4_active_pic_types; i++)
        {
            add32_var_q(vq_frms_in_period[I_PIC], vq_frms_in_period[i], &vq_frms_in_period[I_PIC]);
            add32_var_q(
                vq_frms_in_period[I_PIC],
                vq_frms_in_period[i + FIELD_OFFSET],
                &vq_frms_in_period[I_PIC]);
        }
    }
    else /*field case*/
    {
        for(i = 1; i < i4_active_pic_types; i++)
        {
            add32_var_q(vq_frms_in_period[I_PIC], vq_frms_in_period[i], &vq_frms_in_period[I_PIC]);
        }
    }

    div32_var_q(vq_bits_in_period, vq_frms_in_period[I_PIC], &vq_est_texture_bits_for_frm);
    number_t_to_word32(vq_est_texture_bits_for_frm, &i4_estimated_bits);

    /* If the number of frames are zero then return zero */
    if(!pi4_frms_in_period[e_pic_type])
        i4_estimated_bits = 0;
    return (i4_estimated_bits);
}
/*****************************************************************************
  Function Name : assign_complexity_coeffs
  Description   :
  Inputs        : af_sum_weigh
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void assign_complexity_coeffs(
    bit_allocation_t *ps_bit_allocation, float af_sum_weigh[MAX_PIC_TYPE][3])
{
    WORD32 i;
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_bit_allocation->af_sum_weigh[i][0] = af_sum_weigh[i][0];
        ps_bit_allocation->af_sum_weigh[i][1] = af_sum_weigh[i][1];
        ps_bit_allocation->af_sum_weigh[i][2] = af_sum_weigh[i][2];
    }
}
/*****************************************************************************
  Function Name : ba_get_rbip_and_num_frames
  Description   :
  Inputs        : pi4_num_frames
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 ba_get_rbip_and_num_frames(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 *pi4_num_frames)
{
    return (
        get_rbip_and_num_frames(&ps_bit_allocation->s_rbip, ps_pic_handling, 0, pi4_num_frames));
}
/*****************************************************************************
  Function Name : init_prev_header_bits
  Description   : Intialise header bits for each pic type
  Inputs        : ps_bit_allocation
                  ps_pic_handling
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void init_prev_header_bits(bit_allocation_t *ps_bit_allocation, pic_handling_handle ps_pic_handling)
{
    WORD32 i4_rem_bits_in_period, /* ai4_rem_frms_in_period[MAX_PIC_TYPE], */
        ai4_frms_in_period[MAX_PIC_TYPE], i, j;
    number_t avq_complexity_estimate[MAX_PIC_TYPE];
    WORD32 i4_field_pic;
    i4_field_pic = pic_type_get_field_pic(ps_pic_handling);
    /* Assigning Percentages of I, P and B frame header bits based on actual bits allocated for I, P and B frames */
    /* Getting the remaining bits in period */
    i4_rem_bits_in_period = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);

    /* Hardcoding the bit ratios between I, P and B */
    SET_VAR_Q(
        avq_complexity_estimate[I_PIC],
        (I_TO_P_BIT_RATIO * P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
        0);
    SET_VAR_Q(
        avq_complexity_estimate[P_PIC],
        (P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
        0);
    SET_VAR_Q(
        avq_complexity_estimate[P1_PIC],
        (P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
        0);
    SET_VAR_Q(avq_complexity_estimate[B_PIC], (B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO), 0);
    SET_VAR_Q(avq_complexity_estimate[BB_PIC], (B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO), 0);
    SET_VAR_Q(avq_complexity_estimate[B1_PIC], (B1_TO_B2_BIT_RATIO), 0);
    SET_VAR_Q(
        avq_complexity_estimate[B11_PIC],
        (B1_TO_B2_BIT_RATIO),
        0);  //temporarliy treat ref and non ref as same
    SET_VAR_Q(avq_complexity_estimate[B2_PIC], 1, 0);
    SET_VAR_Q(avq_complexity_estimate[B22_PIC], 1, 0);

    /* Get the rem_frms_in_gop & the frms_in_gop from the pic_type state struct */
    /* pic_type_get_rem_frms_in_gop(ps_pic_handling, ai4_rem_frms_in_period); */
    pic_type_get_frms_in_gop(ps_pic_handling, ai4_frms_in_period);

    /* Depending on the number of gops in a period, find the num_frms_in_prd */
    for(j = 0; j < MAX_PIC_TYPE; j++)
    {
        ai4_frms_in_period[j] = (ai4_frms_in_period[j] * ps_bit_allocation->i4_num_gops_in_period);
    }

    /* Percentage of header bits in teh overall bits allocated to I, P and B frames
    when the data is not known. Since this value is based on bitrate using a equation
    to fit the values. Ran the header bit ratio for [1080@30] carnival, ihits and
    football at 9, 12 and 16 mbps and based on that deriving a equation using bpp.
    Values obtained are:
    (bitrate/bpp)   I              P              B
    9/2.87          0.358617291155 0.549124350786 0.798772545232
    12/3.83         0.288633642796 0.444797334749 0.693933711801
    16/5.11         0.284241839623 0.330152764298 0.557999732549
    Equation for I:
        if bpp > 3.83 hdr = 0.29
        else hdr = -0.073*bpp + 0.569
    Equation for P: hdr = -0.098*bpp + 0.825
    Equation for B: hdr = -0.108*bpp + 1.108
    */
    {
#define FRAME_HEADER_BITS_Q_FACTOR (10)
        WORD32 ai4_header_bits_percentage[MAX_PIC_TYPE];

        WORD32 i4_bpp;
        X_PROD_Y_DIV_Z(
            ps_bit_allocation->i4_bits_per_frm,
            (1 << FRAME_HEADER_BITS_Q_FACTOR),
            ps_bit_allocation->i4_pels_in_frame,
            i4_bpp);
        //ps_bit_allocation->i4_bits_per_frm*(1<<FRAME_HEADER_BITS_Q_FACTOR)/ps_bit_allocation->i4_pels_in_frame;

        if(i4_bpp > 131)
            ai4_header_bits_percentage[I_PIC] = 297;
        else
            ai4_header_bits_percentage[I_PIC] =
                ((-2238 * i4_bpp) >> FRAME_HEADER_BITS_Q_FACTOR) + 583;
        ai4_header_bits_percentage[P_PIC] = ((-2990 * i4_bpp) >> FRAME_HEADER_BITS_Q_FACTOR) + 845;

        ai4_header_bits_percentage[B_PIC] = ((-3308 * i4_bpp) >> FRAME_HEADER_BITS_Q_FACTOR) + 1135;

        /* Changes for 2B subGOP */
        ai4_header_bits_percentage[P_PIC] = (ai4_header_bits_percentage[P_PIC] * 13) >> 4;
        ai4_header_bits_percentage[P1_PIC] = (ai4_header_bits_percentage[P_PIC] * 13) >> 4;
        ai4_header_bits_percentage[B_PIC] = (ai4_header_bits_percentage[B_PIC] * 12) >> 4;
        ai4_header_bits_percentage[BB_PIC] = (ai4_header_bits_percentage[B_PIC] * 12) >> 4;
        /*HEVC_hierarchy: temp change consider same percentage because of insufficient data*/
        ai4_header_bits_percentage[B1_PIC] = ai4_header_bits_percentage[B_PIC];
        ai4_header_bits_percentage[B11_PIC] = ai4_header_bits_percentage[B_PIC];
        ai4_header_bits_percentage[B2_PIC] = ai4_header_bits_percentage[B_PIC];
        ai4_header_bits_percentage[B22_PIC] = ai4_header_bits_percentage[B_PIC];

        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_bit_allocation->af_sum_weigh[i][0] = 1.0;
            ps_bit_allocation->af_sum_weigh[i][1] = 0.0;
            ps_bit_allocation->af_sum_weigh[i][2] = 0.0;
        }

        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            /* Getting the total bits allocated for each picture type */
            WORD32 i4_num_bits_allocated = get_bits_based_on_complexity(
                ps_bit_allocation,
                i4_rem_bits_in_period,
                ai4_frms_in_period,
                avq_complexity_estimate,
                (picture_type_e)i,
                0);

            if(ai4_header_bits_percentage[i] < 0)
                ai4_header_bits_percentage[i] = 0;

            ps_bit_allocation->i4_prev_frm_header_bits[i] = (WORD32)(
                ((LWORD64)ai4_header_bits_percentage[i] * i4_num_bits_allocated) >>
                FRAME_HEADER_BITS_Q_FACTOR);
        }
    }
}

/*****************************************************************************
  Function Name : init_bit_allocation
  Description   : Initalises the bit_allocation structure.
  Inputs        : intra_frm_interval - num frames between two I frames
                  num_intra_frm_interval - num such intervals
                  i4_bit_rate - num bits per second
                  i4_frm_rate - num frms in 1000 seconds
                  i4_num_active_pic_type
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
#if NON_STEADSTATE_CODE
void init_bit_allocation(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_num_intra_frm_interval, /* num such intervals */
    WORD32 i4_bit_rate, /* num bits per second */
    WORD32 i4_frm_rate, /* num frms in 1000 seconds */
    WORD32 *i4_peak_bit_rate,
    WORD32 i4_min_bitrate, /* The minimum bit rate that is to be satisfied for a gop */
    WORD32 i4_pels_in_frame,
    WORD32 i4_is_hbr,
    WORD32 i4_num_active_pic_type,
    WORD32 i4_lap_window,
    WORD32 i4_field_pic,
    WORD32 rc_pass,
    WORD32 i4_luma_pels,
    WORD32 i4_fp_bit_alloc_in_sp)
{
    WORD32 i;
    WORD32 i4_bits_per_frm, i4_max_bits_per_frm[MAX_NUM_DRAIN_RATES];
    /* Store the pels in frame value */
    ps_bit_allocation->i4_pels_in_frame = i4_pels_in_frame;
    ps_bit_allocation->i4_num_scd_in_lap_window = 0;
    ps_bit_allocation->i4_num_frm_b4_scd = 0;
    ps_bit_allocation->i4_num_active_pic_type = i4_num_active_pic_type;
    ps_bit_allocation->i4_field_pic = i4_field_pic;
    ps_bit_allocation->i4_ba_rc_pass = rc_pass;
    ps_bit_allocation->i4_br_id = 0; /* 0 - peak, 1 - average*/
    ps_bit_allocation->i8_cur_gop_num =
        0; /*Will be incremented after first frame allocation is done(during init itslef)*/
    ps_bit_allocation->i8_frm_num_in_gop = 0;
    ps_bit_allocation->pv_gop_stat =
        NULL; /*In 2 pass the gop stat pointer is set API parameter call*/
    ps_bit_allocation->f_min_complexity_cross_peak_rate =
        1.0; /*In single pass buffer based additional bits movement is disabled, hence set to max complexity
                                                                Reset to lower value in case of two pass*/

    ps_bit_allocation->f_cur_peak_factor_2pass = -1.0;
    ps_bit_allocation->i8_total_bits_allocated = -1;
    ps_bit_allocation->i4_luma_pels = i4_luma_pels;
    ps_bit_allocation->i4_num_gop = -1;
    ps_bit_allocation->f_sum_complexity_segment_cross_peak = 0.0f;
    //ps_bit_allocation->i8_vbv_based_excess_for_segment = 0;
    ps_bit_allocation->i4_flag_no_more_set_rbip = 0;
    ps_bit_allocation->f_curr_i_to_sum = 1.0f;
    ps_bit_allocation->i4_fp_bit_alloc_in_sp = i4_fp_bit_alloc_in_sp;

    /* Calculate the bits per frame */
    X_PROD_Y_DIV_Z(i4_bit_rate, 1000, i4_frm_rate, i4_bits_per_frm);
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(i4_peak_bit_rate[i], 1000, i4_frm_rate, i4_max_bits_per_frm[i]);
    }
    /* Initialize the bits_per_frame */
    ps_bit_allocation->i4_bits_per_frm = i4_bits_per_frm;
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        ps_bit_allocation->i4_max_bits_per_frm[i] = i4_max_bits_per_frm[i];
    }
    X_PROD_Y_DIV_Z(i4_min_bitrate, 1000, i4_frm_rate, ps_bit_allocation->i4_min_bits_per_frm);

    /* Initialise the rem_bits in period
       The first gop in case of an OPEN GOP may have fewer B_PICs,
       That condition is not taken care of */
    init_rbip(
        &ps_bit_allocation->s_rbip, ps_pic_handling, i4_bits_per_frm, i4_num_intra_frm_interval);

    /* Initialize the num_gops_in_period */
    ps_bit_allocation->i4_num_gops_in_period = i4_num_intra_frm_interval;
    ps_bit_allocation->i4_actual_num_gops_in_period = i4_num_intra_frm_interval;

    /* Relative complexity between I and P frames */
    ps_bit_allocation->i2_K[I_PIC] = (1 << K_Q);
    ps_bit_allocation->i2_K[P_PIC] = I_TO_P_RATIO;
    ps_bit_allocation->i2_K[P1_PIC] = I_TO_P_RATIO;
    ps_bit_allocation->i2_K[B_PIC] = (P_TO_B_RATIO * I_TO_P_RATIO) >> K_Q;
    ps_bit_allocation->i2_K[BB_PIC] = (P_TO_B_RATIO * I_TO_P_RATIO) >> K_Q;

    /*HEVC_hierarchy: force qp offset with one high level of hierarchy*/
    ps_bit_allocation->i2_K[B1_PIC] = (B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q);
    ps_bit_allocation->i2_K[B11_PIC] = (B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q);
    ps_bit_allocation->i2_K[B2_PIC] =
        (B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q + K_Q);
    ps_bit_allocation->i2_K[B22_PIC] =
        (B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q + K_Q);

    /* Initialize the saved bits to 0*/
    ps_bit_allocation->i4_saved_bits = 0;

    /* Update the error bits module with average bits */
    init_error_bits(ps_bit_allocation->ps_error_bits, i4_frm_rate, i4_bit_rate);
    /* Store the input for implementing change in values */
    ps_bit_allocation->i4_frame_rate = i4_frm_rate;
    ps_bit_allocation->i4_bit_rate = i4_bit_rate;
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        ps_bit_allocation->ai4_peak_bit_rate[i] = i4_peak_bit_rate[i];

    ps_bit_allocation->i4_is_hbr = i4_is_hbr;
    /* Initilising the header bits to be used for each picture type */
    init_prev_header_bits(ps_bit_allocation, ps_pic_handling);

    /*HEVC_RC*/
    /*remember prev frames tot bit consumption. This is required to calcualte error after first sub gop where estimate is not known*/
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_bit_allocation->ai4_prev_frm_tot_bits[i] =
            -1;  //-1 indicates that pic type has not been encoded
        ps_bit_allocation->ai4_prev_frm_tot_est_bits[i] = -1;
    }

    /* #define STATIC_I_TO_P_RATIO ((STATIC_I_TO_B_RATIO)/(STATIC_P_TO_B_RATIO)) */
    /* Calcualte the max i frame bits */
    {
        WORD32 ai4_frms_in_period[MAX_PIC_TYPE];
        WORD32 ai4_actual_frms_in_period[MAX_PIC_TYPE], i4_actual_frms_in_period = 0;
        WORD32 i4_rem_texture_bits, j, i4_tot_header_bits_est = 0;
        number_t avq_complexity_estimate[MAX_PIC_TYPE];
        WORD32 i4_total_frms;

        /* Get the rem_frms_in_gop & the frms_in_gop from the pic_type state struct */
        pic_type_get_frms_in_gop(ps_pic_handling, ai4_frms_in_period);
        pic_type_get_actual_frms_in_gop(ps_pic_handling, ai4_actual_frms_in_period);
        /* Depending on the number of gops in a period, find the num_frms_in_prd */
        i4_total_frms = 0;
        for(j = 0; j < MAX_PIC_TYPE; j++)
        {
            ai4_frms_in_period[j] *= ps_bit_allocation->i4_num_gops_in_period;
            ai4_actual_frms_in_period[j] *= ps_bit_allocation->i4_num_gops_in_period;
            i4_total_frms += ai4_frms_in_period[j];
            i4_actual_frms_in_period += ai4_actual_frms_in_period[j];
        }
        ps_bit_allocation->i4_rem_frame_in_period = i4_actual_frms_in_period; /*i_only*/

        for(j = 0; j < MAX_PIC_TYPE; j++)
        {
            i4_tot_header_bits_est +=
                ai4_frms_in_period[j] * ps_bit_allocation->i4_prev_frm_header_bits[j];
        }
        /* Remove the header bits from the remaining bits to find how many bits you
        can transfer.*/
        i4_rem_texture_bits =
            ps_bit_allocation->i4_bits_per_frm * i4_actual_frms_in_period - i4_tot_header_bits_est;

        /* Set the complexities for static content */
        SET_VAR_Q(avq_complexity_estimate[I_PIC], STATIC_I_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[P_PIC], STATIC_P_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[P1_PIC], STATIC_P_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[B_PIC], STATIC_B_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[BB_PIC], STATIC_B_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[B1_PIC], STATIC_B1_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[B11_PIC], STATIC_B1_TO_B2_RATIO, 0);
        SET_VAR_Q(avq_complexity_estimate[B2_PIC], 1, 0);
        SET_VAR_Q(avq_complexity_estimate[B22_PIC], 1, 0);
        /* Get the texture bits required for the current frame */
        ps_bit_allocation->i4_max_tex_bits_for_i = get_bits_based_on_complexity(
            ps_bit_allocation,
            i4_rem_texture_bits,
            ai4_frms_in_period,
            avq_complexity_estimate,
            I_PIC,
            0);
    }
    /* initialise the GOP and bit errors to zero */
    ps_bit_allocation->i4_gop_level_bit_error = 0;
    ps_bit_allocation->i4_frame_level_bit_error = 0;
    for(i = 0; i < MAX_NUM_FRAME_PARALLEL; i++)
    {
        ps_bit_allocation->ai4_cur_frm_est_tex_bits[i] = 0;
        ps_bit_allocation->ai4_cur_frm_est_hdr_bits[i] = 0;
    }
    ps_bit_allocation->i4_buffer_based_bit_error = 0;
    ps_bit_allocation->i4_bits_from_buffer_in_cur_gop = 0;
    ps_bit_allocation->i4_excess_bits_from_buffer = 0;
    ps_bit_allocation->i4_lap_window = i4_lap_window;
    ps_bit_allocation->i8_cur_gop_bit_consumption = 0;
    //ps_bit_allocation->i8_2pass_gop_error_accum = 0;
    ps_bit_allocation->f_qscale_max_clip_in_second_pass = (float)0x7FFFFFFF;

    /*Buffer play for single pass*/
    if(rc_pass != 2)
    {
        /*Find ps_bit_allocation->f_min_complexity_cross_peak_rate*/
        /*Find the complexity which maps to peak bit-rate*/
        {
            ps_bit_allocation->f_min_complexity_cross_peak_rate = ba_get_min_complexity_for_peak_br(
                i4_peak_bit_rate[0], i4_bit_rate, 10.0f, 1.0f, 0.0f, rc_pass);
        }
    }

    ps_bit_allocation->i4_total_2pass_frames = 0;
    ps_bit_allocation->i8_2pass_avg_bit_rate = -1;
}

/*****************************************************************************
  Function Name : ba_init_stat_data
  Description   : The parsing of stat file is done at the end of init (by that time
                  bit allocation init would have already happened,The memory for gop
                  stat data is alocated inside the parse stat file code. Hence the
                  pointer has to be updated again.
  Inputs        : ps_bit_allocation
                  ps_pic_handling
                  pv_gop_stat      - pointer to update
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_init_stat_data(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    void *pv_gop_stat,
    WORD32 *pi4_pic_dist_in_cur_gop,
    WORD32 i4_total_bits_in_period,
    WORD32 i4_excess_bits)

{
    WORD32 i4_tot_frames_in_gop = 0, i;

    ps_bit_allocation->pv_gop_stat = pv_gop_stat;

    /*Init RBIP*/
    /*Get the complexity*/
    ASSERT(ps_bit_allocation->i8_cur_gop_num == 0);
    ASSERT(ps_bit_allocation->i8_frm_num_in_gop == 0);

    /*INit frames of each type*/
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        i4_tot_frames_in_gop += pi4_pic_dist_in_cur_gop[i];
    }

    /*ASSERT(i4_tot_frames_in_gop == i4_intra_period);*/
    /*Also allocate data for first GOP*/
    /*Added for getting actual gop structure*/
    pic_type_update_frms_in_gop(ps_pic_handling, pi4_pic_dist_in_cur_gop);

    multi_pass_set_rbip(
        &ps_bit_allocation->s_rbip, ps_pic_handling, i4_total_bits_in_period, i4_tot_frames_in_gop);

    ps_bit_allocation->i8_2pass_alloc_per_frm_bits =
        (i4_total_bits_in_period + (i4_tot_frames_in_gop >> 1)) / i4_tot_frames_in_gop;
    ps_bit_allocation->i8_bit_consumption_so_far = 0;

    ASSERT(ps_bit_allocation->i4_ba_rc_pass == 2);
}
#endif /* #if NON_STEADSTATE_CODE */

/*****************************************************************************
  Function Name : bit_alloc_get_intra_bits
  Description   :
  Inputs        : ps_bit_allocation
                  ps_pic_handling
                  pvq_complexity_estimate
                  I_to_avg_rest
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 bit_alloc_get_intra_bits(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handling,
    picture_type_e e_pic_type,
    number_t *pvq_complexity_estimate,
    WORD32 i4_is_scd,
    float I_to_avg_rest,
    WORD32 i4_call_type,
    WORD32 i4_non_I_scd,
    float f_percent_head_bits)
{
    WORD32 ai4_frms_in_period[MAX_PIC_TYPE], ai4_frm_in_gop[MAX_PIC_TYPE], tot_frms_in_period = 0;
    WORD32 i4_field_pic,
        i4_safe_margin = 0,
        i4_lap_window;  //margin in buffer to handle I frames that can come immediately after encoding huge static I frame
    /*obtain buffer size*/
    WORD32 i4_buffer_size =
        ((get_cbr_buffer_size(ps_cbr_buf_handling)) >> 4) * UPPER_THRESHOLD_EBF_Q4;
    WORD32 i4_cur_buf_pos = get_cbr_ebf(ps_cbr_buf_handling), i4_max_buffer_based,
           i4_max_buffer_based_I_pic, i, i4_num_scaled_frms = 1;
    WORD32 i4_bit_alloc_window =
        (ps_bit_allocation->s_rbip.i4_tot_frms_in_gop *
         ps_bit_allocation->s_rbip.i4_num_intra_frm_interval);
    WORD32 i4_num_buf_frms,
        ai4_frms_in_baw[MAX_PIC_TYPE];  //window for which I frame bit allocation is done
    WORD32 i4_bits_in_period, i4_frames_in_buf = 0, i4_default_bits_in_period = 0;
    WORD32 i4_est_bits_for_I, i4_peak_drain_rate, i4_subgop_size;
    rc_type_e rc_type = get_rc_type(ps_cbr_buf_handling);
    pic_type_get_actual_frms_in_gop(ps_pic_handling, ai4_frm_in_gop);

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ai4_frms_in_baw[i] =
            ai4_frm_in_gop[i] * ps_bit_allocation->s_rbip.i4_num_intra_frm_interval;
        ai4_frms_in_period[i] =
            ai4_frm_in_gop[i] * ps_bit_allocation->s_rbip.i4_num_intra_frm_interval;
        tot_frms_in_period += ai4_frm_in_gop[i];
    }

    if(i4_call_type == 1)
    {
        i4_default_bits_in_period = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);
        if((i4_default_bits_in_period + ps_bit_allocation->i4_frame_level_bit_error) <
           (i4_default_bits_in_period * 0.30))
        {
            ps_bit_allocation->i4_frame_level_bit_error = 0;  //-(i4_default_bits_in_period * 0.70);
        }
        i4_bits_in_period = i4_default_bits_in_period + ps_bit_allocation->i4_frame_level_bit_error;
        if(i4_non_I_scd == 0)
        {
            /*For the first gop unnecessarily the QP is going high in order to prevent this bits corresponding to full gop instead of gop-subgop*/

            WORD32 i4_intra_int = pic_type_get_intra_frame_interval(ps_pic_handling);
            WORD32 i4_inter_int = pic_type_get_inter_frame_interval(ps_pic_handling);
            if((tot_frms_in_period ==
                (i4_intra_int - i4_inter_int + (1 << ps_bit_allocation->i4_field_pic))) &&
               (i4_intra_int != 1))
            {
                i4_bits_in_period =
                    (WORD32)(i4_bits_in_period * ((float)i4_intra_int / tot_frms_in_period));
            }
        }
        trace_printf("\nBits in period %d", i4_bits_in_period);
    }
    else
    {
        i4_bits_in_period = ret_rbip_default_preenc(&ps_bit_allocation->s_rbip, ps_pic_handling);

        if(ps_bit_allocation->i4_ba_rc_pass == 2)
            i4_default_bits_in_period = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);
    }

    i4_peak_drain_rate = get_buf_max_drain_rate(ps_cbr_buf_handling);
    i4_num_buf_frms =
        (get_cbr_buffer_size(ps_cbr_buf_handling) + (ps_bit_allocation->i4_bits_per_frm >> 1)) /
        ps_bit_allocation->i4_bits_per_frm;
    /*In VBR encoder buffer will be drained faster, i4_num_buf_frms should correspond to maximum number of bits that can be drained
      In CBR both peak and average must be same*/
    i4_num_buf_frms = i4_num_buf_frms * i4_peak_drain_rate / ps_bit_allocation->i4_bits_per_frm;

    i4_field_pic = pic_type_get_field_pic(ps_pic_handling);

    i4_subgop_size = pic_type_get_inter_frame_interval(ps_pic_handling);
    if(pvq_complexity_estimate == NULL)
        i4_cur_buf_pos = 0;

    i4_lap_window = ps_bit_allocation->i4_lap_window;

    /*assume minimum lap visibilty.A static I frame is given only the bits of duration for which we have visibility*/
    if(ps_bit_allocation->i4_lap_window < MINIMUM_VISIBILITY_B4_STATIC_I)
    {
        i4_lap_window = MINIMUM_VISIBILITY_B4_STATIC_I;
    }
    else
    {
        i4_lap_window = ps_bit_allocation->i4_lap_window;
        /*clip buffer window to max of lap window or buffer window*/
        if((i4_lap_window < i4_num_buf_frms) && (i4_call_type == 1))
            i4_num_buf_frms = i4_lap_window + i4_subgop_size;
    }

    if(i4_lap_window < MINIMUM_FRM_I_TO_REST_LAP_ENABLED)
        i4_lap_window = MINIMUM_FRM_I_TO_REST_LAP_ENABLED;
    if(ps_bit_allocation->i4_ba_rc_pass != 2)
    {
        if(i4_lap_window < i4_num_buf_frms)
            i4_num_buf_frms = i4_lap_window;
    }

    if(i4_num_buf_frms > tot_frms_in_period)
    {
        i4_num_buf_frms = tot_frms_in_period;
        i4_bit_alloc_window = i4_num_buf_frms;
    }
    /*get picture type dist based on bit alloc window*/
    if(i4_num_buf_frms < tot_frms_in_period)
    {
        for(i = 1; i < ps_bit_allocation->i4_num_active_pic_type; i++)
        {
            ai4_frms_in_baw[i] =
                (ai4_frms_in_period[i] * i4_num_buf_frms + (tot_frms_in_period >> 1)) /
                tot_frms_in_period;
            i4_num_scaled_frms += ai4_frms_in_baw[i];
            if(ps_bit_allocation->i4_field_pic)
            {
                ai4_frms_in_baw[i + FIELD_OFFSET] = ai4_frms_in_baw[i];
                i4_num_scaled_frms += ai4_frms_in_baw[i];
            }
        }
        if(ps_bit_allocation->i4_field_pic)
        {
            ai4_frms_in_baw[5]++;
            i4_num_scaled_frms++;
        }
        //if prorating is not exact account for diff with highest layer pic types
        if(!ps_bit_allocation->i4_field_pic)
        {
            ai4_frms_in_baw[ps_bit_allocation->i4_num_active_pic_type - 1] +=
                (i4_num_buf_frms - i4_num_scaled_frms);
        }
        else
        {
            ai4_frms_in_baw[ps_bit_allocation->i4_num_active_pic_type - 1] +=
                ((i4_num_buf_frms - i4_num_scaled_frms) >> 1);
            ai4_frms_in_baw[ps_bit_allocation->i4_num_active_pic_type - 1 + FIELD_OFFSET] +=
                ((i4_num_buf_frms - i4_num_scaled_frms) >> 1);
        }
        i4_bits_in_period =
            ((LWORD64)i4_bits_in_period * i4_num_buf_frms + (tot_frms_in_period >> 1)) /
            tot_frms_in_period;
        i4_bit_alloc_window = i4_num_buf_frms;
    }

    i4_safe_margin = (WORD32)(i4_buffer_size * 0.1);
    i4_max_buffer_based = ((LWORD64)i4_buffer_size - i4_cur_buf_pos) /
                          ps_bit_allocation->i4_bits_per_frm * i4_peak_drain_rate;
    i4_max_buffer_based_I_pic = i4_buffer_size - i4_cur_buf_pos;

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        i4_frames_in_buf += ai4_frms_in_baw[i];
    }

    if((rc_type == VBR_STREAMING) && (i4_call_type == 1))
    {
        WORD32 i4_delay_frames = cbr_get_delay_frames(ps_cbr_buf_handling);
        i4_max_buffer_based =
            (i4_peak_drain_rate *
                 (ps_bit_allocation->s_rbip.i4_tot_frms_in_gop + (WORD32)(i4_delay_frames * 0.8f)) -
             i4_cur_buf_pos);

        /*RBIP is updated once it is restricted for an Intra period */
        if(i4_default_bits_in_period > i4_max_buffer_based)
            update_rbip(
                &ps_bit_allocation->s_rbip,
                ps_pic_handling,
                i4_max_buffer_based - i4_default_bits_in_period);

        i4_max_buffer_based =
            (i4_peak_drain_rate * (i4_frames_in_buf + (WORD32)(i4_delay_frames * 0.8f)) -
             i4_cur_buf_pos);
    }
    else
    {
        i4_max_buffer_based =
            ((((LWORD64)i4_buffer_size - i4_cur_buf_pos) / ps_bit_allocation->i4_bits_per_frm) +
             i4_frames_in_buf) *
            i4_peak_drain_rate;
    }

    /*the estimated bits for total period is clipped to buffer limits*/
    if(i4_bits_in_period > i4_max_buffer_based)
        i4_bits_in_period = i4_max_buffer_based;

    /*get I pic bits with altered bits in period*/
    if((!i4_is_scd) &&
       (ps_bit_allocation->i4_num_frames_since_last_I_frame <
        (ps_bit_allocation->i4_frame_rate * 2) / 1000) &&
       (ps_bit_allocation->i4_ba_rc_pass != 2))
    {
        /*returns texture bits*/
        LWORD64 i8_header_bits_in_previous_period = 0, i8_total_bits_in_previous_period = 0,
                i4_frames_in_header = 0;
        WORD32 i4_texture_bits = 0;
        float f_percent_header_bits = 0.0f;
        /* Remove the header bits from the remaining bits to find how many bits you
        can transfer.*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            i8_header_bits_in_previous_period +=
                (ps_bit_allocation->i4_prev_frm_header_bits[i] * ai4_frms_in_baw[i]);
            i8_total_bits_in_previous_period +=
                (ps_bit_allocation->ai4_prev_frm_tot_bits[i] * ai4_frms_in_baw[i]);
            i4_frames_in_header += ai4_frms_in_baw[i];
        }

        if((i4_call_type == 1) && (ps_bit_allocation->i4_ba_rc_pass == 2))
        {
            i4_texture_bits = (WORD32)(i4_bits_in_period * (1 - f_percent_head_bits));
        }
        else
        {
            f_percent_header_bits =
                (float)i8_header_bits_in_previous_period / i8_total_bits_in_previous_period;
            i4_texture_bits =
                i4_bits_in_period - (WORD32)(f_percent_header_bits * i4_bits_in_period);
        }

        if(i4_call_type == 1)
        {
            trace_printf(
                "\nHeader Bits in period %d, total_frames %d "
                "i4_max_buffer_based %d ",
                (WORD32)f_percent_header_bits * i4_bits_in_period,
                i4_frames_in_header,
                i4_max_buffer_based);
        }
        i4_est_bits_for_I = get_bits_based_on_complexity(
            ps_bit_allocation,
            i4_texture_bits,
            ai4_frms_in_baw,
            pvq_complexity_estimate,
            e_pic_type,
            i4_call_type);
        /*twice the bitrate */
        if(i4_est_bits_for_I > ((ps_bit_allocation->i4_bit_rate << 1) -
                                ps_bit_allocation->i4_prev_frm_header_bits[I_PIC]))
            i4_est_bits_for_I =
                ((ps_bit_allocation->i4_bit_rate << 1) -
                 ps_bit_allocation->i4_prev_frm_header_bits[I_PIC]);

        if(i4_est_bits_for_I >
           (i4_max_buffer_based_I_pic - ps_bit_allocation->i4_prev_frm_header_bits[I_PIC]))
        {
            i4_est_bits_for_I =
                (i4_max_buffer_based_I_pic - ps_bit_allocation->i4_prev_frm_header_bits[I_PIC]);
        }
    }
    else
    {
        /*returns total bits incase of scene cut*/
        ASSERT(ai4_frms_in_baw[I_PIC] != 0);
        if((i4_non_I_scd == 1) && (i4_call_type == 1) &&
           (ps_bit_allocation->f_curr_i_to_sum != 1.0f))
            ai4_frms_in_baw[I_PIC]++;

        i4_est_bits_for_I = (WORD32)(
            (i4_bits_in_period * I_to_avg_rest * ai4_frms_in_baw[I_PIC]) /
            (ai4_frms_in_baw[I_PIC] * I_to_avg_rest +
             (i4_bit_alloc_window - ai4_frms_in_baw[I_PIC])));

        if(i4_call_type == 1)
            i4_est_bits_for_I =
                (WORD32)((float)i4_est_bits_for_I * ps_bit_allocation->f_curr_i_to_sum);
        else
        {
            if(ai4_frms_in_baw[I_PIC] > 0)
                i4_est_bits_for_I = (WORD32)((float)i4_est_bits_for_I / ai4_frms_in_baw[I_PIC]);
        }

        if(i4_call_type == 1)
        {
            trace_printf(
                "bits in period %d I_to_avg_rest %f f_curr_i_to_sum %f i "
                "frames %d i4_non_I_scd %d ",
                i4_bits_in_period,
                I_to_avg_rest,
                ps_bit_allocation->f_curr_i_to_sum,
                ai4_frms_in_baw[I_PIC],
                i4_non_I_scd);
        }

        if(i4_est_bits_for_I > (ps_bit_allocation->i4_bit_rate << 1))
            i4_est_bits_for_I = (ps_bit_allocation->i4_bit_rate << 1);
        if(i4_est_bits_for_I > i4_max_buffer_based_I_pic)
            i4_est_bits_for_I = i4_max_buffer_based_I_pic;
    }

    return i4_est_bits_for_I;
}

/*****************************************************************************
  Function Name : get_cur_frm_est_texture_bits
  Description   : Based on remaining bits in period and rd_model
                  the number of bits required for the current frame is estimated.
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_rd_model - rd model pointer (for all the frame types)
                  e_pic_type - picture type
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 get_cur_frm_est_texture_bits(
    bit_allocation_t *ps_bit_allocation,
    rc_rd_model_handle *pps_rd_model,
    est_sad_handle ps_est_sad,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buffer,
    picture_type_e e_pic_type,
    WORD32 i4_use_model,
    WORD32 i4_is_scd_frame,
    WORD32 i4_call_type,
    float i_to_avg_ratio,
    WORD32 i4_is_model_valid)
{
    WORD32 i, j;
    WORD32 i4_est_texture_bits_for_frm;
    WORD32 i4_rem_texture_bits;
    number_t avq_complexity_estimate[MAX_PIC_TYPE];
    WORD32 ai4_frms_in_period[MAX_PIC_TYPE];
    WORD32 i4_max_consumable_bits, i4_est_tot_head_bits_period = 0, i4_total_bits_prev_gop = 0;
    WORD32 i4_field_pic, i4_inter_frame_int;
    WORD32 complexity_est = 0;
    float f_percent_head_bits = 0.0f;
    WORD32 i4_intra_frm_int;
    i4_intra_frm_int = pic_type_get_actual_intra_frame_interval(ps_pic_handling);
    i4_inter_frame_int = pic_type_get_inter_frame_interval(ps_pic_handling);
    i4_field_pic = pic_type_get_field_pic(ps_pic_handling);

    /* If the complexity estimate is not filled based on
    1) Since not using model
    2) Using the module but one of the estimate values are zero
    Then set the complexity estimate based on default values */
    //  if(!complexity_est)
    {
        /* Hardcoding the bit ratios between I, P and B same as during init time*/
        SET_VAR_Q(
            avq_complexity_estimate[I_PIC],
            (I_TO_P_BIT_RATIO * P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
            0);
        SET_VAR_Q(
            avq_complexity_estimate[P_PIC],
            (P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
            0);
        SET_VAR_Q(
            avq_complexity_estimate[P1_PIC],
            (P_TO_B_BIT_RATIO * B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO),
            0);
        SET_VAR_Q(avq_complexity_estimate[B_PIC], (B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO), 0);
        SET_VAR_Q(avq_complexity_estimate[BB_PIC], (B_TO_B1_BIT_RATO0 * B1_TO_B2_BIT_RATIO), 0);
        SET_VAR_Q(
            avq_complexity_estimate[B1_PIC],
            (B1_TO_B2_BIT_RATIO),
            0);  //temporarliy treat ref and non ref as same
        SET_VAR_Q(avq_complexity_estimate[B11_PIC], (B1_TO_B2_BIT_RATIO), 0);
        SET_VAR_Q(avq_complexity_estimate[B2_PIC], 1, 0);
        SET_VAR_Q(avq_complexity_estimate[B22_PIC], 1, 0);
    }
    /* Get the rem_frms_in_gop & the frms_in_gop from the pic_type state struct */
    /* pic_type_get_rem_frms_in_gop(ps_pic_handling, ai4_rem_frms_in_period); */
    pic_type_get_frms_in_gop(ps_pic_handling, ai4_frms_in_period);

    /* Depending on the number of gops in a period, find the num_frms_in_prd */
    for(j = 0; j < MAX_PIC_TYPE; j++)
    {
        /* ai4_rem_frms_in_period[j] += (ai4_frms_in_period[j] * (ps_bit_allocation->i4_num_gops_in_period - 1)); */
        ai4_frms_in_period[j] *= ps_bit_allocation->i4_num_gops_in_period;
    }

    /* If a frame is detected as SCD and bit allocation is asked for the remaining part of the frame
    we allocate bits assuming that frame as a I frame. So reduce 1 frame from the picture type coming in
    and add that to I frame */
    if(i4_is_scd_frame && e_pic_type != I_PIC)
    {
        /* ai4_rem_frms_in_period[0]++;ai4_rem_frms_in_period[e_pic_type]--; */
        ai4_frms_in_period[0]++;
        ai4_frms_in_period[e_pic_type]--;
    }
    /*HEVC_hierarchy: calculate header bits for all frames in period*/
    for(j = 0; j < MAX_PIC_TYPE; j++)
    {
        i4_est_tot_head_bits_period +=
            ai4_frms_in_period[j] * ps_bit_allocation->i4_prev_frm_header_bits[j];
        i4_total_bits_prev_gop +=
            ai4_frms_in_period[j] * ps_bit_allocation->ai4_prev_frm_tot_bits[j];
    }

    {
        WORD32 ai4_actual_frms_in_gop[MAX_PIC_TYPE], i, i4_total_frames = 0;
        pic_type_get_actual_frms_in_gop(ps_pic_handling, ai4_actual_frms_in_gop);
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            i4_total_frames += ai4_actual_frms_in_gop[i];
        }
        i4_max_consumable_bits = ps_bit_allocation->i4_max_bits_per_frm[0] * i4_total_frames;

        /* Remove the header bits from the remaining bits to find how many bits you
           can transfer.*/
        if(i4_call_type == 1)
        {
            if(ps_bit_allocation->i4_ba_rc_pass == 2)
            {
                WORD32 i4_tot_frm_remain = 0, i4_tot_head_bits_in_gop = 0,
                       i4_tot_bits_last_in_gop = 0, i4_use_default_flag = 0;

                WORD32 i4_rbip = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);
                if((i4_rbip + ps_bit_allocation->i4_frame_level_bit_error) < (i4_rbip * 0.30))
                {
                    ps_bit_allocation->i4_frame_level_bit_error = 0;  //-(i4_rbip * 0.70);
                }
                i4_rem_texture_bits =
                    i4_rbip +
                    ps_bit_allocation->i4_frame_level_bit_error /*- i4_est_tot_head_bits_period*/
                    ;

                i4_est_tot_head_bits_period = 0;
                for(j = 0; j < MAX_PIC_TYPE; j++)
                {
                    if((WORD32)ps_bit_allocation->af_sum_weigh[j][1] > 0)
                    {
                        i4_tot_frm_remain += (WORD32)ps_bit_allocation->af_sum_weigh[j][1];
                        i4_tot_head_bits_in_gop += (WORD32)(
                            ps_bit_allocation->i4_prev_frm_header_bits[j] *
                            ps_bit_allocation->af_sum_weigh[j][1]);
                        i4_tot_bits_last_in_gop += (WORD32)(
                            ps_bit_allocation->ai4_prev_frm_tot_bits[j] *
                            ps_bit_allocation->af_sum_weigh[j][1]);
                        if(ps_bit_allocation->ai4_prev_frm_tot_bits[j] == -1)
                        {
                            i4_use_default_flag = 1;
                        }
                    }
                }

                if(i4_use_default_flag != 1)
                {
                    f_percent_head_bits = (float)i4_tot_head_bits_in_gop / i4_tot_bits_last_in_gop;

                    if(f_percent_head_bits > 0.7f)
                        f_percent_head_bits = 0.7f;

                    /*Subtracting a percentage of header bits from the remaining bits in period*/
                    i4_rem_texture_bits = (WORD32)(i4_rem_texture_bits * (1 - f_percent_head_bits));
                }
                else
                {
                    /*Assuming 30% of bits will go for header bits in a gop in preenc*/
                    i4_rem_texture_bits -= (WORD32)((float)i4_rem_texture_bits * 0.3f);
                }

                trace_printf(
                    "Remaining texture bits %d fbe %d fphb %f thbg %d tblg %d",
                    i4_rem_texture_bits,
                    ps_bit_allocation->i4_frame_level_bit_error,
                    f_percent_head_bits,
                    i4_tot_head_bits_in_gop,
                    i4_tot_bits_last_in_gop);
            }
            else
            {
                /* Remove the header bits from the remaining bits to find how many bits you
            can transfer.*/
                WORD32 i4_rbip = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);
                if((i4_rbip + ps_bit_allocation->i4_frame_level_bit_error) < (i4_rbip * 0.30))
                {
                    ps_bit_allocation->i4_frame_level_bit_error = 0;  //-(i4_rbip * 0.70);
                }
                i4_rem_texture_bits = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0) +
                                      ps_bit_allocation->i4_frame_level_bit_error;

                i4_est_tot_head_bits_period = (WORD32)(
                    ((float)(i4_est_tot_head_bits_period) / (float)i4_total_bits_prev_gop) *
                    i4_rem_texture_bits);

                if(i4_is_model_valid)
                {
                    i4_rem_texture_bits = i4_rem_texture_bits - i4_est_tot_head_bits_period;
                }
                else
                {
                    /*inorder to estimate the buffer position for model invalid cases, to control
                    encoder buffer overflow*/
                    i4_rem_texture_bits = ((i4_rem_texture_bits * 3) >> 1);
                }

                trace_printf(
                    "Remaining texture bits %d fbe %d ethp %d",
                    i4_rem_texture_bits,
                    ps_bit_allocation->i4_frame_level_bit_error,
                    i4_est_tot_head_bits_period);
            }

            {
                WORD32 i4_drain_bits_per_frame = get_buf_max_drain_rate(ps_cbr_buffer), i4_ebf;
                WORD32 i4_delay = cbr_get_delay_frames(ps_cbr_buffer), max_buffer_level = 0,
                       rc_type = get_rc_type(ps_cbr_buffer);

                if(rc_type == VBR_STREAMING)
                    max_buffer_level = i4_drain_bits_per_frame * i4_delay;
                else
                    max_buffer_level = get_cbr_buffer_size(ps_cbr_buffer);

                i4_ebf = get_cbr_ebf(ps_cbr_buffer);

                if(i4_ebf > (WORD32)(0.8f * max_buffer_level))
                {
                    if(ps_bit_allocation->af_sum_weigh[e_pic_type][0] > 1.0f)
                        ps_bit_allocation->af_sum_weigh[e_pic_type][0] = 1.0f;
                }
                if(i4_ebf > (WORD32)(0.6f * max_buffer_level))
                {
                    if(ps_bit_allocation->af_sum_weigh[e_pic_type][0] > 1.5f)
                        ps_bit_allocation->af_sum_weigh[e_pic_type][0] = 1.5f;
                }
            }
        }
        else
        {
            i4_rem_texture_bits =
                ret_rbip_default_preenc(&ps_bit_allocation->s_rbip, ps_pic_handling);
            /*Assuming 30% of bits will go for header bits in a gop in preenc*/
            i4_rem_texture_bits -= (WORD32)(i4_rem_texture_bits * 0.3f);
        }
    }

    if(i4_use_model)
    {
        /* The bits are then allocated based on the relative complexity of the
        current frame with respect to that of the rest of the frames in period */
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            number_t vq_lin_mod_coeff, vq_est_sad, vq_K;

            if(ai4_frms_in_period[i] > 0) /*Added for field case */
            {
                /* Getting the linear model coefficient */
                vq_lin_mod_coeff = get_linear_coefficient(pps_rd_model[i]);
                /* Getting the estimated SAD */
                SET_VAR_Q(vq_est_sad, get_est_sad(ps_est_sad, (picture_type_e)i), 0);
                /* Making K factor a var Q format */
                SET_VAR_Q(vq_K, ps_bit_allocation->i2_K[i], K_Q);
                /* Complexity_estimate = [ (lin_mod_coeff * estimated_sad) / K factor ]  */
                mult32_var_q(vq_lin_mod_coeff, vq_est_sad, &vq_lin_mod_coeff);
                div32_var_q(vq_lin_mod_coeff, vq_K, &avq_complexity_estimate[i]);
            }
        }
        /*flag to check if complexity estimate is available*/
        complexity_est = 1;

        /*HEVC_hierarchy: If complexity estimate = 0 for any pic type then use default ratios*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            if(ai4_frms_in_period[i] > 0)
            {
                complexity_est = complexity_est && avq_complexity_estimate[i].sm;
            }
        }
    }

    /* Make the picture type of the SCD frame a I_PIC */
    if(i4_is_scd_frame && e_pic_type != I_PIC)
        e_pic_type = I_PIC;

    if(e_pic_type == I_PIC)
    {
        /*clip min max values*/
        if(i_to_avg_ratio > I_TO_AVG_REST_GOP_BIT_MAX)
            i_to_avg_ratio = I_TO_AVG_REST_GOP_BIT_MAX;

        if(i_to_avg_ratio < I_TO_AVG_REST_GOP_BIT_MIN)
            i_to_avg_ratio = I_TO_AVG_REST_GOP_BIT_MIN;

        i4_est_texture_bits_for_frm = bit_alloc_get_intra_bits(
            ps_bit_allocation,
            ps_pic_handling,
            ps_cbr_buffer,
            e_pic_type,
            avq_complexity_estimate,
            0,
            i_to_avg_ratio,
            i4_call_type,
            0,
            f_percent_head_bits);
    }
    else
    {
        /* Get the texture bits required for the current frame */
        i4_est_texture_bits_for_frm = get_bits_based_on_complexity(
            ps_bit_allocation,
            i4_rem_texture_bits,
            ai4_frms_in_period,
            avq_complexity_estimate,
            e_pic_type,
            i4_call_type);
    }

    ps_bit_allocation->i4_excess_bits_from_buffer = 0;

    /* If the remaining bits in the period becomes negative then the estimated texture
    bits would also become negative. This would send a feedback to the model which
    may go for a toss. Thus sending the minimum possible value = 0 */
    if(i4_est_texture_bits_for_frm < 0)
        i4_est_texture_bits_for_frm = 0;

    return (i4_est_texture_bits_for_frm);
}

/*****************************************************************************
  Function Name : get_cur_frm_est_header_bits
  Description   : Based on remaining bits in period and rd_model
                  the number of bits required for the current frame is estimated.
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_rd_model - rd model pointer (for all the frame types)
                  e_pic_type - picture type
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 get_cur_frm_est_header_bits(bit_allocation_t *ps_bit_allocation, picture_type_e e_pic_type)
{
    //ASSERT(ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type] == 0);
    return (ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type]);
}
/*****************************************************************************
  Function Name : get_rem_bits_in_period
  Description   : Get remaining bits in period
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_pic_handling
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32
    get_rem_bits_in_period(bit_allocation_t *ps_bit_allocation, pic_handling_handle ps_pic_handling)
{
    return (update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0));
}
/*****************************************************************************
  Function Name : get_bits_per_frame
  Description   : Get Bits per frame
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 get_bits_per_frame(bit_allocation_t *ps_bit_allocation)
{
    return ((*ps_bit_allocation).i4_bits_per_frm);
}
/*****************************************************************************
  Function Name : ba_get_gop_bits
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 ba_get_gop_bits(bit_allocation_t *ps_bit_allocation)
{
    gop_level_stat_t *ps_cur_gop_stat;
    ps_cur_gop_stat =
        (gop_level_stat_t *)ps_bit_allocation->pv_gop_stat + ps_bit_allocation->i8_cur_gop_num;
    return (
        ps_cur_gop_stat->i8_bits_allocated_to_gop +
        ps_cur_gop_stat->i8_buffer_play_bits_allocated_to_gop);
}
/*****************************************************************************
  Function Name : ba_get_gop_sad
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 ba_get_gop_sad(bit_allocation_t *ps_bit_allocation)
{
    gop_level_stat_t *ps_cur_gop_stat;
    ps_cur_gop_stat =
        (gop_level_stat_t *)ps_bit_allocation->pv_gop_stat + ps_bit_allocation->i8_cur_gop_num;
    return (ps_cur_gop_stat->i8_acc_gop_sad);
}
/*****************************************************************************
  Function Name : ba_get_buffer_play_bits_for_cur_gop
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 ba_get_buffer_play_bits_for_cur_gop(bit_allocation_t *ps_bit_allocation)
{
    gop_level_stat_t *ps_cur_gop_stat;
    ps_cur_gop_stat =
        (gop_level_stat_t *)ps_bit_allocation->pv_gop_stat + ps_bit_allocation->i8_cur_gop_num;
    return (ps_cur_gop_stat->i8_buffer_play_bits_allocated_to_gop);
}

/*****************************************************************************
  Function Name : update_cur_frm_consumed_bits
  Description   : Based on remaining bits in period and rd_model
                  the number of bits required for the current frame is estimated.
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_rd_model - rd model pointer (for all the frame types)
                  e_pic_type - picture type
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void update_cur_frm_consumed_bits(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handle,
    WORD32 i4_total_frame_bits,
    WORD32 i4_model_updation_hdr_bits,
    picture_type_e e_pic_type,
    UWORD8 u1_is_scd,
    WORD32 i4_last_frm_in_period,
    WORD32 i4_lap_comp_bits_reset,
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_buffer_based_bit_error,
    WORD32 i4_stuff_bits,
    WORD32 i4_lap_window_comp,
    rc_type_e e_rc_type,
    WORD32 i4_num_gop,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_est_text_bits_ctr_update_qp,
    WORD32 *pi4_gop_correction,
    WORD32 *pi4_new_correction)
{
    WORD32 i4_error_bits = get_error_bits(ps_bit_allocation->ps_error_bits);
    WORD32 i4_intra_frm_int, i, i4_flag_no_error_calc = 0; /*i_only*/
    WORD32 i4_do_correction = 0;
    i4_intra_frm_int = pic_type_get_intra_frame_interval(ps_pic_handling);
    ps_bit_allocation->i4_rem_frame_in_period--;

    /*No frame level bit error for top layer pictures*/

    i4_flag_no_error_calc = /*((e_pic_type != B1_PIC && e_pic_type != B11_PIC)  && ps_bit_allocation->i4_num_active_pic_type == 4)||
                            ((e_pic_type != B2_PIC && e_pic_type != B22_PIC)  && ps_bit_allocation->i4_num_active_pic_type == 5) &&*/
        (i4_is_pause_to_resume == 0);

    /* Update the remaining bits in period */
    ps_bit_allocation->i4_bits_from_buffer_in_cur_gop +=
        ps_bit_allocation->i4_excess_bits_from_buffer;
    ps_bit_allocation->i4_buffer_based_bit_error -= ps_bit_allocation->i4_excess_bits_from_buffer;
    ps_bit_allocation->i4_gop_level_bit_error +=
        (-(i4_total_frame_bits + i4_stuff_bits) + i4_error_bits +
         ps_bit_allocation->i4_bits_per_frm);
    ps_bit_allocation->i8_cur_gop_bit_consumption += (i4_total_frame_bits + i4_stuff_bits);

    //if(ps_bit_allocation-> == 2)ASSERT(i4_stuff_bits == 0);//No stuffing in two pass
    //ASSERT(ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type] == 0);
    ps_bit_allocation->i4_buffer_based_bit_error += i4_buffer_based_bit_error;
    ps_bit_allocation->i8_frm_num_in_gop++;
    if(i4_last_frm_in_period && i4_lap_comp_bits_reset)
        i4_lap_comp_bits_reset = 0;  //end of period is always I frame boundary.

    if(e_pic_type == I_PIC)
        ps_bit_allocation->i4_num_frames_since_last_I_frame = 1;
    else
        ps_bit_allocation->i4_num_frames_since_last_I_frame++;

    if((!i4_suppress_bpic_update))
    {
        //if(ps_bit_allocation->ai4_cur_frm_est_tex_bits[i4_est_text_bits_ctr_update_qp] > 0)
        {
            ps_bit_allocation->ai4_prev_frm_tot_est_bits[e_pic_type] =
                ps_bit_allocation->ai4_cur_frm_est_hdr_bits[i4_est_text_bits_ctr_update_qp] +
                ps_bit_allocation->ai4_cur_frm_est_tex_bits[i4_est_text_bits_ctr_update_qp];

            ps_bit_allocation->i4_frame_level_bit_error +=
                (ps_bit_allocation->ai4_cur_frm_est_hdr_bits[i4_est_text_bits_ctr_update_qp] +
                 ps_bit_allocation->ai4_cur_frm_est_tex_bits[i4_est_text_bits_ctr_update_qp] -
                 i4_total_frame_bits);
        }

        trace_printf(
            "Prev frame header %d Total est %d total frame %d",
            ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type],
            ps_bit_allocation->ai4_cur_frm_est_tex_bits[i4_est_text_bits_ctr_update_qp],
            i4_total_frame_bits);
    }

    trace_printf(
        "  rbip = %d  frame lbe = %d    bbbe = %d  bfbicg = %d\n",
        update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0),
        ps_bit_allocation->i4_frame_level_bit_error,
        ps_bit_allocation->i4_buffer_based_bit_error,
        ps_bit_allocation->i4_bits_from_buffer_in_cur_gop);

    /* Update the header bits so that it can be used as an estimate to the next frame */
    if(u1_is_scd)
    {
        /* Initilising the header bits to be used for each picture type */
        init_prev_header_bits(ps_bit_allocation, ps_pic_handling);

        /*init tot bits consumed of previous frame*/
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_bit_allocation->ai4_prev_frm_tot_bits[i] = -1;
            ps_bit_allocation->ai4_prev_frm_tot_est_bits[i] = -1;
        }
        /* In case of SCD, eventhough the frame type is P, it is equivalent to a I frame
           and so the coresponding header bits is updated */
        //ASSERT(i4_model_updation_hdr_bits == 0);
        ps_bit_allocation->i4_prev_frm_header_bits[I_PIC] = i4_model_updation_hdr_bits;
        ps_bit_allocation->ai4_prev_frm_tot_bits[I_PIC] = i4_total_frame_bits;
        ps_bit_allocation->ai4_prev_frm_tot_est_bits[I_PIC] = i4_total_frame_bits;
        /*SCD allowed only for I_PIC*/
        ASSERT(e_pic_type == I_PIC);

#define MAX_NUM_GOPS_IN_PERIOD (5)
        if(ps_bit_allocation->i4_num_gops_in_period != 1 &&
           ps_bit_allocation->i4_num_gops_in_period < MAX_NUM_GOPS_IN_PERIOD)
        {
            /* Whenever there is a scene change increase the number of gops by 2 so that
               the number of bits allocated is not very constrained. */
            ps_bit_allocation->i4_num_gops_in_period += 2;
            /* Add the extra bits in GOP to remaining bits in period */
            change_rbip(
                &ps_bit_allocation->s_rbip,
                ps_bit_allocation->i4_bits_per_frm,
                ps_bit_allocation->i4_num_gops_in_period);
            /* printf((const WORD8*)"SCD rbp %d, ngp %d\n", update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling,0),
            ps_bit_allocation->i4_num_gops_in_period); */
        }
    }
    else
    {
        //ASSERT(i4_model_updation_hdr_bits == 0);
        if(!i4_suppress_bpic_update)
        {
            ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type] = i4_model_updation_hdr_bits;
            ps_bit_allocation->ai4_prev_frm_tot_bits[e_pic_type] = i4_total_frame_bits;
        }
    }

    {
        /* Removng the error due to buffer movement from gop level bit error */
        WORD32 i4_gop_correction = 0;
        WORD32 i4_cur_ebf = get_cbr_ebf(ps_cbr_buf_handle);
        WORD32 i4_vbv_size = get_cbr_buffer_size(ps_cbr_buf_handle);
        WORD32 i4_min_vbv_size = (WORD32)(i4_vbv_size * MIN_THRESHOLD_VBV_GOP_ERROR);
        WORD32 i4_max_vbv_size = (WORD32)(i4_vbv_size * MAX_THRESHOLD_VBV_GOP_ERROR);
        /*get desired buffer level so that bit error can be calculated. desired buf = 1 - lap window complexity*/
        if(ps_bit_allocation->i4_ba_rc_pass != 2)
        {
            WORD32 i4_inter_frame_interval = pic_type_get_inter_frame_interval(ps_pic_handling);
            LWORD64 vbv_buffer_based_excess = 0;
            WORD32 i4_lap_window_comp_temp = i4_lap_window_comp;
            if(ps_bit_allocation->i4_lap_window > i4_inter_frame_interval)
            {
                if(e_rc_type == VBR_STREAMING)
                {
                    if(((float)i4_lap_window_comp / 128) >
                       ps_bit_allocation->f_min_complexity_cross_peak_rate)
                        i4_lap_window_comp_temp =
                            (WORD32)(ps_bit_allocation->f_min_complexity_cross_peak_rate * 128);

                    /*Get excess bits if any from vbv buffer*/
                    vbv_buffer_based_excess = get_vbv_buffer_based_excess(
                        ps_cbr_buf_handle,
                        ps_bit_allocation->f_min_complexity_cross_peak_rate,
                        ((float)i4_lap_window_comp / 128),
                        (i4_intra_frm_int * ps_bit_allocation->s_rbip.i4_num_intra_frm_interval),
                        1);
                }

                i4_do_correction = 1;
                i4_gop_correction = get_error_bits_for_desired_buf(
                    ps_cbr_buf_handle,
                    i4_lap_window_comp_temp,
                    (i4_intra_frm_int * ps_bit_allocation->s_rbip.i4_num_intra_frm_interval));
                /*In case of VBR, don't do buffer based correction if gop_correction is less than 0, as it is less than average*/
                if((e_rc_type == VBR_STREAMING) && (i4_gop_correction <= 0))
                {
                    i4_do_correction = 0;
                }

                /* vbv buffer position based error correction to keep away encoder buffer overflow at GOP (I to I, not user configured)*/
                if(i4_do_correction)
                {
                    WORD32 i4_buffer_err_bits;
                    /*check if the ebf is greater than max ebf,
                    then account for complete error above max ebf in the current GOP itself*/
                    if(i4_cur_ebf > i4_max_vbv_size)
                    {
                        i4_gop_correction -= (i4_cur_ebf - i4_max_vbv_size);
                        *pi4_new_correction -= (i4_cur_ebf - i4_max_vbv_size);
                        i4_cur_ebf = i4_max_vbv_size;
                    }
                    /* if ebf is above min but less than max, then distribute to the next GOPs*/
                    if(i4_cur_ebf > i4_min_vbv_size)
                    {
                        WORD32 i4_num_gops;
                        float f_ebf_percent;
                        /*compute the error bits to be distributed over the next GOPs*/
                        i4_buffer_err_bits = (i4_cur_ebf - i4_min_vbv_size);
                        /*compute number fo GOPs the error to be distributed
                            high error -> few GOPs, less error -> more GOPs*/
                        f_ebf_percent = ((float)i4_cur_ebf / i4_vbv_size);
                        i4_num_gops = (WORD32)((1.0 - f_ebf_percent) * 10) + 2;
                        /*add the error bits to the period*/
                        i4_gop_correction -= (WORD32)(i4_buffer_err_bits / i4_num_gops);
                        *pi4_new_correction -= (WORD32)(i4_buffer_err_bits / i4_num_gops);
                    }
                }
                *pi4_gop_correction = i4_gop_correction;
                set_rbip(
                    &ps_bit_allocation->s_rbip,
                    (i4_gop_correction + (WORD32)vbv_buffer_based_excess));

                update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);
                ASSERT(ps_bit_allocation->i4_bits_from_buffer_in_cur_gop == 0);
                trace_printf("\nRBIP updated ");
            }
            /* initialise the GOP and bit errors to zero */
            ps_bit_allocation->i4_gop_level_bit_error = 0;
            /*frame level error can't be carried over when it is more than VBV buffer size*/
            if(ps_bit_allocation->i4_frame_level_bit_error > i4_max_vbv_size)
            {
                ps_bit_allocation->i4_frame_level_bit_error = i4_max_vbv_size;
            }
            if((i4_last_frm_in_period) ||
               (i4_intra_frm_int == 1 && ps_bit_allocation->i4_rem_frame_in_period == 0))
            { /*For 1st pass set the errors to 0 at end of a gop*/
                ps_bit_allocation->i8_cur_gop_bit_consumption = 0;
                ps_bit_allocation->i4_frame_level_bit_error = 0;
                ps_bit_allocation->i4_bits_from_buffer_in_cur_gop = 0;
                ps_bit_allocation->i4_rem_frame_in_period =
                    ps_bit_allocation->i4_num_gops_in_period *
                    i4_intra_frm_int; /*TBD: I only case*/
                ps_bit_allocation->i8_frm_num_in_gop = 0;
            }
        }
    }

    if(i4_last_frm_in_period && i4_intra_frm_int != 1)
    {
        /* If the number of gops in period has been increased due to scene change, slowly bring in down
           across the gops */
        if(ps_bit_allocation->i4_num_gops_in_period >
           ps_bit_allocation->i4_actual_num_gops_in_period)
        {
            ps_bit_allocation->i4_num_gops_in_period--;
            change_rbip(
                &ps_bit_allocation->s_rbip,
                ps_bit_allocation->i4_bits_per_frm,
                ps_bit_allocation->i4_num_gops_in_period);
        }
    }
    /*Check for complexity based bits reset in future with GOP*/

    /* Update the lower modules */
    update_error_bits(ps_bit_allocation->ps_error_bits);
}

/*****************************************************************************
  Function Name : change_remaining_bits_in_period
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure

  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void change_remaining_bits_in_period(
    bit_allocation_t *ps_bit_allocation,
    WORD32 i4_bit_rate,
    WORD32 i4_frame_rate,
    WORD32 *i4_peak_bit_rate)
{
    WORD32 i4_new_avg_bits_per_frm, i4_new_peak_bits_per_frm[MAX_NUM_DRAIN_RATES];
    int i;

    /* Calculate the new per frame bits */
    X_PROD_Y_DIV_Z(i4_bit_rate, 1000, i4_frame_rate, i4_new_avg_bits_per_frm);

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(i4_peak_bit_rate[i], 1000, i4_frame_rate, i4_new_peak_bits_per_frm[i]);
    }

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        ps_bit_allocation->i4_max_bits_per_frm[i] = i4_new_peak_bits_per_frm[i];
    }

    /* Get the rem_frms_in_prd & the frms_in_prd from the pic_type state struct */
    /* pic_type_get_rem_frms_in_gop(ps_pic_handling, i4_rem_frms_in_period); */

    /* If the difference > 0(/ <0), the remaining bits in period needs to be increased(/decreased)
    based on the remaining number of frames */
    change_rbip(
        &ps_bit_allocation->s_rbip,
        i4_new_avg_bits_per_frm,
        ps_bit_allocation->i4_num_gops_in_period);

    /* Update the new average bits per frame */
    ps_bit_allocation->i4_bits_per_frm = i4_new_avg_bits_per_frm;

    /*change max_bits_per_frame*/
    //ps_bit_allocation->i4_max_bits_per_frm[0]=i4_new_avg_bits_per_frm;
    //ps_bit_allocation->i4_max_bits_per_frm[1]=i4_new_avg_bits_per_frm;
    ps_bit_allocation->i4_min_bits_per_frm =
        i4_new_avg_bits_per_frm; /*VBR storage related parameter so this variable is currently not in use*/
    /* change the lower modules state */
    /*#ifdef DYNAMIC_RC*/
    if(i4_bit_rate != ps_bit_allocation->i4_bit_rate)
    {
        X_PROD_Y_DIV_Z(
            ps_bit_allocation->i4_max_tex_bits_for_i,
            i4_bit_rate,
            ps_bit_allocation->i4_bit_rate,
            ps_bit_allocation->i4_max_tex_bits_for_i);
    }
    /*#endif*/

    change_bitrate_in_error_bits(ps_bit_allocation->ps_error_bits, i4_bit_rate);
    change_frm_rate_in_error_bits(ps_bit_allocation->ps_error_bits, i4_frame_rate);

    /* Store the modified frame_rate */
    ps_bit_allocation->i4_frame_rate = i4_frame_rate;
    ps_bit_allocation->i4_bit_rate = i4_bit_rate;
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        ps_bit_allocation->ai4_peak_bit_rate[i] = i4_peak_bit_rate[i];
}
/*****************************************************************************
  Function Name : change_ba_peak_bit_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ai4_peak_bit_rate
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void change_ba_peak_bit_rate(bit_allocation_t *ps_bit_allocation, WORD32 *ai4_peak_bit_rate)
{
    WORD32 i;
    /* Calculate the bits per frame */
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(
            ai4_peak_bit_rate[i],
            1000,
            ps_bit_allocation->i4_frame_rate,
            ps_bit_allocation->i4_max_bits_per_frm[i]);
        ps_bit_allocation->ai4_peak_bit_rate[i] = ai4_peak_bit_rate[i];
    }
}
/*****************************************************************************
  Function Name : check_and_update_bit_allocation
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_pic_handling
                  i4_max_bits_inflow_per_frm
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void check_and_update_bit_allocation(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    WORD32 i4_max_bits_inflow_per_frm)
{
    WORD32 i4_max_drain_bits, i4_extra_bits, i4_less_bits, i4_allocated_saved_bits,
        i4_min_bits_for_period;
    WORD32 i4_num_frms_in_period = get_actual_num_frames_in_gop(ps_pic_handling);
    WORD32 i4_rem_bits_in_period = update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, 0);

    /* If the remaining bits is greater than what can be drained in that period
            Clip the remaining bits in period to the maximum it can drain in that pariod
            with the error of current buffer size.Accumulate the saved bits if any.
       else if the remaining bits is lesser than the minimum bit rate promissed in that period
            Add the excess bits to remaining bits in period and reduce it from the saved bits
       Else
            Provide the extra bits from the "saved bits pool".*/

    i4_max_drain_bits = ps_bit_allocation->i4_num_gops_in_period * i4_num_frms_in_period *
                        i4_max_bits_inflow_per_frm;

    /* Practical DBF = middle of the buffer */
    /* NITT TO BE VERIFIED
    MAx drain bits becomes negative if the buffer underflows
    i4_max_drain_bits += (i4_cur_buf_size + i4_max_bits_inflow_per_frm - i4_tot_frame_bits); */

    i4_min_bits_for_period = ps_bit_allocation->i4_num_gops_in_period * i4_num_frms_in_period *
                             ps_bit_allocation->i4_min_bits_per_frm;

    /* printf((const WORD8*)" mdb %d, mbfp %d, rbip %d, sb %d \n",i4_max_drain_bits,
        i4_min_bits_for_period, ps_bit_allocation->i4_rem_bits_in_period, ps_bit_allocation->i4_saved_bits); */
    if(i4_rem_bits_in_period > i4_max_drain_bits)
    {
        i4_extra_bits = (i4_rem_bits_in_period - i4_max_drain_bits);
        update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, -1 * i4_extra_bits);
        overflow_avoided_summation(&ps_bit_allocation->i4_saved_bits, i4_extra_bits);
    }
    else if(i4_rem_bits_in_period < i4_min_bits_for_period)
    {
        i4_extra_bits = (i4_min_bits_for_period - i4_rem_bits_in_period);
        update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, i4_extra_bits);
        overflow_avoided_summation(&ps_bit_allocation->i4_saved_bits, -1 * i4_extra_bits);
    }
    else if(ps_bit_allocation->i4_saved_bits > 0)
    {
        i4_less_bits = i4_max_drain_bits - i4_rem_bits_in_period;
        i4_allocated_saved_bits = MIN(i4_less_bits, ps_bit_allocation->i4_saved_bits);
        update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handling, i4_allocated_saved_bits);
        ps_bit_allocation->i4_saved_bits -= i4_allocated_saved_bits;
    }
    return;
}
/*****************************************************************************
  Function Name : ba_get_frame_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_frame_rate(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i4_frame_rate);
}
/*****************************************************************************
  Function Name : ba_get_bit_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_bit_rate(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i4_bit_rate);
}
/*****************************************************************************
  Function Name : ba_get_2pass_avg_bit_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 ba_get_2pass_avg_bit_rate(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i8_2pass_avg_bit_rate);
}
/*****************************************************************************
  Function Name : ba_set_2pass_avg_bit_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_set_2pass_avg_bit_rate(bit_allocation_t *ps_bit_allocation, LWORD64 i8_2pass_avg_bit_rate)
{
    ps_bit_allocation->i8_2pass_avg_bit_rate = i8_2pass_avg_bit_rate;
}
/*****************************************************************************
  Function Name : ba_get_peak_bit_rate
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_get_peak_bit_rate(bit_allocation_t *ps_bit_allocation, WORD32 *pi4_peak_bit_rate)
{
    WORD32 i;
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        pi4_peak_bit_rate[i] = ps_bit_allocation->ai4_peak_bit_rate[i];
    }
}
/*****************************************************************************
  Function Name : init_intra_header_bits
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void init_intra_header_bits(bit_allocation_t *ps_bit_allocation, WORD32 i4_intra_header_bits)
{
    //ASSERT(i4_intra_header_bits == 0);
    ps_bit_allocation->i4_prev_frm_header_bits[0] = i4_intra_header_bits;
}
/*****************************************************************************
  Function Name : get_prev_header_bits
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 get_prev_header_bits(bit_allocation_t *ps_bit_allocation, WORD32 pic_type)
{
    //ASSERT(ps_bit_allocation->i4_prev_frm_header_bits[pic_type] == 0);
    return (ps_bit_allocation->i4_prev_frm_header_bits[pic_type]);
}

#define I_TO_P_RATIO_HI_MO (16)
#define P_TO_B_RATIO_HI_MO (18)
#define P_TO_B_RATIO_HI_MO_HBR (16)
/*****************************************************************************
  Function Name : set_Kp_Kb_for_hi_motion
  Description   :
  Inputs        : ps_bit_allocation - bit_allocation structure
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void set_Kp_Kb_for_hi_motion(bit_allocation_t *ps_bit_allocation)
{
    ps_bit_allocation->i2_K[I_PIC] = (1 << K_Q);
    ps_bit_allocation->i2_K[P_PIC] = I_TO_P_RATIO_HI_MO;

    if(ps_bit_allocation->i4_is_hbr)
    {
        ps_bit_allocation->i2_K[B_PIC] = (P_TO_B_RATIO_HI_MO * I_TO_P_RATIO_HI_MO) >> K_Q;
    }
    else
    {
        ps_bit_allocation->i2_K[B_PIC] = (P_TO_B_RATIO_HI_MO_HBR * I_TO_P_RATIO_HI_MO) >> K_Q;
    }
}
/*****************************************************************************
  Function Name : reset_Kp_Kb
  Description   : I_P_B_B1_B2 QP offset calculation based on hme sad
  Inputs        :
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/

void reset_Kp_Kb(
    bit_allocation_t *ps_bit_allocation,
    float f_i_to_avg_ratio,
    WORD32 i4_num_active_pic_type,
    float f_hme_sad_per_pixel,
    float f_max_hme_sad_per_pixel,
    WORD32 *pi4_complexity_bin,
    WORD32 i4_rc_pass)
{
    WORD32 i, i4_ratio = (WORD32)(f_max_hme_sad_per_pixel / f_hme_sad_per_pixel);
    WORD32 ai4_offsets[5] = { 0 };
    float f_ratio = f_max_hme_sad_per_pixel / f_hme_sad_per_pixel;

    /*Filling out the offfset array for QP offset 0 - 7*/
    const WORD32 ai4_offset_qp[8] = {
        (1 << K_Q),
        I_TO_P_RATIO,
        ((P_TO_B_RATIO * I_TO_P_RATIO) >> K_Q),
        (B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q),
        (B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q + K_Q),
        (B1_TO_B2_RATIO * B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO * I_TO_P_RATIO) >>
            (K_Q + K_Q + K_Q + K_Q),
        (B1_TO_B2_RATIO * B1_TO_B2_RATIO * B1_TO_B2_RATIO * B_TO_B1_RATIO * P_TO_B_RATIO *
         I_TO_P_RATIO) >>
            (K_Q + K_Q + K_Q + K_Q + K_Q),
        (B1_TO_B2_RATIO * B1_TO_B2_RATIO * B1_TO_B2_RATIO * B1_TO_B2_RATIO * B_TO_B1_RATIO *
         P_TO_B_RATIO * I_TO_P_RATIO) >>
            (K_Q + K_Q + K_Q + K_Q + K_Q + K_Q)
    };

    ba_get_qp_offset_offline_data(
        ai4_offsets, i4_ratio, f_ratio, i4_num_active_pic_type, pi4_complexity_bin);
    for(i = 0; i < 5; i++)
    {
        ASSERT((ai4_offsets[i] >= 0) && (ai4_offsets[i] <= 7));
        ps_bit_allocation->i2_K[i] = ai4_offset_qp[ai4_offsets[i]];

        /*For interlaced also we are filling out the offsets*/
        if(i > 0)
            ps_bit_allocation->i2_K[i + 4] = ai4_offset_qp[ai4_offsets[i]];
    }
}

/*****************************************************************************
  Function Name : ba_get_qp_offset_offline_data
  Description   : Offline model for qp offset calculation
  Inputs        : ai4_offsets
                  i4_ratio
                  f_ratio
                  i4_num_active_pic_type
                  pi4_complexity_bin
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_get_qp_offset_offline_data(
    WORD32 ai4_offsets[5],
    WORD32 i4_ratio,
    float f_ratio,
    WORD32 i4_num_active_pic_type,
    WORD32 *pi4_complexity_bin)
{
    WORD32 i4_bin;
    /*Desired QP offset's for different complexity bins depending on number of temporal layers*/
    /*There are 6 complexity bins
    Max_compl - Max_compl*3/4,
    Max_compl*3/4 - Max_compl*1/2,
    Max_compl*1/2 - Max_compl*1/4,
    Max_compl*1/4 - Max_compl*1/8,
    Max_compl*1/8 - Max_compl*1/16
    <Max_compl*1/16*/
    /*The kids_rain content was run on different resolutions and the max value for different temporal configs is the max value used*/

    /*First index for complexity bin, second index for pic_types (P,B,B1,B2)*/
    const WORD32 ai4_offset_values_7B[7][4] = { { 0, 1, 1, 2 }, { 1, 1, 2, 3 }, { 1, 2, 3, 3 },
                                                { 1, 2, 3, 4 }, { 2, 2, 3, 4 }, { 2, 3, 4, 5 },
                                                { 3, 4, 5, 6 } };
    const WORD32 ai4_offset_values_3B[7][3] = { { 0, 1, 2 }, { 1, 2, 2 }, { 1, 2, 3 }, { 2, 2, 3 },
                                                { 2, 3, 4 }, { 2, 4, 5 }, { 3, 4, 5 } };
    const WORD32 ai4_offset_values_1B[7][2] = { { 1, 1 }, { 1, 2 }, { 1, 2 }, { 1, 3 },
                                                { 2, 3 }, { 3, 4 }, { 3, 5 } };
    const WORD32 ai4_offset_values_0B[7][1] = { { 0 }, { 1 }, { 2 }, { 2 }, { 3 }, { 3 }, { 4 } };

    /*The ratio is clipped between 16 and 2 to put it into bins*/

    CLIP(i4_ratio, 16, 2);

    for(i4_bin = 1; i4_bin < 5; i4_bin++)
    {
        if((i4_ratio >> i4_bin) == 1)
        {
            break;
        }
    }
    switch(i4_bin)
    {
    case(1):
        (f_ratio > 2.0f) ? (i4_bin = 3) : ((f_ratio > 1.33f) ? (i4_bin = 2) : (i4_bin = 1));
        break;
    case(2):
        i4_bin = 4;
        break;
    case(3):
        (f_ratio > 12.0f) ? (i4_bin = 6) : (i4_bin = 5);
        break;
    case(4):
        i4_bin = 7;
        break;
    }

    /*For the i4_bin == 1, actual ratio could be >2.0,>1.33 or lesser hence putting them into different bins*/

    trace_printf("1 bin %d", i4_bin);

    /*Total 7 bins hence the clip*/
    CLIP(i4_bin, 7, 1);

    *pi4_complexity_bin = i4_bin - 1;

    switch(i4_num_active_pic_type)
    {
    case 5:
        memmove(
            &ai4_offsets[1],
            ai4_offset_values_7B[i4_bin - 1],
            sizeof(ai4_offset_values_7B[i4_bin - 1]));
        break;
    case 4:
        memmove(
            &ai4_offsets[1],
            ai4_offset_values_3B[i4_bin - 1],
            sizeof(ai4_offset_values_3B[i4_bin - 1]));
        break;
    case 3:
        memmove(
            &ai4_offsets[1],
            ai4_offset_values_1B[i4_bin - 1],
            sizeof(ai4_offset_values_1B[i4_bin - 1]));
        break;
    case 2:
        memmove(
            &ai4_offsets[1],
            ai4_offset_values_0B[i4_bin - 1],
            sizeof(ai4_offset_values_0B[i4_bin - 1]));
        break;
    default:
        memmove(
            &ai4_offsets[1],
            ai4_offset_values_0B[i4_bin - 1],
            sizeof(ai4_offset_values_0B[i4_bin - 1]));
        break;
    }

    trace_printf(
        "Enc %d,%d,%d,%d,%d offsets",
        ai4_offsets[0],
        ai4_offsets[1],
        ai4_offsets[2],
        ai4_offsets[3],
        ai4_offsets[4]);
}

/*****************************************************************************
  Function Name : get_Kp_Kb
  Description   : Get the operating Kp and Kp so that scene cut sub gop can go
                  with similar qp offset
  Inputs        : ps_bit_allocation
                  e_pic_type
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/

WORD32 get_Kp_Kb(bit_allocation_t *ps_bit_allocation, picture_type_e e_pic_type)
{
    return ps_bit_allocation->i2_K[e_pic_type];
}
/*****************************************************************************
  Function Name : get_scene_change_tot_frm_bits
  Description   : Based on remaining bits in period and default I_TO_B complexity
                  total bit budget for scene cut frame is obtained.
  Inputs        : ps_bit_allocation - bit_allocation structure
                  ps_rd_model - rd model pointer (for all the frame types)
                  e_pic_type - picture type
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 get_scene_change_tot_frm_bits(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handling,
    cbr_buffer_handle ps_cbr_buf_handling,
    WORD32 i4_num_pixels,
    WORD32 i4_f_sim_lap,
    float i_to_avg_rest,
    WORD32 i4_call_type,
    WORD32 i4_non_I_scd,
    WORD32 i4_is_infinite_gop)
{
    WORD32 j;
    WORD32 i4_tot_bits_for_scd_frame;
    WORD32 i4_total_bits_in_period;
    //number_t avq_complexity_estimate[MAX_PIC_TYPE];
    WORD32 /* ai4_rem_frms_in_period[MAX_PIC_TYPE], */
        ai4_frms_in_period[MAX_PIC_TYPE];
    WORD32 i4_max_consumable_bits;
    WORD32 i4_intra_frm_int;
    WORD32 ai4_actual_frms_in_gop[MAX_PIC_TYPE], i, i4_total_frames = 0;
    float final_ratio, f_sim = (float)i4_f_sim_lap / 128;

    i4_intra_frm_int = pic_type_get_intra_frame_interval(ps_pic_handling);

    /* Get the rem_frms_in_gop & the frms_in_gop from the pic_type state struct */
    /* pic_type_get_rem_frms_in_gop(ps_pic_handling, ai4_rem_frms_in_period); */
    pic_type_get_frms_in_gop(ps_pic_handling, ai4_frms_in_period);

    /* Depending on the number of gops in a period, find the num_frms_in_prd */
    for(j = 0; j < MAX_PIC_TYPE; j++)
    {
        /* ai4_rem_frms_in_period[j] += (ai4_frms_in_period[j] * (ps_bit_allocation->i4_num_gops_in_period - 1)); */
        ai4_frms_in_period[j] *= ps_bit_allocation->i4_num_gops_in_period;
    }

    /* Remove the header bits from the remaining bits to find how many bits you
       can transfer.*/
    {
        i4_total_bits_in_period = ps_bit_allocation->s_rbip.i4_bits_per_frm *
                                  ps_bit_allocation->s_rbip.i4_tot_frms_in_gop;
        //trace_printf(" SCD_rbip = %d",i4_total_bits_in_period);
    }
    //since this marks end of previous GOP it is better to consider actual error than ps_bit_allocation->i4_frame_level_bit_error;

    {
        pic_type_get_actual_frms_in_gop(ps_pic_handling, ai4_actual_frms_in_gop);
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            i4_total_frames += ai4_frms_in_period[i];
        }
        i4_max_consumable_bits = ps_bit_allocation->i4_max_bits_per_frm[0] * i4_total_frames;
    }
    if(i4_total_bits_in_period > 0)
    {
        i4_total_bits_in_period = MIN(i4_total_bits_in_period, i4_max_consumable_bits);
    }
    final_ratio = i_to_avg_rest;
    /*If FSIM says the content is static (> 126 is assured to be static*/
    /*Very low FSIM safety check*/
    if(f_sim < 0.50 && final_ratio > 8)
        final_ratio = 8;
    /*Do not apply safety limits if second pass as data is reliable*/
    if(ps_bit_allocation->i4_ba_rc_pass != 2)
    {
        /*clip min max values*/
        if((i4_is_infinite_gop == 1) && (final_ratio > I_TO_AVG_REST_GOP_BIT_MAX_INFINITE))
        {
            final_ratio = I_TO_AVG_REST_GOP_BIT_MAX_INFINITE;
        }
        else
        {
            if(final_ratio > I_TO_AVG_REST_GOP_BIT_MAX)
                final_ratio = I_TO_AVG_REST_GOP_BIT_MAX;
        }
        if(final_ratio < I_TO_AVG_REST_GOP_BIT_MIN)
            final_ratio = I_TO_AVG_REST_GOP_BIT_MIN;
    }
    else
    {
        if(final_ratio > I_TO_AVG_REST_GOP_BIT_MAX_2_PASS)
            final_ratio = I_TO_AVG_REST_GOP_BIT_MAX_2_PASS;

        if(final_ratio < I_TO_AVG_REST_GOP_BIT_MIN_2_PASS)
            final_ratio = I_TO_AVG_REST_GOP_BIT_MIN_2_PASS;
    }

    /*based on offline runs to find I_BITS/(AVERAGE_CONSUMPTION_OF_REST_GOP)*/
    /*  BITS FOR I
        BITS = I_TO_AVG_REST_GOP * total_bits_period
                -------------------------------------
                N - (num_I_in_period) + (I_TO_AVG_REST_GOP * num_I_in_period)
                */
    i4_tot_bits_for_scd_frame = bit_alloc_get_intra_bits(
        ps_bit_allocation,
        ps_pic_handling,
        ps_cbr_buf_handling,
        I_PIC,
        NULL,
        1,
        final_ratio,
        i4_call_type,
        i4_non_I_scd,
        0.0f);
    ps_bit_allocation->i4_excess_bits_from_buffer = 0;

    if(i4_call_type == 1)
    {
        trace_printf("I_TO_AVG_REST_GOP_BIT used = %f\n", final_ratio);
        trace_printf(" SCD DETECTED   bits allocated = %d", i4_tot_bits_for_scd_frame);
    }

    /* If the remaining bits in the period becomes negative then the estimated texture
    bits would also become negative. This would send a feedback to the model which
    may go for a toss. Thus sending the minimum possible value = 0 */
    if(i4_tot_bits_for_scd_frame < 0)
        i4_tot_bits_for_scd_frame = 0;

    return (i4_tot_bits_for_scd_frame);
}

/*****************************************************************************
  Function Name : update_estimate_status
  Description   : Est texture bits in case of scene cut is obtained form offline
                  model. Update bit alloc
  Inputs        : ps_bit_allocation
                  e_pic_type
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/

void update_estimate_status(
    bit_allocation_t *ps_bit_allocation,
    WORD32 i4_est_texture_bits,
    WORD32 i4_hdr_bits,
    WORD32 i4_est_text_bits_ctr_get_qp)
{
    ps_bit_allocation->ai4_cur_frm_est_tex_bits[i4_est_text_bits_ctr_get_qp] = i4_est_texture_bits;
    ps_bit_allocation->ai4_cur_frm_est_hdr_bits[i4_est_text_bits_ctr_get_qp] = i4_hdr_bits;
}

/*****************************************************************************
  Function Name : bit_allocation_set_num_scd_lap_window
  Description   :
  Inputs        : ps_bit_allocation
                  i4_num_scd_in_lap_window
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void bit_allocation_set_num_scd_lap_window(
    bit_allocation_t *ps_bit_allocation,
    WORD32 i4_num_scd_in_lap_window,
    WORD32 i4_num_frames_b4_Scd)
{
    ps_bit_allocation->i4_num_scd_in_lap_window = i4_num_scd_in_lap_window;
    ps_bit_allocation->i4_num_frm_b4_scd = i4_num_frames_b4_Scd;
    /*To avoid trashing I frame badly due to back to back scene cut limit the increment in Ni*/
    if(ps_bit_allocation->i4_num_scd_in_lap_window > 2)
        ps_bit_allocation->i4_num_scd_in_lap_window = 2;
}
/*****************************************************************************
  Function Name : bit_allocation_set_sc_i_in_rc_look_ahead
  Description   :
  Inputs        : ps_bit_allocation
                  i4_next_sc_i_in_rc_look_ahead
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void bit_allocation_set_sc_i_in_rc_look_ahead(
    bit_allocation_t *ps_bit_allocation, WORD32 i4_next_sc_i_in_rc_look_ahead)
{
    ps_bit_allocation->i4_next_sc_i_in_rc_look_ahead = i4_next_sc_i_in_rc_look_ahead;
}
/*****************************************************************************
  Function Name : bit_allocation_update_gop_level_bit_error
  Description   :
  Inputs        : ps_bit_allocation
                  i4_error_bits
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void bit_allocation_update_gop_level_bit_error(
    bit_allocation_t *ps_bit_allocation, WORD32 i4_error_bits)
{
    ps_bit_allocation->i4_gop_level_bit_error += i4_error_bits;
    ps_bit_allocation->i4_frame_level_bit_error += i4_error_bits;
    /*Error is (rdopt - entropy) Hence for total bit consumption subtract error*/
    ps_bit_allocation->i8_cur_gop_bit_consumption -= i4_error_bits;
}

/******************************************************************************
  Function Name   : rc_update_bit_distribution_gop_level_2pass
  Description     : This function distributes the bits to all the gops depending
                    on the complexities and the error bits accumulated until now
  Arguments       : ps_rate_control_api - rate control api handle
                    i4_start_gop_number : GOP number from which distribution should happen
  Return Values   :
  Revision History:


 Assumptions    -

 Checks     -
*****************************************************************************/
void rc_update_bit_distribution_gop_level_2pass(
    bit_allocation_t *ps_bit_allocation,
    pic_handling_handle ps_pic_handle,
    void *pv_gop_stat,
    rc_type_e e_rc_type,
    WORD32 i4_num_gop,
    WORD32 i4_start_gop_number,
    float f_avg_qscale_first_pass,
    WORD32 i4_max_ebf,
    WORD32 i4_ebf,
    LWORD64 i8_tot_bits_sequence,
    WORD32 i4_comp_error)
{
    float cur_peak_factor, f_bits_per_frame;
    LWORD64 total_nbp_bits_allocated = 0;
    LWORD64 total_bp_bits_allocated = 0;
    LWORD64 total_bits_allocated = 0, prev_total_bits_allocated = -1;
    WORD32
    i4_num_loop_inter_GOP_alloc = 0, ai4_peak_bitrate[MAX_NUM_DRAIN_RATES] = { 0 },
    temp_i; /*Loop 20 times to meet precise bitrate, after that exit the loop and distribute remaining bits equally for all GOP*/
    gop_level_stat_t *ps_cur_gop;
    WORD32 i4_num_frames_in_gop, i4_cur_gop_num, i4_num_frm_with_rmax, i4_num_frm_with_rmin;
    LWORD64 i8_max_bit_for_gop, /*i8_min_bit_for_gop,*/ i8_peak_bitrate, i8_frame_rate,
        i8_current_bitrate = (LWORD64)ba_get_2pass_avg_bit_rate(ps_bit_allocation);
    LWORD64 i8_actual_avg_bit_rate = ba_get_bit_rate(ps_bit_allocation);
    LWORD64 i8_num_frame_remaining = 0, i8_excess_bits = 0;
    float min_complexity_beyond_peak /*,f_max_complexity = 1.0f,f_min_complexity = 0.0f*/
        ;  //The minimum complexity for which bit allocation exceeds peak rate but
    float f_avg_bits_complexity_based;
    WORD32 i4_num_gop_not_rmax;
    LWORD64 i8_bits_for_this_gop;

#define MAX_LOOP_INTER_GOP_ALLOC                                                                   \
    20 /*The below loop shall run maximum of this macro once it exits allocate the difference bits equally for all the GOPS*/

    i4_ebf = MAX(i4_ebf, 0);
    //i4_ebf = 0;
    if(i4_start_gop_number == 0)
    {
        cur_peak_factor = 7.0;
    }
    else
    {
        cur_peak_factor = ps_bit_allocation->f_cur_peak_factor_2pass;
    }
    /*Parsing of entire log file is done and summary of GOP level data has been updated in the temp,
    Iteratively allocate the bits to make it meet bitrate*/
    for(temp_i = i4_start_gop_number; temp_i < i4_num_gop; temp_i++)
    {
        ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + temp_i);
    }
    i8_frame_rate = ba_get_frame_rate(ps_bit_allocation);
    ba_get_peak_bit_rate(ps_bit_allocation, &ai4_peak_bitrate[0]);
    i8_peak_bitrate = (LWORD64)ai4_peak_bitrate[0];

    /*Modify the bitrate depending on the error bits and total bits*/
    //i8_current_bitrate = (LWORD64)((float)i8_tot_bits_sequence*i8_frame_rate/(1000*i8_num_frame_remaining));

    f_bits_per_frame = (float)i8_current_bitrate / i8_frame_rate * 1000;
    ps_bit_allocation->i8_current_bitrate_2_pass = i8_current_bitrate;
    //printf("\n%d current bitrate",i8_current_bitrate);

    do
    {
        /*Get gop level stat*/
        /*recalculate the bits based on new scaling factor*/
        total_bits_allocated = 0;
        total_bp_bits_allocated = 0;
        total_nbp_bits_allocated = 0;
        min_complexity_beyond_peak =
            (float)ps_bit_allocation->ai4_peak_bit_rate[0] / i8_current_bitrate;

        /*min_complexity_beyond_peak = ba_get_min_complexity_for_peak_br(ps_bit_allocation->ai4_peak_bit_rate[0],
                                            (WORD32)i8_current_bitrate,
                                            cur_peak_factor,
                                            f_max_complexity,
                                            f_min_complexity,
                                            ps_bit_allocation->i4_ba_rc_pass);*/

        for(i4_cur_gop_num = i4_start_gop_number; i4_cur_gop_num < i4_num_gop; i4_cur_gop_num++)
        {
            ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + i4_cur_gop_num);
            ps_cur_gop->f_bits_complexity_l1_based_peak_factor =
                ps_cur_gop->f_bits_complexity_l1_based * cur_peak_factor;
        }
        i4_num_frm_with_rmax = 0;
        i4_num_frm_with_rmin = 0;
        f_avg_bits_complexity_based = 0.0;
        i4_num_gop_not_rmax = 0;
        i8_num_frame_remaining = 0;
        for(i4_cur_gop_num = i4_start_gop_number; i4_cur_gop_num < i4_num_gop; i4_cur_gop_num++)
        {
            ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + i4_cur_gop_num);
            if(!ps_cur_gop->i4_peak_br_clip)
            {
                f_avg_bits_complexity_based +=
                    (ps_cur_gop->f_bits_complexity_l1_based * ps_cur_gop->i4_tot_frm_in_gop);
                i8_num_frame_remaining += ps_cur_gop->i4_tot_frm_in_gop;
                i4_num_gop_not_rmax++;
            }
        }
        f_avg_bits_complexity_based = (f_avg_bits_complexity_based / i8_num_frame_remaining);
        for(i4_cur_gop_num = i4_start_gop_number; i4_cur_gop_num < i4_num_gop; i4_cur_gop_num++)
        {
            /*Parse through all the GOP*/
            /*get current gop data*/
            //i4_num_frames_in_gop = 0;
            LWORD64 i8_avg_bit_rate_bits;
            LWORD64 i8_curr_bit_rate_bits;
            ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + i4_cur_gop_num);

            if(ps_cur_gop->i4_peak_br_clip)
            {
                i4_num_frm_with_rmax++;
                total_nbp_bits_allocated += ps_cur_gop->i8_bits_allocated_to_gop;
                continue;
            }
            ps_cur_gop->f_buffer_play_complexity = 0.;
            //ps_cur_gop->f_gop_level_complexity_sum = -1;
            //ps_cur_gop->i8_buffer_play_bits = 0;
            ps_cur_gop->i8_buffer_play_bits_allocated_to_gop = 0;
            i4_num_frames_in_gop = ps_cur_gop->i4_tot_frm_in_gop;

            if(i4_num_gop_not_rmax == i4_num_gop)
            {
                i8_bits_for_this_gop =
                    (LWORD64)((i8_current_bitrate * i4_num_frames_in_gop * 1000) / i8_frame_rate);
                if(e_rc_type == VBR_STREAMING)
                {
                    ps_cur_gop->i8_bits_allocated_to_gop = (LWORD64)(
                        (ps_cur_gop->f_bits_complexity_l1_based / (f_avg_bits_complexity_based)) *
                        i8_bits_for_this_gop);
                }
                else
                {
                    ps_cur_gop->i8_bits_allocated_to_gop =
                        (LWORD64)(i8_current_bitrate * i4_num_frames_in_gop / i8_frame_rate * 1000);
                }
            }
            else
            {
                //i8_bits_for_this_gop = (LWORD64)((i8_excess_bits * i4_num_frames_in_gop * 1000)/(i8_frame_rate*i4_num_gop_not_rmax));
                i8_bits_for_this_gop =
                    (LWORD64)((i8_excess_bits * i4_num_frames_in_gop) / (i8_num_frame_remaining));
                if(e_rc_type == VBR_STREAMING)
                {
                    ps_cur_gop->i8_bits_allocated_to_gop += (LWORD64)(
                        (ps_cur_gop->f_bits_complexity_l1_based / (f_avg_bits_complexity_based)) *
                        i8_bits_for_this_gop);
                }
                else
                {
                    ASSERT(0);
                }
            }
            ps_cur_gop->i8_actual_bits_allocated_to_gop = ps_cur_gop->i8_bits_allocated_to_gop;
            /*clip based on peak rate*/
            i8_max_bit_for_gop = i8_peak_bitrate * i4_num_frames_in_gop * 1000 / i8_frame_rate;
            ps_cur_gop->i8_max_bit_for_gop = i8_max_bit_for_gop;
            ps_cur_gop->i4_peak_br_clip = 0;
            if(ps_cur_gop->i8_bits_allocated_to_gop > i8_max_bit_for_gop)
            {
                ps_cur_gop->i8_bits_allocated_to_gop = i8_max_bit_for_gop;
                ps_cur_gop->i4_peak_br_clip = 1;
                i4_num_frm_with_rmax++;
                /*if(ps_cur_gop->f_bits_complexity_l1_based < min_complexity_beyond_peak)
                    min_complexity_beyond_peak = ps_cur_gop->f_bits_complexity_l1_based;*/
            }
            i8_curr_bit_rate_bits =
                (LWORD64)(i8_current_bitrate * i4_num_frames_in_gop / i8_frame_rate * 1000);
            i8_avg_bit_rate_bits =
                (LWORD64)(i8_actual_avg_bit_rate * i4_num_frames_in_gop / i8_frame_rate * 1000);
            ps_cur_gop->i4_is_below_avg_rate_gop_frame = 0;
            if(ps_cur_gop->i8_bits_allocated_to_gop <
               (MIN(i8_curr_bit_rate_bits, ps_cur_gop->i8_minimum_gop_bits)))
            {
                ps_cur_gop->i4_is_below_avg_rate_gop_frame = 1;
                ps_cur_gop->i8_bits_allocated_to_gop =
                    MIN(i8_curr_bit_rate_bits, ps_cur_gop->i8_minimum_gop_bits);
                i4_num_frm_with_rmin++;
            }
            total_nbp_bits_allocated += ps_cur_gop->i8_bits_allocated_to_gop;
        }
        i4_num_loop_inter_GOP_alloc++;
        /*check for tolerance of 0.5% in terms of meeting bitrate, terminate the loop when bitrate is met*/
        total_bits_allocated = total_nbp_bits_allocated + total_bp_bits_allocated;
        if((total_bits_allocated < (1.005 * i8_tot_bits_sequence) &&
            total_bits_allocated > (0.995 * i8_tot_bits_sequence)) ||
           (i4_num_loop_inter_GOP_alloc > MAX_LOOP_INTER_GOP_ALLOC) /*|| (cur_peak_factor <= 1 )*/)
        {
            float error_bits = ((float)i8_tot_bits_sequence - total_bits_allocated);
            WORD32 temp_i;
            float f_per_frm_bits = ((float)(i8_current_bitrate)) / (i8_frame_rate / 1000);
            //cur_peak_factor *= (float)i8_tot_bits_sequence/total_bits_allocated;
            if((i4_comp_error == 1) || ((i4_comp_error == 0) && (error_bits < 0)))
            {
                for(temp_i = i4_start_gop_number; temp_i < i4_num_gop; temp_i++)
                {
                    ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + temp_i);
                    ps_cur_gop->i8_bits_allocated_to_gop += (LWORD64)(
                        (error_bits * ps_cur_gop->i8_bits_allocated_to_gop / total_bits_allocated));
                }
            }
            for(temp_i = i4_start_gop_number; temp_i < i4_num_gop; temp_i++)
            {
                ps_cur_gop = (gop_level_stat_t *)((gop_level_stat_t *)pv_gop_stat + temp_i);
                ps_cur_gop->f_avg_complexity_factor = (ps_cur_gop->f_bits_complexity_l1_based /
                                                       ps_cur_gop->i8_bits_allocated_to_gop) *
                                                      (f_per_frm_bits) *
                                                      (ps_cur_gop->i4_tot_frm_in_gop);
            }
            break;
        }
        else
        {
            /*Go for next iteration*/
            cur_peak_factor *= (float)i8_tot_bits_sequence / total_bits_allocated;
            //cur_peak_factor = MAX(cur_peak_factor,1);
            prev_total_bits_allocated = total_bits_allocated;
            i8_excess_bits = i8_tot_bits_sequence - total_bits_allocated;
        }

    } while(1);
    ps_bit_allocation->f_cur_peak_factor_2pass = cur_peak_factor;
    ps_bit_allocation->i8_total_bits_allocated = total_bits_allocated;

    /*Store complexity beyond which bits are clipped to peak rate*/
    /*if(i4_start_gop_number == 0)*/
    {
        ps_bit_allocation->f_min_complexity_cross_peak_rate = /*min_complexity_beyond_peak*/
            (float)ps_bit_allocation->ai4_peak_bit_rate[0] / i8_current_bitrate;
        //ba_get_min_complexity_for_peak_br(ps_bit_allocation->ai4_peak_bit_rate[0],ps_bit_allocation->i4_bit_rate,cur_peak_factor,f_max_complexity,f_min_complexity,ps_bit_allocation->i4_ba_rc_pass);
    }
}

/*****************************************************************************
  Function Name : get_prev_frame_total_header_bits
  Description   :
  Inputs        : ps_bit_allocation
                  e_pic_type
  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void get_prev_frame_total_header_bits(
    bit_allocation_t *ps_bit_allocation,
    WORD32 *pi4_prev_frame_total_bits,
    WORD32 *pi4_prev_frame_header_bits,
    picture_type_e e_pic_type)
{
    *pi4_prev_frame_total_bits = ps_bit_allocation->ai4_prev_frm_tot_bits[e_pic_type];
    *pi4_prev_frame_header_bits = ps_bit_allocation->i4_prev_frm_header_bits[e_pic_type];
}

/*****************************************************************************
  Function Name : bit_alloc_get_gop_num
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
LWORD64 bit_alloc_get_gop_num(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i8_cur_gop_num);
}
/*****************************************************************************
  Function Name : ba_get_min_bits_per_frame
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_min_bits_per_frame(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i4_min_bits_per_frm);
}
/*****************************************************************************
  Function Name : set_bit_allocation_i_frames
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void set_bit_allocation_i_frames(
    bit_allocation_t *ps_bit_allocation,
    cbr_buffer_handle ps_cbr_buffer,
    pic_handling_handle ps_pic_handle,
    WORD32 i4_lap_window_comp,
    WORD32 i4_num_frames)
{
    LWORD64 vbv_buffer_based_excess = 0;
    WORD32 i4_gop_correction;
    WORD32 i4_lap_window_comp_temp = i4_lap_window_comp;
    rc_type_e e_rc_type = get_rc_type(ps_cbr_buffer);
    if(e_rc_type == VBR_STREAMING)
    {
        if(((float)i4_lap_window_comp / 128) > ps_bit_allocation->f_min_complexity_cross_peak_rate)
            i4_lap_window_comp_temp =
                (WORD32)(ps_bit_allocation->f_min_complexity_cross_peak_rate * 128);

        /*Get excess bits if any from vbv buffer*/
        vbv_buffer_based_excess = get_vbv_buffer_based_excess(
            ps_cbr_buffer,
            ps_bit_allocation->f_min_complexity_cross_peak_rate,
            ((float)i4_lap_window_comp / 128),
            i4_num_frames,
            1);
    }
    i4_gop_correction =
        get_error_bits_for_desired_buf(ps_cbr_buffer, i4_lap_window_comp_temp, i4_num_frames);

    update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handle, 0);

    set_rbip(&ps_bit_allocation->s_rbip, (i4_gop_correction + (WORD32)vbv_buffer_based_excess));

    update_rbip(&ps_bit_allocation->s_rbip, ps_pic_handle, 0);
}

/*****************************************************************************
  Function Name : bit_alloc_set_curr_i_to_sum_i
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void bit_alloc_set_curr_i_to_sum_i(bit_allocation_t *ps_bit_allocation, float f_curr_i_to_sum)
{
    ps_bit_allocation->f_curr_i_to_sum = f_curr_i_to_sum;
}

/*****************************************************************************
  Function Name : ba_set_gop_stat_in_bit_alloc
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_set_gop_stat_in_bit_alloc(bit_allocation_t *ps_bit_allocation, void *pv_gop_stat_summary)
{
    ps_bit_allocation->pv_gop_stat = pv_gop_stat_summary;
}
/*****************************************************************************
  Function Name : ba_get_luma_pels
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_luma_pels(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i4_luma_pels);
}
/*****************************************************************************
  Function Name : overflow_avoided_summation
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void overflow_avoided_summation(WORD32 *pi4_accumulator, WORD32 i4_input)
{
    if((pi4_accumulator[0] > 0) && (((int)0x7fffffff - pi4_accumulator[0]) < i4_input))
        pi4_accumulator[0] = 0x7fffffff;
    else if((pi4_accumulator[0] < 0) && (((int)0x80000000 - pi4_accumulator[0]) > i4_input))
        pi4_accumulator[0] = 0x80000000;
    else
        pi4_accumulator[0] += i4_input;
}
/*****************************************************************************
  Function Name : ba_get_sum_complexity_segment_cross_peak
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_get_sum_complexity_segment_cross_peak(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_sum_complexity_segment_cross_peak);
}
/*****************************************************************************
  Function Name : ba_get_prev_frame_tot_est_bits
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_prev_frame_tot_est_bits(bit_allocation_t *ps_bit_allocation, WORD32 i4_pic)
{
    return (ps_bit_allocation->ai4_prev_frm_tot_est_bits[i4_pic]);
}
/*****************************************************************************
  Function Name : ba_get_prev_frame_tot_bits
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_prev_frame_tot_bits(bit_allocation_t *ps_bit_allocation, WORD32 i4_pic)
{
    return (ps_bit_allocation->ai4_prev_frm_tot_bits[i4_pic]);
}
/*****************************************************************************
  Function Name : ba_gop_info_average_qscale_gop_without_offset
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_gop_info_average_qscale_gop_without_offset(bit_allocation_t *ps_bit_allocation)
{
    gop_level_stat_t *ps_gop_level_stat =
        (gop_level_stat_t *)ps_bit_allocation->pv_gop_stat + ps_bit_allocation->i8_cur_gop_num;

    return (ps_gop_level_stat->f_hbd_avg_q_scale_gop_without_offset);
}
/*****************************************************************************
  Function Name : ba_get_min_complexity_for_peak_br
  Description   : compute min complexity above which peak rate needs to be given
  Inputs        : i4_peak_bit_rate

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_get_min_complexity_for_peak_br(
    WORD32 i4_peak_bit_rate,
    WORD32 i4_bit_rate,
    float f_peak_rate_factor,
    float f_max_val,
    float f_min_val,
    WORD32 i4_pass)
{
    float f_target_bits_ratio = (float)i4_peak_bit_rate / i4_bit_rate;
    float f_at_min_val;
    float f_at_max_val;
    float f_avg_val, f_at_avg_val;
    WORD32 i4_iter = 0, i4_max_iter = 25;

    f_avg_val = (f_max_val + f_min_val) / 2;
    /*i4_target_bits_ratio = (-1.7561*(X*X*X*X) + ( 2.5547 * X * X * X) - 0.3408 * (X * X) + (0.5343 * X) - 0.003) * 10;*/
    if(i4_pass != 2)
    {
        f_at_min_val = COMP_TO_BITS_MAP(f_min_val, f_peak_rate_factor);
        f_at_max_val = COMP_TO_BITS_MAP(f_max_val, f_peak_rate_factor);
        f_at_avg_val = COMP_TO_BITS_MAP(f_avg_val, f_peak_rate_factor);
    }
    else
    {
        f_at_min_val = COMP_TO_BITS_MAP_2_PASS(f_min_val, f_peak_rate_factor);
        f_at_max_val = COMP_TO_BITS_MAP_2_PASS(f_max_val, f_peak_rate_factor);
        f_at_avg_val = COMP_TO_BITS_MAP_2_PASS(f_avg_val, f_peak_rate_factor);
    }

    do
    {
        if((f_at_min_val < f_target_bits_ratio) && (f_target_bits_ratio < f_at_avg_val))
        {
            f_max_val = f_avg_val;
        }
        else
        {
            f_min_val = f_avg_val;
        }
        f_avg_val = (f_max_val + f_min_val) / 2;

        /*i4_target_bits_ratio = (-1.7561*(X*X*X*X) + ( 2.5547 * X * X * X) - 0.3408 * (X * X) + (0.5343 * X) - 0.003) * 10;*/
        if(i4_pass != 2)
        {
            f_at_min_val = COMP_TO_BITS_MAP(f_min_val, f_peak_rate_factor);
            f_at_max_val = COMP_TO_BITS_MAP(f_max_val, f_peak_rate_factor);
            f_at_avg_val = COMP_TO_BITS_MAP(f_avg_val, f_peak_rate_factor);
        }
        else
        {
            f_at_min_val = COMP_TO_BITS_MAP_2_PASS(f_min_val, f_peak_rate_factor);
            f_at_max_val = COMP_TO_BITS_MAP_2_PASS(f_max_val, f_peak_rate_factor);
            f_at_avg_val = COMP_TO_BITS_MAP_2_PASS(f_avg_val, f_peak_rate_factor);
        }

        if(((fabs((float)(f_at_avg_val - f_target_bits_ratio))) <= .0001f) ||
           (i4_iter >= i4_max_iter))
        {
            break;
        }
        i4_iter++;
    } while(1);

    /*f_min_complexity_across_which pk br is given is unmapped value for 1 pass and mapped value for 2 pass*/
    if(i4_pass != 2)
        return (f_avg_val);
    else
        return (f_at_avg_val);
}
/*****************************************************************************
  Function Name : get_f_curr_by_sum_subgop
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float get_f_curr_by_sum_subgop(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_curr_by_sum_subgop);
}
/*****************************************************************************
  Function Name : ba_get_frame_number_in_gop
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_frame_number_in_gop(bit_allocation_t *ps_bit_allocation)
{
    return ((WORD32)(ps_bit_allocation->i8_frm_num_in_gop));
}
/*****************************************************************************
  Function Name : ba_get_qscale_max_clip_in_second_pass
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_get_qscale_max_clip_in_second_pass(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_qscale_max_clip_in_second_pass);
}
/*****************************************************************************
  Function Name : ba_set_avg_qscale_first_pass
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_set_avg_qscale_first_pass(
    bit_allocation_t *ps_bit_allocation, float f_average_qscale_1st_pass)
{
    ps_bit_allocation->f_average_qscale_1st_pass = f_average_qscale_1st_pass;
}
/*****************************************************************************
  Function Name : ba_set_max_avg_qscale_first_pass
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_set_max_avg_qscale_first_pass(
    bit_allocation_t *ps_bit_allocation, float f_average_qscale_1st_pass)
{
    ps_bit_allocation->f_max_average_qscale_1st_pass = f_average_qscale_1st_pass;
}
/*****************************************************************************
  Function Name : ba_get_avg_qscale_first_pass
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_get_avg_qscale_first_pass(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_average_qscale_1st_pass);
}
/*****************************************************************************
  Function Name : ba_get_max_avg_qscale_first_pass
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
float ba_get_max_avg_qscale_first_pass(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->f_max_average_qscale_1st_pass);
}
/*****************************************************************************
  Function Name : bit_alloc_set_2pass_total_frames
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void bit_alloc_set_2pass_total_frames(
    bit_allocation_t *ps_bit_allocation, WORD32 i4_total_2pass_frames)
{
    ps_bit_allocation->i4_total_2pass_frames = i4_total_2pass_frames;
}
/*****************************************************************************
  Function Name : ba_get_2pass_total_frames
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
WORD32 ba_get_2pass_total_frames(bit_allocation_t *ps_bit_allocation)
{
    return (ps_bit_allocation->i4_total_2pass_frames);
}
/*****************************************************************************
  Function Name : ba_set_enable_look_ahead
  Description   :
  Inputs        : ps_bit_allocation

  Revision History:
         DD MM YYYY   Author(s)       Changes (Describe the changes made)
*****************************************************************************/
void ba_set_enable_look_ahead(bit_allocation_t *ps_bit_allocation, WORD32 i4_fp_bit_alloc_in_sp)
{
    ps_bit_allocation->i4_fp_bit_alloc_in_sp = i4_fp_bit_alloc_in_sp;
}
