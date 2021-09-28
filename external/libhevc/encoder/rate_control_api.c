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
* \file rate_control_api.c
*
* \brief
*    This file contain rate control API functions
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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* User include files */
#include "ittiam_datatypes.h"
/* Lower module include files. These inclusion can be removed by having
   fwd declaration for each and every module*/
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "mem_req_and_acq.h"
#include "var_q_operator.h"
#include "rc_rd_model.h"
#include "est_sad.h"
#include "fixed_point_error_bits.h"
#include "vbr_storage_vbv.h"
#include "picture_type.h"
#include "cbr_buffer_control.h"
#include "bit_allocation.h"
#include "mb_model_based.h"
#include "vbr_str_prms.h"
#include "init_qp.h"
#include "rc_sad_acc.h"
#include "rc_frame_info_collector.h"
#include "rate_control_api.h"
#include "rate_control_api_structs.h"
#include "trace_support.h"

/** Macros **/
#define MIN(x, y) ((x) < (y)) ? (x) : (y)
#define MAX(x, y) ((x) < (y)) ? (y) : (x)
#define CLIP3RC(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))

#define DEV_Q 4 /*Q format(Shift) for Deviation range factor */
#define HI_DEV_FCTR 26  //23//32  /* 1.4*16 */
#define LO_DEV_B_FCTR 10  //temp change to avoid stuffing12  /* 0.75*16 */
#define LO_DEV_FCTR_1B 14  //8   /* 0.75*16 */
//#define LO_DEV_FCTR_7B     10//8   /* 0.75*16 */
#define LO_DEV_FCTR_3B 12  //8   /* 0.75*16 */
#define LO_DEV_FCTR_7B 12  //8   /* 0.75*16 */
#define GET_HI_DEV_QP(Qprev) ((((WORD32)Qprev) * HI_DEV_FCTR + (1 << (DEV_Q - 1))) >> DEV_Q)

#define GET_LO_DEV_QP(Qprev, i4_num_active_pic_types)((i4_num_active_pic_types <= B1_PIC)?(((((WORD32) Qprev)*LO_DEV_FCTR_1B + (1<<(DEV_Q-1)))>>DEV_Q): \
                                                        ((i4_num_active_pic_types == B2_PIC)? ((((WORD32) Qprev)*LO_DEV_FCTR_3B + (1<<(DEV_Q-1)))>>DEV_Q) \
                                                                                            ((((WORD32) Qprev)*LO_DEV_FCTR_7B + (1<<(DEV_Q-1)))>>DEV_Q))))

#define GET_LO_DEV_QP_B(Qprev) ((((WORD32)Qprev) * LO_DEV_B_FCTR + (1 << (DEV_Q - 1))) >> DEV_Q)
#define CLIP_QP(Qc, hi_d, lo_d) (((Qc) < (lo_d)) ? ((lo_d)) : (((Qc) > (hi_d)) ? (hi_d) : (Qc)))

/*below macors are used when qp is already in q format so adding 0.5 for rounding is not required*/
#define GET_HI_DEV_QP_QFAC(Qprev) ((((WORD32)Qprev) * HI_DEV_FCTR) >> DEV_Q)
#define GET_LO_DEV_QP_QFAC(Qprev, i4_num_active_pic_types)                                         \
    ((i4_num_active_pic_types <= B1_PIC)                                                           \
         ? ((((WORD32)Qprev) * LO_DEV_FCTR_1B) >> DEV_Q)                                           \
         : ((i4_num_active_pic_types == B2_PIC) ? ((((WORD32)Qprev) * LO_DEV_FCTR_3B) >> DEV_Q)    \
                                                : ((((WORD32)Qprev) * LO_DEV_FCTR_7B) >> DEV_Q)))

#define GET_LO_DEV_QP_QFAC_B_PIC(Qprev) ((((WORD32)Qprev) * LO_DEV_FCTR_3B) >> DEV_Q)

#define GET_LO_DEV_QP_B_QFAC(Qprev) ((((WORD32)Qprev) * LO_DEV_B_FCTR) >> DEV_Q)

#define P_TO_I_RATIO_Q_FACTOR (9)
#define MULT_FACTOR_SATD (4)
#define GET_L0_SATD_BY_ACT_MAX_PER_PIXEL(i4_num_pixel)                                             \
    ((5.4191f * i4_num_pixel + 4000000.0f) / i4_num_pixel)
#define GET_WEIGH_FACTOR_FOR_MIN_SCD_Q_SCALE(normal_satd_act, f_satd_by_Act_norm)                  \
    (MULT_FACTOR_SATD * normal_satd_act + f_satd_by_Act_norm) /                                    \
        (normal_satd_act + MULT_FACTOR_SATD * f_satd_by_Act_norm)

void SET_NETRA_TRACE(UWORD8 tag[], WORD32 value);
#define NETRA_TRACE (0)
#if NETRA_TRACE
#else
#define SET_NETRA_TRACE(x, y)
#endif

/*****************************************************************************/
/* Restricts the quantisation parameter variation within delta */
/*****************************************************************************/
/* static WORD32 restrict_swing(WORD32 cur_qp, WORD32 prev_qp, WORD32 delta_qp)
{
    if((cur_qp) - (prev_qp) > (delta_qp)) (cur_qp) = (prev_qp) + (delta_qp) ;
    if((prev_qp) - (cur_qp) > (delta_qp)) (cur_qp) = (prev_qp) - (delta_qp) ;
    return cur_qp;
}*/

/*****************************************************************************
Function Name : rate_control_get_init_free_memtab
Description   : Takes or gives memtab
Inputs        : pps_rate_control_api -  pointer to RC api pointer
                ps_memtab            -  Memtab pointer
                i4_use_base          -  Set during init, else 0
                i4_fill_base         -  Set during free, else 0
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
#if NON_STEADSTATE_CODE
WORD32 rate_control_num_fill_use_free_memtab(
    rate_control_handle *pps_rate_control_api, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0, i;
    static rate_control_api_t s_temp_rc_api;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_rate_control_api) = &s_temp_rc_api;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx],
            sizeof(rate_control_api_t),
            MEM_TAB_ALIGNMENT,
            PERSISTENT,
            DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_rate_control_api, e_func_type);
    }
    i4_mem_tab_idx++;

    /* Get the memory requirement of lower modules */
    i4_mem_tab_idx += bit_allocation_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_bit_allocation, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += cbr_buffer_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_cbr_buffer, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += est_sad_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_est_sad, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += mbrc_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_mb_rate_control, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += vbr_vbv_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_vbr_storage_vbv, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += init_qp_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_init_qp, &ps_memtab[i4_mem_tab_idx], e_func_type);

    i4_mem_tab_idx += sad_acc_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_sad_acc, &ps_memtab[i4_mem_tab_idx], e_func_type);

    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        i4_mem_tab_idx += rc_rd_model_num_fill_use_free_memtab(
            &pps_rate_control_api[0]->aps_rd_model[i], &ps_memtab[i4_mem_tab_idx], e_func_type);
    }
    i4_mem_tab_idx += pic_handling_num_fill_use_free_memtab(
        &pps_rate_control_api[0]->ps_pic_handling, &ps_memtab[i4_mem_tab_idx], e_func_type);
    return (i4_mem_tab_idx);
}

/*****************************************************************************
Function Name : initialise_rate_control
Description   : Initialise the rate control structure
Inputs        : ps_rate_control_api     - api struct
                e_rate_control_type     - VBR, CBR (NLDRC/LDRC), VBR_STREAMING
                u1_is_mb_level_rc_on        - enabling mb level RC
                u4_avg_bit_rate         - bit rate to achieved across the entire file size
                u4_peak_bit_rate            - max possible drain rate
                u4_frame_rate               - number of frames in 1000 seconds
                u4_intra_frame_interval - num frames between two I frames
                *au1_init_qp             - init_qp for I,P,B


Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void initialise_rate_control(
    rate_control_api_t *ps_rate_control_api,
    rc_type_e e_rate_control_type,
    UWORD8 u1_is_mb_level_rc_on,
    UWORD32 u4_avg_bit_rate,
    UWORD32 *pu4_peak_bit_rate,
    UWORD32 u4_min_bit_rate,
    UWORD32 u4_frame_rate,
    UWORD32 u4_max_delay,
    UWORD32 u4_intra_frame_interval,
    UWORD32 u4_idr_period,
    WORD32 *pi4_init_qp,
    UWORD32 u4_max_vbv_buff_size,
    WORD32 i4_max_inter_frm_int,
    WORD32 i4_is_gop_closed,
    WORD32 *pi4_min_max_qp,
    WORD32 i4_use_est_intra_sad,
    UWORD32 u4_src_ticks,
    UWORD32 u4_tgt_ticks,
    WORD32 i4_frame_height,
    WORD32 i4_frame_width,
    WORD32 i4_num_active_pic_type,
    WORD32 i4_field_pic,
    WORD32 i4_quality_preset,
    WORD32 i4_lap_window,
    WORD32 i4_initial_decoder_delay_frames,
    float f_max_peak_rate_sustain_dur,
    LWORD64 i8_num_frames_to_encode,
    UWORD32 u4_min_scd_hevc_qp,
    UWORD8 u1_bit_depth,
    FILE *pf_rc_stat_file,
    WORD32 i4_pass_num,
    void *pv_gop_stat,
    LWORD64 i8_num_gop_mem_alloc,
    WORD32 i4_is_infinite_gop,
    WORD32 i4_size_of_lap_out,
    WORD32 i4_size_of_rc_lap_out,
    void *pv_sys_rc_api,
    WORD32 i4_fp_bit_alloc_in_sp,
    WORD32 i4_num_frame_parallel,
    WORD32 i4_capped_vbr_flag)
{
    WORD32 i, i4_temp;
    UWORD32 u4_frms_in_delay_prd = (u4_frame_rate * u4_max_delay) / 1000000;
    UWORD32 i4_cbr_bit_alloc_period;
    float f_bit_depth_based_max_qp;
    UWORD32 u4_bit_depth_based_max_qp;
    WORD32 i4_pels_in_frame = (3 * (i4_frame_height * i4_frame_width) >> 1);

    if(u4_intra_frame_interval ==
       1) /*i_only: Set bit allocation period to 15( currently not configurable) for i only mode*/
    {
        i4_cbr_bit_alloc_period = u4_frame_rate / 1000; /*changed */
    }
    else
    {
        i4_cbr_bit_alloc_period = 1;
    }

    if(CBR_NLDRC_HBR == e_rate_control_type)
    {
        e_rate_control_type = CBR_NLDRC;
        ps_rate_control_api->i4_is_hbr = 1;
    }
    else
    {
        ps_rate_control_api->i4_is_hbr = 0;
    }
    ps_rate_control_api->e_rc_type = e_rate_control_type;
    ps_rate_control_api->i4_capped_vbr_flag = i4_capped_vbr_flag;
    ps_rate_control_api->u1_is_mb_level_rc_on = u1_is_mb_level_rc_on;
    ps_rate_control_api->i4_num_active_pic_type = i4_num_active_pic_type;
    ps_rate_control_api->i4_quality_preset = i4_quality_preset;
    ps_rate_control_api->i4_scd_I_frame_estimated_tot_bits = 0;
    ps_rate_control_api->i4_I_frame_qp_model = 0;
    ps_rate_control_api->u4_min_scd_hevc_qp = u4_min_scd_hevc_qp;
    ps_rate_control_api->pf_rc_stat_file = pf_rc_stat_file;
    ps_rate_control_api->i4_rc_pass = i4_pass_num;
    ps_rate_control_api->i4_max_frame_height = i4_frame_height;
    ps_rate_control_api->i4_max_frame_width = i4_frame_width;
    ps_rate_control_api->i4_underflow_warning = 0;
    ps_rate_control_api->f_p_to_i_comp_ratio = 1.0f;
    ps_rate_control_api->i4_scd_in_period_2_pass = 0;
    ps_rate_control_api->i4_is_infinite_gop = i4_is_infinite_gop;
    ps_rate_control_api->i4_frames_since_last_scd = 0;
    ps_rate_control_api->i4_num_frame_parallel = i4_num_frame_parallel;

    /*The memory for gop level summary struct is stored only for 2 pass*/
    if(i4_pass_num == 2)
    {
        ps_rate_control_api->pv_2pass_gop_summary = pv_gop_stat;
    }
    else
    {
        ps_rate_control_api->pv_2pass_gop_summary = NULL;
    }
    /*Initialize the call back funcitons for file related operations*/
    ps_rate_control_api->pv_rc_sys_api = pv_sys_rc_api;

    ps_rate_control_api->u1_bit_depth = u1_bit_depth;

    f_bit_depth_based_max_qp = (float)((51 + (6 * (u1_bit_depth - 8))) - 4) / 6;
    u4_bit_depth_based_max_qp = (UWORD32)pow(2.0f, f_bit_depth_based_max_qp);

    ps_rate_control_api->u4_bit_depth_based_max_qp = u4_bit_depth_based_max_qp;

    trace_printf("RC type = %d\n", e_rate_control_type);

    /* Set the avg_bitrate_changed flag for each pic_type to 0 */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->au1_avg_bitrate_changed[i] = 0;
    }

    /* Initialize the pic_handling module */
    init_pic_handling(
        ps_rate_control_api->ps_pic_handling, /*ps_pic_handling*/
        (WORD32)u4_intra_frame_interval, /*i4_intra_frm_int,*/
        i4_max_inter_frm_int, /*i4_max_inter_frm_int,*/
        i4_is_gop_closed,
        (WORD32)u4_idr_period,
        ps_rate_control_api->i4_num_active_pic_type,
        i4_field_pic); /*gop_struct_e*/

    /* initialise the init Qp module */
    init_init_qp(
        ps_rate_control_api->ps_init_qp,
        pi4_min_max_qp,
        i4_pels_in_frame,
        ps_rate_control_api->i4_is_hbr);

    /*** Initialize the rate control modules  ***/
    if(ps_rate_control_api->e_rc_type != CONST_QP)
    {
        UWORD32 au4_num_pics_in_delay_prd[MAX_PIC_TYPE] = { 0 };

        /* Initialise the model parameter structures */
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            init_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[i], MAX_FRAMES_MODELLED);
        }

        /* Initialize the buffer mechanism */
        if((ps_rate_control_api->e_rc_type == VBR_STORAGE) ||
           (ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP))
        {
            /* Assuming both the peak bit rates are same for a VBR_STORAGE and
            VBR_STORAGE_DVD_COMP */
            if(pu4_peak_bit_rate[0] != pu4_peak_bit_rate[1])
            {
                trace_printf("For VBR_STORAGE and VBR_STORAGE_DVD_COMP the peak bit "
                             "rates should be same\n");
            }
            init_vbr_vbv(
                ps_rate_control_api->ps_vbr_storage_vbv,
                (WORD32)pu4_peak_bit_rate[0],
                (WORD32)u4_frame_rate,
                (WORD32)u4_max_vbv_buff_size);
        }
        else if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
        {
            UWORD32 u4_avg_bit_rate_copy[MAX_NUM_DRAIN_RATES];
            for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
            {
                u4_avg_bit_rate_copy[i] = u4_avg_bit_rate;
            }

            init_cbr_buffer(
                ps_rate_control_api->ps_cbr_buffer,
                u4_max_delay,
                u4_frame_rate,
                u4_avg_bit_rate,
                au4_num_pics_in_delay_prd,
                u4_max_vbv_buff_size,
                u4_intra_frame_interval,
                ps_rate_control_api->e_rc_type,
                pu4_peak_bit_rate[0],
                i4_initial_decoder_delay_frames,
                f_max_peak_rate_sustain_dur,
                i8_num_frames_to_encode,
                i4_max_inter_frm_int,
                i4_pass_num,
                0 /*capped vbr off */);
        }
        else if(ps_rate_control_api->e_rc_type == VBR_STREAMING)
        {
            init_vbv_str_prms(
                &ps_rate_control_api->s_vbr_str_prms,
                u4_intra_frame_interval,
                u4_src_ticks,
                u4_tgt_ticks,
                u4_frms_in_delay_prd);

            init_cbr_buffer(
                ps_rate_control_api->ps_cbr_buffer,
                u4_max_delay,
                u4_frame_rate,
                u4_avg_bit_rate,
                au4_num_pics_in_delay_prd,
                u4_max_vbv_buff_size,
                u4_intra_frame_interval,
                ps_rate_control_api->e_rc_type,
                pu4_peak_bit_rate[0],
                i4_initial_decoder_delay_frames,
                f_max_peak_rate_sustain_dur,
                i8_num_frames_to_encode,
                i4_max_inter_frm_int,
                i4_pass_num,
                ps_rate_control_api->i4_capped_vbr_flag);
        }

        /* Initalise the SAD estimation module */
        init_est_sad(ps_rate_control_api->ps_est_sad, i4_use_est_intra_sad);

        /* Initialise the bit allocation module according to VBR or CBR */
        if((ps_rate_control_api->e_rc_type == VBR_STORAGE) ||
           (ps_rate_control_api->e_rc_type == VBR_STREAMING) ||
           (ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP))
        {
            /*UWORD32 u4_scaled_avg_bit_rate;*/
            /*X_PROD_Y_DIV_Z (u4_avg_bit_rate,1126,1024,u4_scaled_avg_bit_rate);*/
            init_bit_allocation(
                ps_rate_control_api->ps_bit_allocation,
                ps_rate_control_api->ps_pic_handling,
                i4_cbr_bit_alloc_period,
                u4_avg_bit_rate /*u4_scaled_avg_bit_rate*/,
                u4_frame_rate,
                (WORD32 *)pu4_peak_bit_rate,
                u4_min_bit_rate,
                i4_pels_in_frame,
                ps_rate_control_api->i4_is_hbr,
                ps_rate_control_api->i4_num_active_pic_type,
                i4_lap_window,
                i4_field_pic,
                i4_pass_num,
                (i4_frame_height * i4_frame_width),
                i4_fp_bit_alloc_in_sp);
        }
        else if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
        {
            init_bit_allocation(
                ps_rate_control_api->ps_bit_allocation,
                ps_rate_control_api->ps_pic_handling,
                i4_cbr_bit_alloc_period,  //i_onlyCBR_BIT_ALLOC_PERIOD,
                u4_avg_bit_rate,
                u4_frame_rate,
                (WORD32 *)pu4_peak_bit_rate,
                u4_min_bit_rate,
                i4_pels_in_frame,
                ps_rate_control_api->i4_is_hbr,
                ps_rate_control_api->i4_num_active_pic_type,
                i4_lap_window,
                i4_field_pic,
                i4_pass_num,
                (i4_frame_height * i4_frame_width),
                i4_fp_bit_alloc_in_sp);
        }
    }
    else
    {
        UWORD32 au4_num_pics_in_delay_prd[MAX_PIC_TYPE];

        for(i = 0; i < MAX_PIC_TYPE; i++)
            au4_num_pics_in_delay_prd[i] = 0;

        init_cbr_buffer(
            ps_rate_control_api->ps_cbr_buffer,
            u4_max_delay,
            u4_frame_rate,
            u4_avg_bit_rate,
            au4_num_pics_in_delay_prd,
            u4_max_vbv_buff_size,
            u4_intra_frame_interval,
            ps_rate_control_api->e_rc_type,
            pu4_peak_bit_rate[0],
            i4_initial_decoder_delay_frames,
            f_max_peak_rate_sustain_dur,
            i8_num_frames_to_encode,
            i4_max_inter_frm_int,
            i4_pass_num,
            0 /*capped vbr off */);
    }

    /* Initialize the init_qp */
    for(i4_temp = 0; i4_temp < MAX_SCENE_NUM_RC; i4_temp++)
    {
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_temp][i] = 0x7FFFFFFF;  //pi4_init_qp[i];
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_temp][i] =
                0x7FFFFFFF;  //pi4_init_qp[i] << QSCALE_Q_FAC;
            ps_rate_control_api->ai4_min_qp[i] = pi4_min_max_qp[(i << 1)];
            ps_rate_control_api->ai4_max_qp[i] = pi4_min_max_qp[(i << 1) + 1];
        }
    }
    /*init min and max qp in qscale*/
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->ai4_min_qp_q6[i] = MIN_QSCALE_Q6;
        //ps_rate_control_api->ai4_max_qp_q6[i] = (228 << QSCALE_Q_FAC);
        ps_rate_control_api->ai4_max_qp_q6[i] = (u4_bit_depth_based_max_qp << QSCALE_Q_FAC);
    }

    /* Initialize the is_first_frm_encoded */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->au1_is_first_frm_coded[i] = 0;
    }
    ps_rate_control_api->u1_is_first_frm = 1;
    ps_rate_control_api->i4_prev_ref_is_scd = 0;

    for(i = 0; i < MAX_NUM_FRAME_PARALLEL; i++)
    {
        ps_rate_control_api->ai4_est_tot_bits[i] =
            get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
    }

    /* Control flag for delayed impact after a change in peak bitrate has been made */
    ps_rate_control_api->u4_frms_in_delay_prd_for_peak_bit_rate_change = 0;
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        ps_rate_control_api->au4_new_peak_bit_rate[i] = pu4_peak_bit_rate[i];
    }

    /* initialise the mb level rate control module */
    init_mb_level_rc(ps_rate_control_api->ps_mb_rate_control);
    ps_rate_control_api->i4_prev_frm_est_bits = u4_avg_bit_rate / (u4_frame_rate / 1000);

    ps_rate_control_api->prev_ref_pic_type = I_PIC;
    ps_rate_control_api->i4_P_to_I_ratio = (1 << (P_TO_I_RATIO_Q_FACTOR + K_Q)) / I_TO_P_RATIO;

    /* Initialise sad accumulator */
    init_sad_acc(ps_rate_control_api->ps_sad_acc);

    rc_get_max_hme_sad_per_pixel(ps_rate_control_api, i4_frame_height * i4_frame_width);
}
#endif /* #if NON_STEADSTATE_CODE */

/****************************************************************************
*Function Name : add_picture_to_stack
*Description   : calls add_pic_to_stack
*Inputs        :
*Globals       :
*Processing    :
*Outputs       :
*Returns       :
*Issues        :
*Revision History:d
*DD MM YYYY   Author(s)       Changes (Describe the changes made)
*
*****************************************************************************/
void add_picture_to_stack(
    rate_control_api_t *rate_control_api, WORD32 i4_enc_pic_id, WORD32 i4_rc_in_pic)
{
    /* Call the routine to add the pic to stack in encode order */
    add_pic_to_stack(rate_control_api->ps_pic_handling, i4_enc_pic_id, i4_rc_in_pic);
}

/****************************************************************************
Function Name : add_picture_to_stack_re_enc
Description   :
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void add_picture_to_stack_re_enc(
    rate_control_api_t *rate_control_api, WORD32 i4_enc_pic_id, picture_type_e e_pic_type)
{
    /* In case of a re-encoder, the pics will come in the encode order itself.
       So, there is no need to buffer the pics up */
    add_pic_to_stack_re_enc(rate_control_api->ps_pic_handling, i4_enc_pic_id, e_pic_type);
}

/****************************************************************************
Function Name : get_picture_details
Description   : decides the picture type based on the state
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void get_picture_details(
    rate_control_handle rate_control_api,
    WORD32 *pi4_pic_id,
    WORD32 *pi4_pic_disp_order_no,
    picture_type_e *pe_pic_type,
    WORD32 *pi4_is_scd)
{
    /* Call to get the pic_details */
    get_pic_from_stack(
        rate_control_api->ps_pic_handling,
        pi4_pic_id,
        pi4_pic_disp_order_no,
        pe_pic_type,
        pi4_is_scd);
}

/****************************************************************************
Function Name : get_min_max_bits_based_on_buffer
Description   :
Inputs        : ps_rate_control_api

Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
static void get_min_max_bits_based_on_buffer(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 *pi4_min_bits,
    WORD32 *pi4_max_bits,
    WORD32 i4_get_error)
{
    WORD32 i4_min_bits = 0, i4_max_bits = 0;

    cbr_modify_ebf_estimate(ps_rate_control_api->ps_cbr_buffer, i4_get_error);  //ELP_RC

    /* Find the min and max bits that can be consumed based on the buffer condition */
    if(ps_rate_control_api->e_rc_type == VBR_STORAGE)
    {
        i4_max_bits = get_max_target_bits(ps_rate_control_api->ps_vbr_storage_vbv);
    }
    else if(ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP)
    {
        WORD32 i4_rem_bits_in_gop, i4_rem_frms_in_gop;
        /* WORD32 ai4_rem_frms_in_gop[MAX_PIC_TYPE]; */
        i4_rem_frms_in_gop = pic_type_get_rem_frms_in_gop(ps_rate_control_api->ps_pic_handling);
        i4_rem_bits_in_gop = rc_get_rem_bits_in_period(ps_rate_control_api);

        i4_max_bits = get_max_tgt_bits_dvd_comp(
            ps_rate_control_api->ps_vbr_storage_vbv,
            i4_rem_bits_in_gop,
            i4_rem_frms_in_gop,
            e_pic_type);
    }
    else if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
    {
        cbr_buffer_constraint_check(
            ps_rate_control_api->ps_cbr_buffer, 0, e_pic_type, &i4_max_bits, &i4_min_bits);
    }
    else /* if(ps_rate_control_api->e_rc_type == VBR_STREAMING) */
    {
        vbr_stream_buffer_constraint_check(
            ps_rate_control_api->ps_cbr_buffer, 0, e_pic_type, &i4_max_bits, &i4_min_bits);
    }
    /* Fill the min and max bits consumed */
    if(1 != ps_rate_control_api->i4_capped_vbr_flag)
    {
        pi4_min_bits[0] = i4_min_bits;
    }
    else
    {
        /* Capped VBR case */
        pi4_min_bits[0] = 0;
    }
    pi4_max_bits[0] = i4_max_bits;
}

/****************************************************************************
Function Name : is_first_frame_coded
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 is_first_frame_coded(rate_control_handle ps_rate_control_api)
{
    WORD32 i4_is_first_frame_coded = 1, i;
    /* Check whether atleast one frame of a each picture type gets encoded */
    /* Check whether it is an IPP or IPB kind of encoding */
    if(pic_type_get_intra_frame_interval(ps_rate_control_api->ps_pic_handling) == 1)
    {
        i4_is_first_frame_coded = ps_rate_control_api->au1_is_first_frm_coded[I_PIC];
    }
    else /*HEVC_hierarchy*/
    {
        if(pic_type_get_field_pic(ps_rate_control_api->ps_pic_handling))
        {
            i4_is_first_frame_coded &= ps_rate_control_api->au1_is_first_frm_coded[I_PIC];

            for(i = 1; i < ps_rate_control_api->i4_num_active_pic_type; i++)
            {
                i4_is_first_frame_coded &= ps_rate_control_api->au1_is_first_frm_coded[i];
                i4_is_first_frame_coded &=
                    ps_rate_control_api->au1_is_first_frm_coded[i + FIELD_OFFSET];
            }
        }
        else
        {
            for(i = 0; i < ps_rate_control_api->i4_num_active_pic_type; i++)
            {
                i4_is_first_frame_coded &= ps_rate_control_api->au1_is_first_frm_coded[i];
            }
        }
    }

    return i4_is_first_frame_coded;
}

/****************************************************************************
Function Name : get_min_max_qp
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/

static void get_min_max_qp(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 *pi4_hi_dev_qp_q6,
    WORD32 *pi4_lo_dev_qp_q6,
    WORD32 i4_complexity_bin,
    WORD32 i4_scene_num)
{
    WORD32 prev_qp_q6, prev_I_qp_q6;
    WORD32 hi_dev_qp_q6, lo_dev_qp_q6, hi_dev_qp_temp_q6;
    WORD32 i4_intra_frm_int, prev_qp_for_high_dev_q6,
        use_I_frame_qp_high_dev = 0; /*i_only : to detect i only case*/
    float per_pixel_p_hme_sad =
        (float)ps_rate_control_api->i8_per_pixel_p_frm_hme_sad_q10 / (1 << 10);

    i4_intra_frm_int = pic_type_get_intra_frame_interval(ps_rate_control_api->ps_pic_handling);

    /* Restricting the Quant swing */
    prev_qp_q6 = ps_rate_control_api
                     ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type];
    prev_qp_for_high_dev_q6 = prev_qp_q6;
    prev_I_qp_q6 = ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC];
    if(ps_rate_control_api->prev_ref_pic_type != e_pic_type)
    {
        if(e_pic_type == I_PIC)
        {
            /* Constrain I-frame QP to be within specified limit of prev_ref_qp/Kp */
            // SS - suppressing this assuming re-encode will take care
            /* prev_qp = i4_frame_qp; */
            prev_qp_q6 = (ps_rate_control_api->i4_P_to_I_ratio * (LWORD64)prev_qp_q6) >>
                         P_TO_I_RATIO_Q_FACTOR;
        }
        else if(e_pic_type == P_PIC || e_pic_type == P1_PIC)
        {
            /* Constrain P-frame QP to be within specified limit of Kp*prev_ref_qp */
            prev_qp_q6 = (I_TO_P_RATIO * (LWORD64)prev_qp_q6) >> K_Q;
            use_I_frame_qp_high_dev = 1;
        }
        else if(ps_rate_control_api->prev_ref_pic_type == P_PIC)
        {
            /* current frame is B-pic */
            /* Constrain B-frame QP to be within specified limit of prev_ref_qp/Kb */
            if(!ps_rate_control_api->i4_is_hbr)
            {
                prev_qp_q6 = (P_TO_B_RATIO * (LWORD64)prev_qp_q6) >> (K_Q);
            }
            else
            {
                prev_qp_q6 = (P_TO_B_RATIO_HBR * (LWORD64)prev_qp_q6) >> (K_Q);
            }
        }
        else /* if(ps_rate_control_api->prev_ref_pic_type == I_PIC)  */
        {
            /* current frame is B-pic */
            /* Constrain B-frame QP to be within specified limit of prev_ref_qp/Kb */
            if(!ps_rate_control_api->i4_is_hbr)
            {
                prev_qp_q6 = (P_TO_B_RATIO * I_TO_P_RATIO * (LWORD64)prev_qp_q6) >> (K_Q + K_Q);
            }
            else
            {
                prev_qp_q6 = (P_TO_B_RATIO_HBR * I_TO_P_RATIO * (LWORD64)prev_qp_q6) >> (K_Q + K_Q);
            }
        }
    }

    /*if (1)//e_pic_type != B_PIC)*/
    {
        if(use_I_frame_qp_high_dev)
        {
            /*For P pic if previous reference was I then pre_qp = I qp + 1, Then +4 high dev is allowed. To avoid P frame to be +5 off comapared to previous I*/
            hi_dev_qp_q6 = GET_HI_DEV_QP_QFAC(prev_qp_for_high_dev_q6);
        }
        else
        {
            hi_dev_qp_q6 = GET_HI_DEV_QP_QFAC(prev_qp_q6);
        }

        if(e_pic_type == I_PIC || e_pic_type == P_PIC || e_pic_type == P1_PIC)
        {
            lo_dev_qp_q6 =
                GET_LO_DEV_QP_QFAC(prev_qp_q6, ps_rate_control_api->i4_num_active_pic_type);
        }
        else
        {
            lo_dev_qp_q6 = GET_LO_DEV_QP_QFAC_B_PIC(prev_qp_q6);
        }
    }
    /* For lower QPs due to scale factor and fixed point arithmetic, the
    hi_dev_qp can be same as that of the prev qp and in which case it gets stuck
    in the lower most qp and thus not allowing QPs not to change. To avoid this,
    for lower qps the hi_dev_qp should be made slightly more than prev_qp */
    if(prev_qp_q6 == hi_dev_qp_q6)
    {
        hi_dev_qp_q6 = ((LWORD64)hi_dev_qp_q6 * 18) >> 4;
    }
    /*minimum qp should atleast be 1 less than previous*/
    if(prev_qp_q6 == lo_dev_qp_q6 && lo_dev_qp_q6 > (1 << QSCALE_Q_FAC))
    {
        lo_dev_qp_q6 = ((LWORD64)lo_dev_qp_q6 * 14) >> 4;
    }
    /*for shorter GOP make sure the P does not get better than I , NEED TO BE REVIEWED as gains seen in bq terrace after this change was with wrong config*/
    /*Anything with per pixel sad < 1 is considered static. Since the hme sad is at L1 resolution, the threshold chosen is 0.25*/
    if((per_pixel_p_hme_sad < 0.25f) && (ps_rate_control_api->i4_is_infinite_gop != 1))
    {
        if(e_pic_type == P_PIC && ps_rate_control_api->i4_I_frame_qp_model)
        {
            /*P is not allowed to get too better compared to previous I in static content*/
            if(lo_dev_qp_q6<(prev_I_qp_q6 * 14)>> 4)
                lo_dev_qp_q6 = ((LWORD64)prev_I_qp_q6 * 14) >> 4;
            /*If previous reference is I then it cannot get better than I in static case*/
            if(lo_dev_qp_q6 < prev_I_qp_q6)
                lo_dev_qp_q6 = prev_I_qp_q6;
        }
    }
    if(e_pic_type == I_PIC &&
       i4_intra_frm_int !=
           1) /*i_only: In this case P frame Qp will be arbitrary value hence avoiding max_dev_qp to be independent of it*/
    {
        //WORD32 i4_p_qp = ps_rate_control_api->ai4_prev_frm_qp[P_PIC];
        WORD32 i4_p_qp_q6 = ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][P_PIC];
        switch(i4_complexity_bin)
        {
        case 0:
            hi_dev_qp_temp_q6 = (WORD32)(
                ((LWORD64)i4_p_qp_q6 * I_TO_P_RATIO * I_TO_P_RATIO * I_TO_P_RATIO) >>
                (K_Q + K_Q + K_Q));
            break;
        case 1:
            hi_dev_qp_temp_q6 =
                (WORD32)(((LWORD64)i4_p_qp_q6 * I_TO_P_RATIO * I_TO_P_RATIO) >> (K_Q + K_Q));
            break;
        case 2:
            hi_dev_qp_temp_q6 = (WORD32)(((LWORD64)i4_p_qp_q6 * I_TO_P_RATIO) >> (K_Q));
            break;
        case 3:
            hi_dev_qp_temp_q6 = i4_p_qp_q6;
            break;
        default:
            hi_dev_qp_temp_q6 = (WORD32)(((LWORD64)i4_p_qp_q6 * P_TO_I_RATIO) >> (K_Q));
            break;
        }
        hi_dev_qp_q6 = (hi_dev_qp_q6 > hi_dev_qp_temp_q6) ? hi_dev_qp_temp_q6 : hi_dev_qp_q6;
    }
    pi4_hi_dev_qp_q6[0] = hi_dev_qp_q6;
    pi4_lo_dev_qp_q6[0] = lo_dev_qp_q6;
}

/****************************************************************************
Function Name : get_min
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
static WORD32 get_min(WORD32 a, WORD32 b, WORD32 c, WORD32 d)
{
    WORD32 min = a;
    if(b < min)
        min = b;
    if(c < min)
        min = c;
    if(d < min)
        min = d;
    return min;
}

/****************************************************************************
Function Name : get_max
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
static WORD32 get_max(WORD32 a, WORD32 b, WORD32 c)
{
    WORD32 max = a;
    if(b > max)
        max = b;
    if(c > max)
        max = c;
    return max;
}
/****************************************************************************
Function Name : rc_modify_est_tot
Description   : Adds latest Estimated total bits to the loop .
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_modify_est_tot(rate_control_api_t *ps_rate_control_api, WORD32 i4_tot_est_bits)  //ELP_RC
{
    WORD32 i4_num_frm_parallel, i;
    i4_num_frm_parallel = ps_rate_control_api->i4_num_frame_parallel;

    if(i4_num_frm_parallel)  //for CPU i4_num_frm_parallel=0
    {
        for(i = 1; i < (i4_num_frm_parallel - 1); i++)
        {
            ps_rate_control_api->ai4_est_tot_bits[i - 1] = ps_rate_control_api->ai4_est_tot_bits[i];
        }
        ps_rate_control_api->ai4_est_tot_bits[i - 1] = i4_tot_est_bits;
    }
}
/****************************************************************************
Function Name : rc_get_estimate_bit_error
Description   : function returns the estimated bit error using estimated total
                bits for the Enc Loop Parallelism based Encoder.
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
static WORD32 rc_get_estimate_bit_error(rate_control_api_t *ps_rate_control_api)
{
    WORD32 i4_error_bits = 0, i, i4_bits_per_frame;
    i4_bits_per_frame = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
    if(ps_rate_control_api->i4_num_frame_parallel >
       0)  // for CPU ps_rate_control_api->i4_num_frame_parallel =0;
    {
        for(i = 0; i < (ps_rate_control_api->i4_num_frame_parallel - 1); i++)
        {
            i4_error_bits += (ps_rate_control_api->ai4_est_tot_bits[i] - i4_bits_per_frame);
        }
    }
    return i4_error_bits;
}

/****************************************************************************
Function Name : get_est_hdr_bits
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_est_hdr_bits(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    return (get_cur_frm_est_header_bits(ps_rate_control_api->ps_bit_allocation, e_pic_type));
}

/****************************************************************************
Function Name : model_availability
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 model_availability(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    return (is_model_valid(ps_rate_control_api->aps_rd_model[e_pic_type]));
}

/****************************************************************************
Function Name : clip_qp_based_on_prev_ref
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 clip_qp_based_on_prev_ref(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_call_type,
    WORD32 i4_scene_num)
{
    /* WORD32 i4_bpp_based_qp; */
    /* If the number pf pels is set to zero it uses the value set during init time */
    /* i4_frame_qp = get_init_qp_using_pels_bits_per_frame(ps_rate_control_api->ps_init_qp,
    e_pic_type, i4_est_tex_bits, 0); */
    WORD32 i4_frame_qp, i4_frame_qp_q6 = 0, i4_min_Kp_Kb_factor = 0;
    WORD32 Kp_kb_factor = get_Kp_Kb(ps_rate_control_api->ps_bit_allocation, e_pic_type);
    WORD32 kp_kb_ref_ref =
        get_Kp_Kb(ps_rate_control_api->ps_bit_allocation, ps_rate_control_api->prev_ref_pic_type);

    {
        WORD32 i4_drain_bits_per_frame = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer),
               i4_ebf;
        WORD32 i4_delay = cbr_get_delay_frames(ps_rate_control_api->ps_cbr_buffer),
               max_buffer_level = 0, rc_type = get_rc_type(ps_rate_control_api->ps_cbr_buffer);

        if(rc_type == VBR_STREAMING)
            max_buffer_level = i4_drain_bits_per_frame * i4_delay;
        else
            max_buffer_level = get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer);

        i4_ebf = get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer);

        if(i4_ebf > (WORD32)(0.9f * max_buffer_level))
        {
            switch(e_pic_type)
            {
            case P_PIC:
            case P1_PIC:
                i4_min_Kp_Kb_factor = I_TO_P_RATIO;
                break;
            case B_PIC:
            case BB_PIC:
                i4_min_Kp_Kb_factor = I_TO_B_RATIO;
                break;
            case B1_PIC:
            case B11_PIC:
                i4_min_Kp_Kb_factor = I_TO_B1_RATIO;
                break;
            default:
                i4_min_Kp_Kb_factor = I_TO_B2_RATIO;
                break;
            }
        }
    }
    if((e_pic_type == I_PIC) &&
       (ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] == 0x7FFFFFFF))
    {
        /*Is this a valid case?*/
        ASSERT(0);
    }
    /*If there is a scene cut I frame followed by a scene cut I frame, non scene cut I frame
    better assume the Qp of the I frame same as before instead of using bpp based qp*/
    else if(
        (e_pic_type == I_PIC) &&
        (ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] != 0x7FFFFFFF))
    {
        i4_frame_qp = ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC];
        i4_frame_qp_q6 = ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC];
    }
    else /*! ISlice*/
    {
        if((Kp_kb_factor < i4_min_Kp_Kb_factor) && (i4_call_type == 1))
        {
            Kp_kb_factor = i4_min_Kp_Kb_factor;
            trace_printf("Kp_kb_factor %d", Kp_kb_factor);
        }
        if((kp_kb_ref_ref > Kp_kb_factor) && (i4_call_type == 1))
        {
            kp_kb_ref_ref = Kp_kb_factor;
        }

        if(ps_rate_control_api
               ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] ==
           0x7FFFFFFF)
        {
            ps_rate_control_api
                ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] =
                ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC];
            kp_kb_ref_ref = 16;
        }

        i4_frame_qp_q6 =
            ((ps_rate_control_api
                  ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] *
              Kp_kb_factor) /
             kp_kb_ref_ref);
    }
    return i4_frame_qp_q6;
}

/****************************************************************************
Function Name : get_frame_level_qp
Description   : Get frame qp from the estimated bits
Inputs        : ps_rate_control_api
                i_to_avg_ratio

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_frame_level_qp(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_ud_max_bits,
    WORD32 *pi4_cur_est_texture_bits,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    WORD32 i4_call_type,
    float i_to_avg_ratio,
    frame_info_t *ps_frame_stat,
    WORD32 i4_complexity_bin,
    WORD32 i4_scene_num,
    WORD32 *pi4_tot_bits_estimated,
    WORD32 *pi4_is_model_valid,
    WORD32 *pi4_vbv_buf_max_bits,
    WORD32 *pi4_est_tex_bits,
    WORD32 *pi4_cur_est_header_bits,
    WORD32 *pi4_maxEbfQP,
    WORD32 *pi4_modelQP,
    WORD32 *pi4_estimate_to_calc_frm_error)
{
    /* UWORD8 u1_frame_qp; */
    WORD32 i4_frame_qp /*,i4_min_frame_qp = 1,i4_max_frame_qp = MAX_MPEG2_QP*/;
    WORD32 i4_max_frame_qp_q6 = (MAX_MPEG2_QP << QSCALE_Q_FAC),
           i4_min_frame_qp_q6 = MIN_QSCALE_Q6; /*0.707 in q6 corresponds to hevc qp = 1*/
    WORD32 i4_is_first_frame_coded = 1;
    WORD32 i4_is_model_valid = 0;
    WORD32 i4_frame_qp_q6, i4_cur_est_header_bits, i4_frame_qp_q6_based_max_vbv_bits;
    WORD32 i4_bit_alloc_est_tex_bits = 0, i4_bit_alloc_est_tex_bits_for_invalid_model = 0,
           i4_est_tex_bits, i4_qp_based_min_est_tex_bits, i4_qp_based_max_est_tex_bits,
           i4_buf_based_min_bits, i4_buf_based_max_bits;
    UWORD32 u4_estimated_sad;
    WORD32 i4_buffer_based_max_qp_clip_flag = 0;
    WORD32 i4_min_Kp_Kb_factor = 0;
    WORD32 i4_steady_state_texture_case = 0;

    if(i4_call_type == 1)
    {
        *pi4_maxEbfQP = INVALID_QP;
        *pi4_modelQP = INVALID_QP;
    }

    if((ps_rate_control_api->e_rc_type != VBR_STORAGE) &&
       (ps_rate_control_api->e_rc_type != VBR_STORAGE_DVD_COMP) &&
       (ps_rate_control_api->e_rc_type != CBR_NLDRC) &&
       (ps_rate_control_api->e_rc_type != CONST_QP) &&
       (ps_rate_control_api->e_rc_type != VBR_STREAMING))
    {
        return (0);
    }

    i4_is_first_frame_coded = is_first_frame_coded(ps_rate_control_api);

    assign_complexity_coeffs(ps_rate_control_api->ps_bit_allocation, af_sum_weigh);

    if(ps_rate_control_api->e_rc_type == CONST_QP)
    {
        i4_frame_qp = ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][e_pic_type];
        i4_frame_qp_q6 =
            (ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][e_pic_type] >> QSCALE_Q_FAC);
    }
    else
    {
        i4_cur_est_header_bits =
            get_cur_frm_est_header_bits(ps_rate_control_api->ps_bit_allocation, e_pic_type);
        u4_estimated_sad = get_est_sad(ps_rate_control_api->ps_est_sad, e_pic_type);
        /* Constraining the qp variations based on bits allocated */
        /* Step 1: Getting the bits based on bit allocation module */
        /*check if model has atleast one data point, otherwise go with default qp*/
        i4_is_model_valid = is_model_valid(ps_rate_control_api->aps_rd_model[e_pic_type]);

        if(i4_is_model_valid == 1)
        {
            i4_bit_alloc_est_tex_bits = get_cur_frm_est_texture_bits(
                ps_rate_control_api->ps_bit_allocation,
                ps_rate_control_api->aps_rd_model,
                ps_rate_control_api->ps_est_sad,
                ps_rate_control_api->ps_pic_handling,
                ps_rate_control_api->ps_cbr_buffer,
                e_pic_type,
                i4_is_first_frame_coded,
                0,
                i4_call_type,
                i_to_avg_ratio,
                i4_is_model_valid);
            if(i4_call_type == 1)
            {
                *pi4_estimate_to_calc_frm_error =
                    i4_bit_alloc_est_tex_bits + i4_cur_est_header_bits;
            }

            /* vbv buffer position based error correction to keep away encoder buffer overflow at layer 0 pictures*/
            if(e_pic_type == I_PIC || e_pic_type == P_PIC || e_pic_type == P1_PIC)
            {
                WORD32 i4_cur_ebf = get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer);
                WORD32 i4_vbv_size = get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer);
                WORD32 i4_max_ebf = (WORD32)(i4_vbv_size * MAX_THRESHOLD_VBV_FRM_ERROR);
                WORD32 i4_drain_rate = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
                WORD32 i4_total_bits_allocted = i4_bit_alloc_est_tex_bits + i4_cur_est_header_bits;
                WORD32 i4_total_bits_to_be_alloc;
                WORD32 i4_expected_ebf = (i4_cur_ebf + i4_total_bits_allocted - i4_drain_rate);
                /*if expected ebf is greater than max threashold, correct the allocation such that it never cross max
                but if it less than drain rate, atleast give drainrate bits*/
                if(i4_expected_ebf > i4_max_ebf)
                {
                    i4_total_bits_to_be_alloc = MAX(
                        i4_drain_rate, (i4_total_bits_allocted - (i4_expected_ebf - i4_max_ebf)));
                    i4_bit_alloc_est_tex_bits = i4_total_bits_to_be_alloc - i4_cur_est_header_bits;
                }
            }
        }
        else
        {
            i4_bit_alloc_est_tex_bits_for_invalid_model = get_cur_frm_est_texture_bits(
                ps_rate_control_api->ps_bit_allocation,
                ps_rate_control_api->aps_rd_model,
                ps_rate_control_api->ps_est_sad,
                ps_rate_control_api->ps_pic_handling,
                ps_rate_control_api->ps_cbr_buffer,
                e_pic_type,
                i4_is_first_frame_coded,
                0,
                i4_call_type,
                i_to_avg_ratio,
                i4_is_model_valid);
            if(i4_call_type == 1)
            {
                *pi4_estimate_to_calc_frm_error =
                    i4_bit_alloc_est_tex_bits_for_invalid_model + i4_cur_est_header_bits;
            }
        }

#if 1 /*model_low_bitrate_bug*/
        /* This condition is added to use the model for cases when the estimated bits is less than zero.
        We assume some bits of the header are used for texture and calcualte the qp */
        if(i4_bit_alloc_est_tex_bits <= (i4_cur_est_header_bits >> 3))
        {
            i4_bit_alloc_est_tex_bits = (i4_cur_est_header_bits >> 3);
        }
#endif

        /* Step 2: Getting the min and max texture bits based on min and max qp */
        if(i4_is_model_valid && ps_rate_control_api->au1_avg_bitrate_changed[e_pic_type] == 0)
        {
            WORD32 /*i4_min_qp, i4_max_qp,*/ i4_max_qp_q6, i4_min_qp_q6;
            number_t s_lin_coeff_wo_int =
                get_linear_coefficient(ps_rate_control_api->aps_rd_model[e_pic_type]);

            if(s_lin_coeff_wo_int.sm != 0)
            {
                /* Get the min and max qp deviation allowed based on prev frame qp */
                get_min_max_qp(
                    ps_rate_control_api,
                    e_pic_type,
                    &i4_max_qp_q6,
                    &i4_min_qp_q6,
                    i4_complexity_bin,
                    i4_scene_num);

                /* Estimate the max bits based on min qp */
                i4_qp_based_min_est_tex_bits = estimate_bits_for_qp(
                    ps_rate_control_api->aps_rd_model[e_pic_type], u4_estimated_sad, i4_max_qp_q6);
                /* Estimate the min bits based on max qp */
                i4_qp_based_max_est_tex_bits = estimate_bits_for_qp(
                    ps_rate_control_api->aps_rd_model[e_pic_type], u4_estimated_sad, i4_min_qp_q6);
                /*disable qp based min and max swing restriction*/
                i4_min_frame_qp_q6 = i4_min_qp_q6;
                i4_max_frame_qp_q6 = i4_max_qp_q6;
                i4_qp_based_max_est_tex_bits = i4_bit_alloc_est_tex_bits;
                i4_qp_based_min_est_tex_bits = i4_bit_alloc_est_tex_bits;
            }
            else
            {
                i4_qp_based_min_est_tex_bits = i4_bit_alloc_est_tex_bits;
                i4_qp_based_max_est_tex_bits = i4_bit_alloc_est_tex_bits;
            }
        }
        else
        {
            i4_qp_based_min_est_tex_bits = i4_bit_alloc_est_tex_bits_for_invalid_model;
            i4_qp_based_max_est_tex_bits = i4_bit_alloc_est_tex_bits_for_invalid_model;
            ps_rate_control_api->au1_avg_bitrate_changed[e_pic_type] = 0;
        }

        /* Step 3: Getting the min and max texture bits based on buffer fullness */

        if(i4_call_type == 1)
        {
            WORD32 i4_get_error;

            i4_get_error = rc_get_estimate_bit_error(ps_rate_control_api);

            get_min_max_bits_based_on_buffer(
                ps_rate_control_api,
                e_pic_type,
                &i4_buf_based_min_bits,
                &i4_buf_based_max_bits,
                i4_get_error);

            /*In case buffer limitation will come, no need to reduce the QP further because of warning flag*/
            if(i4_bit_alloc_est_tex_bits < (i4_buf_based_min_bits - i4_cur_est_header_bits))
                ps_rate_control_api->i4_underflow_warning = 0;

            if(i4_buf_based_max_bits < (i4_bit_alloc_est_tex_bits + i4_cur_est_header_bits))
            {
                i4_buffer_based_max_qp_clip_flag = 1;
            }
            trace_printf(
                "i4_buf_based_min_bits %d i4_buf_based_max_bits %d",
                i4_buf_based_min_bits,
                i4_buf_based_max_bits);
            trace_printf(
                "Prev I frame qp q6 %d P frame qp q6 %d",
                ps_rate_control_api->ai4_prev_frm_qp_q6[I_PIC],
                ps_rate_control_api->ai4_prev_frm_qp_q6[P_PIC]);
        }
        else
        {
            i4_buf_based_min_bits = i4_qp_based_min_est_tex_bits;
            i4_buf_based_max_bits = i4_qp_based_max_est_tex_bits;
        }
        /* for I frame the max bits is not restricted based on the user input */
        if(e_pic_type == I_PIC)
        {
            i4_ud_max_bits = 0x7fffffff; /* i4_bit_alloc_est_tex_bits + i4_cur_est_header_bits; */
        }

        /* Step 4: Clip the bits allocated based on
        1) FinalBits = Max of (BitallocBits, MinBitsMaxQp, MinBufferBits)
        2) FinalBits = Min of (MaxBitsMinQp, MaxBufferBits, MaxUserDefBits, FinalBits)
        Note that max is done after min to prevent over-consumption */
        /* Finding the max of all the minimum bits */
        i4_est_tex_bits = get_max(
            i4_bit_alloc_est_tex_bits,
            i4_qp_based_min_est_tex_bits,
            (i4_buf_based_min_bits - i4_cur_est_header_bits));
        i4_est_tex_bits = get_min(
            i4_est_tex_bits,
            i4_qp_based_max_est_tex_bits,
            (i4_ud_max_bits - i4_cur_est_header_bits),
            (i4_buf_based_max_bits - i4_cur_est_header_bits));

        /*Highest priority given to min and max qp followed by buffer based min and max to prevent overconsumption in process of preventing stuffing*/
        CLIP(
            i4_est_tex_bits,
            i4_buf_based_max_bits - i4_cur_est_header_bits,
            i4_buf_based_min_bits - i4_cur_est_header_bits);

        {
            WORD32 i4_drain_bits_per_frame =
                       get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer),
                   i4_ebf;
            WORD32 i4_delay = cbr_get_delay_frames(ps_rate_control_api->ps_cbr_buffer),
                   max_buffer_level = 0, rc_type = get_rc_type(ps_rate_control_api->ps_cbr_buffer);

            if(rc_type == VBR_STREAMING)
                max_buffer_level = i4_drain_bits_per_frame * i4_delay;
            else
                max_buffer_level = get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer);

            i4_ebf = get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer);

            if(i4_ebf > (WORD32)(0.9f * max_buffer_level))
            {
                i4_buffer_based_max_qp_clip_flag = 1;
                switch(e_pic_type)
                {
                case P_PIC:
                case P1_PIC:
                    i4_min_Kp_Kb_factor = I_TO_P_RATIO;
                    break;
                case B_PIC:
                case BB_PIC:
                    i4_min_Kp_Kb_factor = I_TO_B_RATIO;
                    break;
                case B1_PIC:
                case B11_PIC:
                    i4_min_Kp_Kb_factor = I_TO_B1_RATIO;
                    break;
                default:
                    i4_min_Kp_Kb_factor = I_TO_B2_RATIO;
                    break;
                }
            }
        }
        /*i4_is_first_frame_coded will be considered only in 2 pass, since 2 pass precise I to rest is calcuated considering first sug-gop and full sub-gop complexity separately. Using offset based
        qp instead of single frame model(with default bit allocation)*/
        /* Step 6: Estimate the qp generated for the given texture bits */
        if((!i4_is_first_frame_coded /* && ps_rate_control_api->i4_rc_pass == 2*/) ||
           !i4_is_model_valid)  //ELP_RC
        {
            /* WORD32 i4_bpp_based_qp; */
            /* If the number pf pels is set to zero it uses the value set during init time */
            /* i4_frame_qp = get_init_qp_using_pels_bits_per_frame(ps_rate_control_api->ps_init_qp,
            e_pic_type, i4_est_tex_bits, 0); */
            WORD32 Kp_kb_factor = get_Kp_Kb(ps_rate_control_api->ps_bit_allocation, e_pic_type);
            WORD32 kp_kb_ref_ref = get_Kp_Kb(
                ps_rate_control_api->ps_bit_allocation, ps_rate_control_api->prev_ref_pic_type);

            if(e_pic_type == I_PIC &&
               ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] == 0x7FFFFFFF)
            {
                /*Is this a valid case?*/
                ASSERT(0);
                i4_frame_qp = get_init_qp_using_pels_bits_per_frame(
                    ps_rate_control_api->ps_init_qp, e_pic_type, i4_est_tex_bits, 0);
                i4_frame_qp_q6 = i4_frame_qp << QSCALE_Q_FAC;
            }
            /*If there is a scene cut I frame followed by a scene cut I frame, non scene cut I frame
            better assume the Qp of the I frame same as before instead of using bpp based qp*/
            else if(
                e_pic_type == I_PIC &&
                ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] != 0x7FFFFFFF)
            {
                i4_frame_qp = ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC];
                i4_frame_qp_q6 = ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC];
            }
            else /*! ISlice*/
            {
                if((Kp_kb_factor < i4_min_Kp_Kb_factor) && (i4_call_type == 1))
                {
                    Kp_kb_factor = i4_min_Kp_Kb_factor;
                    trace_printf("Kp_kb_factor %d", Kp_kb_factor);
                }
                if((kp_kb_ref_ref > Kp_kb_factor) && (i4_call_type == 1))
                {
                    kp_kb_ref_ref = Kp_kb_factor;
                }

                if(ps_rate_control_api
                       ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] ==
                   0x7FFFFFFF)
                {
                    ps_rate_control_api
                        ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] =
                        ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC];
                    kp_kb_ref_ref = 16;
                }

                i4_frame_qp_q6 =
                    ((ps_rate_control_api
                          ->ai4_prev_frm_qp_q6[i4_scene_num][ps_rate_control_api->prev_ref_pic_type] *
                      Kp_kb_factor) /
                     kp_kb_ref_ref);
            }

            /*HEVC_hierarchy: Breaks pause to resume logic if any and also the HBR mode concept as bit ratios are not known. It is now quaranteed that all frames
            encoded after scene cut will belong to new scene(B pic of first sub-gop)Hence the below logic of using max of either current estimate
            or previous B frame qp is not required*/
            /* Since precise SCD position at B-pic level is not known, take the MAX of earlier B-QP and scaled I_QP after SCD */
            /*HEVC_RC : Since precise SCD location is known and it is guranteed that pic encoded after I pic belongs to new scene*/

            {
                WORD32 i4_bits_per_frame;
                i4_bits_per_frame = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
                if(i4_call_type == 1)
                {
                    rc_modify_est_tot(ps_rate_control_api, i4_bits_per_frame);
                }
            }
        }
        /* The check is becaue the model gives a negative QP when the
        i4_est_tex_bits is less than or equal to 0
        [This is a bug in the model]. As a temporary fix, the frame QP
        is being set to the max QP allowed  */
        else if(i4_est_tex_bits > 0)
        {
            if(i4_call_type == 1)
            {
                rc_modify_est_tot(ps_rate_control_api, (i4_est_tex_bits + i4_cur_est_header_bits));
            }
            i4_steady_state_texture_case = 1;
            /* Query the model for the Qp for the corresponding frame*/
            i4_frame_qp_q6_based_max_vbv_bits = find_qp_for_target_bits(
                ps_rate_control_api->aps_rd_model[e_pic_type],
                i4_buf_based_max_bits - i4_cur_est_header_bits,
                u4_estimated_sad,
                (ps_rate_control_api->ai4_max_qp_q6[e_pic_type]),
                (ps_rate_control_api->ai4_min_qp_q6[e_pic_type]));
            if(i4_call_type == 1)
            {
                *pi4_maxEbfQP = ihevce_rc_get_scaled_hevce_qp_q6(
                    i4_frame_qp_q6_based_max_vbv_bits, ps_rate_control_api->u1_bit_depth);
            }
            /* Query the model for the Qp for the corresponding frame*/
            i4_frame_qp_q6 = find_qp_for_target_bits(
                ps_rate_control_api->aps_rd_model[e_pic_type],
                i4_est_tex_bits,
                u4_estimated_sad,
                (ps_rate_control_api->ai4_max_qp_q6[e_pic_type]),
                (ps_rate_control_api->ai4_min_qp_q6[e_pic_type]));
            i4_frame_qp = ((i4_frame_qp_q6 + (1 << (QSCALE_Q_FAC - 1))) >> QSCALE_Q_FAC);
        }
        else
        {
            {
                WORD32 i4_bits_per_frame;
                i4_bits_per_frame = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
                if(i4_call_type == 1)
                {
                    rc_modify_est_tot(ps_rate_control_api, i4_bits_per_frame);
                }
            }
            i4_frame_qp = ps_rate_control_api->ai4_max_qp[e_pic_type];
            i4_frame_qp_q6 = ps_rate_control_api->ai4_max_qp_q6[e_pic_type];
        }
        if(i4_call_type == 1)
        {
            *pi4_modelQP =
                ihevce_rc_get_scaled_hevce_qp_q6(i4_frame_qp_q6, ps_rate_control_api->u1_bit_depth);
        }
        {
            /*This clip is added to prevent the change in qp close to scene cuts i.e even though the buffer
            allows the qp to go low the bit alloc model has a problem of having the denominator considering
            the previous subgop complexity and giving bits*/
            WORD32 i4_clip_flag =
                ((i4_call_type == 1) && (i4_is_model_valid == 1) &&
                 (ps_rate_control_api->i4_rc_pass == 2) &&
                 (i4_buf_based_max_bits > i4_est_tex_bits));
            WORD32 i4_ebf = rc_get_ebf(ps_rate_control_api),
                   i4_max_ebf = i4_ebf + i4_buf_based_max_bits;
            WORD32 i4_inter_frame_interval =
                pic_type_get_inter_frame_interval(ps_rate_control_api->ps_pic_handling);
            float f_buffer_fullness = (float)i4_ebf / i4_max_ebf;
            i4_clip_flag = i4_clip_flag && (ps_rate_control_api->i4_scd_in_period_2_pass == 1);
            i4_clip_flag = i4_clip_flag && (i4_ebf < (i4_max_ebf * 0.5f));
            i4_clip_flag = i4_clip_flag && (ps_rate_control_api->e_rc_type == VBR_STREAMING);

            i4_clip_flag = i4_clip_flag && (ps_rate_control_api->i4_frames_since_last_scd >
                                            i4_inter_frame_interval);

            if(i4_clip_flag == 1)
            {
                WORD32 i4_prev_frame_tot_est_bits = ba_get_prev_frame_tot_est_bits(
                    ps_rate_control_api->ps_bit_allocation, (WORD32)ps_rate_control_api->e_rc_type);
                WORD32 i4_prev_frame_tot_bits = ba_get_prev_frame_tot_bits(
                    ps_rate_control_api->ps_bit_allocation, (WORD32)ps_rate_control_api->e_rc_type);
                float i4_consumption_ratio =
                    (float)i4_prev_frame_tot_bits / i4_prev_frame_tot_est_bits;
                if(i4_consumption_ratio > 0.7f && i4_consumption_ratio < 1.5f)
                    i4_clip_flag = 1;
                else
                    i4_clip_flag = 0;
            }
            if(i4_clip_flag == 1)
            {
                trace_printf("Clipped");
                trace_printf("Before %d", i4_frame_qp_q6);
                if(af_sum_weigh[e_pic_type][0] > 1.0f)
                {
                    /*Complex followed by simple*/
                    if(i4_frame_qp_q6 >
                       ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type])
                    {
                        if(f_buffer_fullness < 0.3f)
                        {
                            i4_frame_qp_q6 =
                                ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type];
                        }
                        else
                        {
                            if(i4_frame_qp_q6 >
                               (ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type] *
                                72 * 3))
                                i4_frame_qp_q6 =
                                    (ps_rate_control_api
                                         ->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type] *
                                     72 * 3);
                        }
                    }
                }
                if(af_sum_weigh[e_pic_type][0] < 1.0f)
                {
                    /*Simple followed by complex*/
                    if(i4_frame_qp_q6 <
                       ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type])
                    {
                        /*i4_frame_qp_q6 = ps_rate_control_api->ai4_prev_frm_qp_q6[e_pic_type];*/
                    }
                }
                trace_printf("After %d", i4_frame_qp_q6);
            }
        }

        /*swing restriciton based on previous frame qp swing*/
        {
            if(i4_call_type == 1)
            {
                trace_printf(
                    "Before i4_frame_qp_q6 = %d min qp = %d  max_qp = %d    "
                    "bufclip %d",
                    i4_frame_qp_q6,
                    (i4_min_frame_qp_q6),
                    (i4_max_frame_qp_q6),
                    i4_buffer_based_max_qp_clip_flag);
            }
            if(i4_frame_qp_q6 < i4_min_frame_qp_q6)
                i4_frame_qp_q6 = i4_min_frame_qp_q6;

            /*removed low side clipping to avoid HRD compliance issue*/
            if(i4_steady_state_texture_case)
            {
                if(i4_frame_qp_q6 > i4_max_frame_qp_q6)
                {
                    if(i4_max_frame_qp_q6 > (i4_frame_qp_q6_based_max_vbv_bits))
                    {
                        i4_frame_qp_q6 = i4_max_frame_qp_q6;
                    }
                    else
                    {
                        i4_frame_qp_q6 = i4_frame_qp_q6_based_max_vbv_bits;
                    }
                }
            }
        }
        if(i4_call_type == 1)
        {
            trace_printf("After i4_frame_qp_q6 = %d", i4_frame_qp_q6);
        }

        /* SS - Following done to restore this after pause to resume detect - 0.25 is for syntax bits */
        ps_rate_control_api->i4_orig_frm_est_bits = (i4_est_tex_bits * 5) >> 2;
        ps_rate_control_api->i4_prev_frm_est_bits = (i4_est_tex_bits + i4_cur_est_header_bits);
        pi4_cur_est_texture_bits[0] = i4_est_tex_bits;

        /*For frames after SCD, when neither online or offline model can estimate the bits,
        use the remaining bits in period as max bits*/
        *pi4_is_model_valid = i4_is_model_valid;

        if(0 == i4_is_model_valid)
        {
            *pi4_tot_bits_estimated =
                i4_bit_alloc_est_tex_bits_for_invalid_model;  //(i4_buf_based_max_bits * 0.80);
        }
        else
        {
            *pi4_tot_bits_estimated = i4_est_tex_bits + i4_cur_est_header_bits;
        }

        /*For B pics assigning a non-zero value to avoid asser */
        if(*pi4_tot_bits_estimated == 0)
        {
            *pi4_tot_bits_estimated = 1;
        }
        ASSERT(*pi4_tot_bits_estimated != 0);
        /*Underflow prevention*/
        if((ps_rate_control_api->i4_underflow_warning == 1) &&
           (i4_est_tex_bits < (i4_buf_based_max_bits - i4_cur_est_header_bits)) &&
           (i4_call_type == 1))
        {
            //printf("\nUnderflow warning\n");
            /*Decrement the hevc_qp by 1 for underflow prevention*/
            i4_frame_qp_q6 = (WORD32)((float)i4_frame_qp_q6 / (float)1.125f);
            ps_rate_control_api->i4_underflow_warning = 0;
            if(i4_call_type == 1)
            {
                trace_printf("\nUnderflow warning");
            }
        }
    }

    /* Clip the frame qp within Min and Max QP */
    if(i4_frame_qp_q6 < ps_rate_control_api->ai4_min_qp_q6[e_pic_type])
    {
        i4_frame_qp_q6 = ps_rate_control_api->ai4_min_qp_q6[e_pic_type];
    }
    else if(i4_frame_qp_q6 > ps_rate_control_api->ai4_max_qp_q6[e_pic_type])
    {
        i4_frame_qp_q6 = ps_rate_control_api->ai4_max_qp_q6[e_pic_type];
    }
    if(i4_call_type == 1)
    {
        *pi4_vbv_buf_max_bits = i4_buf_based_max_bits;
        *pi4_est_tex_bits = i4_est_tex_bits;
        *pi4_cur_est_header_bits = i4_cur_est_header_bits;
    }
    return (i4_frame_qp_q6);
}

/****************************************************************************
Function Name : get_bits_for_final_qp
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/

void get_bits_for_final_qp(
    rate_control_api_t *ps_rate_control_api,
    WORD32 *pi4_modelQP,
    WORD32 *pi4_maxEbfQP,
    LWORD64 *pi8_bits_from_finalQP,
    WORD32 i4_clipQP,
    WORD32 i4_frame_qp_q6,
    WORD32 i4_cur_est_header_bits,
    WORD32 i4_est_tex_bits,
    WORD32 i4_buf_based_max_bits,
    picture_type_e e_pic_type,
    WORD32 i4_display_num)
{
    UWORD32 u4_estimated_sad;
    u4_estimated_sad = get_est_sad(ps_rate_control_api->ps_est_sad, e_pic_type);
    {
        //printf("%d:\ti4_modelQP = %d\t i4_maxEbfQP = %d\t i4_clipQP = %d\t bits = %d\n",i4_display_num,*pi4_modelQP,*pi4_maxEbfQP,i4_clipQP,*pi8_bits_from_finalQP);
        if((*pi4_modelQP != INVALID_QP) && (*pi4_maxEbfQP != INVALID_QP) &&
           /*(*pi4_modelQP >= i4_clipQP) &&*/
           (i4_clipQP > *pi4_maxEbfQP))
        {
            WORD32 i4_loop = 0, i4_error, i4_prev_error = 0x7FFFFFFF;
            WORD32 i4_frame_qp_q6_temp;
            WORD32 i4_buf_max_text_bits = i4_buf_based_max_bits - i4_cur_est_header_bits;
            WORD32 i4_min_bits = i4_est_tex_bits, i4_max_bits = i4_buf_max_text_bits;
            WORD32 i4_temp_bits = (i4_min_bits + i4_max_bits) >> 1;
            if(*pi4_modelQP == i4_clipQP)
            {
                *pi8_bits_from_finalQP = i4_est_tex_bits + i4_cur_est_header_bits;
                //printf("%d:\ti4_modelQP = %d\t i4_maxEbfQP = %d\t i4_clipQP = %d\t bits = %d\n",i4_display_num,*pi4_modelQP,*pi4_maxEbfQP,i4_clipQP,*pi8_bits_from_finalQP);
                return;
            }
            //printf("%d:\ti4_modelQP = %d\t i4_maxEbfQP = %d\t i4_clipQP = %d\t bits = %d\n",i4_display_num,*pi4_modelQP,*pi4_maxEbfQP,i4_clipQP,*pi8_bits_from_finalQP);
            /*binary search to find out bits corresponds to final QP(clipped)*/
            while(i4_loop < 30)
            {
                i4_frame_qp_q6_temp = find_qp_for_target_bits(
                    ps_rate_control_api->aps_rd_model[e_pic_type],
                    i4_temp_bits,
                    u4_estimated_sad,
                    (ps_rate_control_api->ai4_max_qp_q6[e_pic_type]),
                    (ps_rate_control_api->ai4_min_qp_q6[e_pic_type]));
                i4_error = abs(i4_frame_qp_q6_temp - i4_frame_qp_q6);
                if(i4_error < i4_prev_error)
                {
                    *pi8_bits_from_finalQP = i4_temp_bits + i4_cur_est_header_bits;
                    i4_prev_error = i4_error;
                    //printf("*pi8_bits_from_finalQP = %d\n",*pi8_bits_from_finalQP);
                }
                if(i4_frame_qp_q6_temp < i4_frame_qp_q6)
                {
                    i4_max_bits = i4_temp_bits;
                }
                else
                {
                    i4_min_bits = i4_temp_bits;
                }
                i4_temp_bits = (i4_min_bits + i4_max_bits) >> 1;
                i4_loop++;
            }
        }
        else
        {
            /* when est bits is less than 0 , max ebfQP is not updated, hence invalid
            as estimated bits are less it will not cause any buffer trouble*/
            if(((*pi4_maxEbfQP == INVALID_QP) && (*pi4_modelQP == i4_clipQP)))
            {
                *pi8_bits_from_finalQP = i4_est_tex_bits + i4_cur_est_header_bits;
            }
            else
            {
                *pi8_bits_from_finalQP = i4_buf_based_max_bits;
            }
        }
    }
    return;
}
/****************************************************************************
*Function Name : get_buffer_status
*Description   : Gets the state of VBV buffer
*Inputs        : Rate control API , header and texture bits
*Globals       :
*Processing    :
*Outputs       : 0 = normal, 1 = underflow, 2= overflow
*Returns       : vbv_buf_status_e
*Issues        :
*Revision History:
*DD MM YYYY   Author(s)       Changes (Describe the changes made)
*
********************************************************************************/
vbv_buf_status_e get_buffer_status(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_total_frame_bits, /* Total frame bits consumed */
    picture_type_e e_pic_type,
    WORD32 *pi4_num_bits_to_prevent_vbv_underflow)
{
    vbv_buf_status_e e_buf_status = VBV_NORMAL;

    /* Get the buffer status for the current total consumed bits and error bits*/
    if(ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP)
    {
        e_buf_status = get_vbv_buffer_status(
            ps_rate_control_api->ps_vbr_storage_vbv,
            i4_total_frame_bits,
            pi4_num_bits_to_prevent_vbv_underflow);
    }
    else if(ps_rate_control_api->e_rc_type == VBR_STORAGE)
    {
        /* For VBR case since there is not underflow returning the max value */
        pi4_num_bits_to_prevent_vbv_underflow[0] =
            get_max_vbv_buf_size(ps_rate_control_api->ps_vbr_storage_vbv);
        e_buf_status = VBV_NORMAL;
    }
    else if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
    {
        e_buf_status = get_cbr_buffer_status(
            ps_rate_control_api->ps_cbr_buffer,
            i4_total_frame_bits,
            pi4_num_bits_to_prevent_vbv_underflow,
            e_pic_type,
            ps_rate_control_api->e_rc_type);
    }
    else if(ps_rate_control_api->e_rc_type == VBR_STREAMING)
    {
        /* For VBR_streaming the error bits are computed according to peak bitrate*/
        e_buf_status = get_cbr_buffer_status(
            ps_rate_control_api->ps_cbr_buffer,
            i4_total_frame_bits,
            pi4_num_bits_to_prevent_vbv_underflow,
            e_pic_type,
            ps_rate_control_api->e_rc_type);
    }
    return e_buf_status;
}
/****************************************************************************
  Function Name : update_pic_handling_state
  Description   : If the forward path and the backward path of rate control
  Inputs        :
  Globals       :
  Processing    :
  Outputs       :
  Returns       :
  Issues        :
  Revision History:
  DD MM YYYY   Author(s)       Changes (Describe the changes made)
                KJN             Original
*****************************************************************************/
void update_pic_handling_state(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    WORD32 i4_is_non_ref_pic = 0;
    update_pic_handling(ps_rate_control_api->ps_pic_handling, e_pic_type, i4_is_non_ref_pic, 0);
}
LWORD64 get_gop_bits(rate_control_api_t *ps_rate_control_api)
{
    return (ba_get_gop_bits(ps_rate_control_api->ps_bit_allocation));
}
LWORD64 get_gop_sad(rate_control_api_t *ps_rate_control_api)
{
    return (ba_get_gop_sad(ps_rate_control_api->ps_bit_allocation));
}
WORD32 check_if_current_GOP_is_simple(rate_control_api_t *ps_rate_control_api)
{
    LWORD64 i8_buffer_play_bits =
        ba_get_buffer_play_bits_for_cur_gop(ps_rate_control_api->ps_bit_allocation);
    if(i8_buffer_play_bits)
    {
        if((i8_buffer_play_bits + get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer)) >
           (0.6 * get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer)))
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else
    {
        return 1;
    }
}
LWORD64 rc_get_rbip_and_num_frames(rate_control_api_t *ps_rate_control_api, WORD32 *pi4_num_frames)
{
    return (ba_get_rbip_and_num_frames(
        ps_rate_control_api->ps_bit_allocation,
        ps_rate_control_api->ps_pic_handling,
        pi4_num_frames));
}
/****************************************************************************
Function Name : update_frame_level_info
Description   : Updates the frame level information into the rate control structure
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
KJN             Original
25 04 2008   Sushmita        Added support to get different bits for model
updation & buffer updation.May be used,in case encoder
decides to follow strict VBV compliance and hence
skips a picture after encoding it.Since it has
statistics of the current picture also we update
the model based on the discarded picture's stats
and the buffer model on the basis of actual bits
consumed by skipped picture
*****************************************************************************/
void update_frame_level_info(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    LWORD64 *pi8_mb_type_sad, /* Frame level SAD for each type of MB[Intra/Inter] */
    WORD32 i4_total_frame_bits, /* Total frame bits actually consumed */
    WORD32 i4_model_updation_hdr_bits, /*header bits for model updation*/
    WORD32 *
        pi4_mb_type_tex_bits, /* Total texture bits consumed for each type of MB[Intra/Inter] used for model */
    LWORD64 *pi8_tot_mb_type_qp_q6, /* Total qp of all MBs based on mb type */
    WORD32 *pi4_tot_mb_in_type, /* total number of mbs in each mb type */
    WORD32 i4_avg_activity, /* Average mb activity in frame */
    UWORD8 u1_is_scd, /* Is a scene change detected at the current frame */
    WORD32 i4_is_it_a_skip,
    WORD32 i4_intra_frm_cost,
    WORD32
        i4_is_pic_handling_done, /* If picture handling is not done then update pic handling module. Special case for staggered endcoding */
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_bits_to_be_stuffed,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_lap_window_comp,
    WORD32 i4_is_end_of_period,
    WORD32 i4_lap_based_comp_reset,
    frame_info_t *ps_frame_info,
    WORD32 i4_is_rc_model_needs_to_be_updated,
    WORD8 i1_qp_offset,
    WORD32 i4_scene_num,
    WORD32 i4_num_frm_enc_in_scene,
    WORD32 i4_est_text_bits_ctr_update_qp)
{
    UWORD8 u1_num_skips = 0;
    WORD32 i;
    /*picture_type_e  e_orig_pic_type = e_pic_type;*/
    LWORD64 i8_frame_sad = 0; /* Frame level SAD */
    WORD32 i4_tot_texture_bits = 0; /* Total texture bits consumed */
    WORD32 i4_tot_mbs = 0; /* Total number of mbs in frame */
    LWORD64 i8_avg_qp = 0, i8_avg_qp_q6 = 0;
    WORD32 i4_flag_rc_model_update = (i4_is_rc_model_needs_to_be_updated == 1);
    WORD32 i4_gop_correction = 0, i4_new_correction = 0;

    ps_frame_info->i4_flag_rc_model_update = i4_flag_rc_model_update;
    ps_frame_info->i4_num_entries++;
    trace_printf(
        "update pic_type = %d      tbc = %d   hbc = %d\n",
        e_pic_type,
        (i4_total_frame_bits - i4_model_updation_hdr_bits),
        i4_model_updation_hdr_bits);
    /* NOTE KJN: SCD not supported in case of B Frames */
    if(u1_is_scd && (e_pic_type != I_PIC && e_pic_type != P_PIC))
    {
        u1_is_scd = 0;
    }

    /*if both pause to resume and scene cut is signalled then ignore pause to resume flag*/
    if(u1_is_scd && i4_is_pause_to_resume)
        i4_is_pause_to_resume = 0;

    if(!i4_is_it_a_skip && !i4_is_pic_handling_done)
    {
        /* Update the pic_handling struct */
        /*: do not update pic handling even in case of non-reference B-PIC*/
        update_pic_handling(
            ps_rate_control_api->ps_pic_handling, e_pic_type, i4_suppress_bpic_update, u1_is_scd);
    }
    {
        WORD32 *pi4_qp_array =
            ps_rate_control_api
                ->ai4_prev_frm_qp[(i4_scene_num + HALF_MAX_SCENE_NUM_RC) % MAX_SCENE_NUM_RC];
        WORD32 *pi4_qp_array_q6 =
            ps_rate_control_api
                ->ai4_prev_frm_qp_q6[(i4_scene_num + HALF_MAX_SCENE_NUM_RC) % MAX_SCENE_NUM_RC];
        WORD32 i4_i;
        for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
        {
            pi4_qp_array[i4_i] = 0x7FFFFFFF;
            pi4_qp_array_q6[i4_i] = 0x7FFFFFFF;
        }
    }

    if(ps_rate_control_api->e_rc_type == CONST_QP)
    {
        if(!i4_is_it_a_skip)
        {
            /******************************************************************
            Calculate the total values from the individual values
            ******************************************************************/
            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_frame_sad += pi8_mb_type_sad[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i4_tot_texture_bits += pi4_mb_type_tex_bits[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_avg_qp += (pi8_tot_mb_type_qp_q6[i] >> 6);

            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_avg_qp_q6 += pi8_tot_mb_type_qp_q6[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i4_tot_mbs += pi4_tot_mb_in_type[i];
            i8_avg_qp /= i4_tot_mbs; /* Calculate the average QP */
            i8_avg_qp_q6 /= i4_tot_mbs;

            if(ps_rate_control_api->u1_is_mb_level_rc_on)
            {
                /* The model needs to take into consideration the average activity of the
               entire frame while estimating the QP. Thus the frame sad values are scaled by
               the average activity before updating it into the model.*/
                if(!i4_avg_activity)
                    i4_avg_activity = 1;
                i4_intra_frm_cost /= i4_avg_activity;
                i8_frame_sad /= i4_avg_activity;
            }

            ps_frame_info->i8_frame_num = get_num_frms_encoded(ps_rate_control_api->ps_cbr_buffer);
            ps_frame_info->i4_num_entries++;

            update_cbr_buffer(
                ps_rate_control_api->ps_cbr_buffer,
                (i4_total_frame_bits + i4_bits_to_be_stuffed),
                e_pic_type);
        }
    }

    if(ps_rate_control_api->e_rc_type != CONST_QP)
    {
        /* For improving CBR streams quality */
        WORD32 i4_buffer_based_bit_error = 0;

        if(!i4_is_it_a_skip)
        {
            WORD32 i4_new_period_flag;
            /******************************************************************
            Calculate the total values from the individual values
            ******************************************************************/
            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_frame_sad += pi8_mb_type_sad[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i4_tot_texture_bits += pi4_mb_type_tex_bits[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_avg_qp += (pi8_tot_mb_type_qp_q6[i] >> 6);

            for(i = 0; i < MAX_MB_TYPE; i++)
                i8_avg_qp_q6 += pi8_tot_mb_type_qp_q6[i];
            for(i = 0; i < MAX_MB_TYPE; i++)
                i4_tot_mbs += pi4_tot_mb_in_type[i];
            i8_avg_qp /= i4_tot_mbs; /* Calculate the average QP */
            i8_avg_qp_q6 /= i4_tot_mbs;

            if(ps_rate_control_api->u1_is_mb_level_rc_on)
            {
                /* The model needs to take into consideration the average activity of the
               entire frame while estimating the QP. Thus the frame sad values are scaled by
               the average activity before updating it into the model.*/
                if(!i4_avg_activity)
                    i4_avg_activity = 1;
                i4_intra_frm_cost /= i4_avg_activity;
                i8_frame_sad /= i4_avg_activity;
            }

            ps_frame_info->i8_frame_num = get_num_frms_encoded(ps_rate_control_api->ps_cbr_buffer);
            ps_frame_info->i4_num_entries++;
            /******************************************************************
            Update the bit allocation module
            NOTE: For bit allocation module, the pic_type should not be modified
            to that of 'I', in case of a SCD.
            ******************************************************************/
            i4_new_period_flag = is_last_frame_in_gop(ps_rate_control_api->ps_pic_handling);

            update_cur_frm_consumed_bits(
                ps_rate_control_api->ps_bit_allocation,
                ps_rate_control_api->ps_pic_handling,
                ps_rate_control_api->ps_cbr_buffer,
                i4_total_frame_bits,
                /*((ps_rate_control_api->e_rc_type == CBR_NLDRC)?(i4_total_frame_bits + i4_bits_to_be_stuffed):i4_total_frame_bits)*/  //account for stuffing bits even when encoder does not stuff in case of CBR
                i4_model_updation_hdr_bits,
                e_pic_type,
                u1_is_scd,
                i4_is_end_of_period,
                i4_lap_based_comp_reset,
                i4_suppress_bpic_update,
                i4_buffer_based_bit_error,
                i4_bits_to_be_stuffed,
                i4_lap_window_comp,
                ps_rate_control_api->e_rc_type,
                ps_rate_control_api->i4_num_gop,
                i4_is_pause_to_resume,
                i4_est_text_bits_ctr_update_qp,
                &i4_gop_correction,
                &i4_new_correction);
            if(1 == i4_new_period_flag &&
               ((ps_rate_control_api->e_rc_type == VBR_STORAGE) ||
                (ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP)))
            {
                check_and_update_bit_allocation(
                    ps_rate_control_api->ps_bit_allocation,
                    ps_rate_control_api->ps_pic_handling,
                    get_max_bits_inflow_per_frm_periode(ps_rate_control_api->ps_vbr_storage_vbv));
            }
        }

        /******************************************************************
        Update the buffer status
        ******************************************************************/
        /* This updation is done after overflow and underflow handling to
        account for the actual bits dumped*/
        if((ps_rate_control_api->e_rc_type == VBR_STORAGE) ||
           (ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP))
        {
            update_vbr_vbv(ps_rate_control_api->ps_vbr_storage_vbv, i4_total_frame_bits);
        }
        else if(
            ps_rate_control_api->e_rc_type == CBR_NLDRC ||
            ps_rate_control_api->e_rc_type == VBR_STREAMING)
        {
            update_cbr_buffer(
                ps_rate_control_api->ps_cbr_buffer,
                (i4_total_frame_bits + i4_bits_to_be_stuffed),
                e_pic_type);
        }

        if(e_pic_type != B_PIC || e_pic_type != B1_PIC || e_pic_type != B2_PIC)
        {
            ps_rate_control_api->i4_prev_ref_is_scd = 0;
        }

        if(!i4_is_it_a_skip)
        {
            /******************************************************************
            Handle the SCENE CHANGE DETECTED
            1) Make the picture type as I, so that updation happens as if it is
            a I frame
            2) Reset model, SAD and flag to restart the estimation process
            ******************************************************************/
            if(u1_is_scd || ps_rate_control_api->u1_is_first_frm)
            {
                e_pic_type = I_PIC;

                /* Reset the SAD estimation module */
                reset_est_sad(ps_rate_control_api->ps_est_sad);

                /*remember the previous reference as SCD. This is required to trigger quering model for B
                 * frames with delay one sub-gop*/
                ps_rate_control_api->i4_prev_ref_is_scd = 1;

                /* Reset the MB Rate control */
                init_mb_level_rc(ps_rate_control_api->ps_mb_rate_control);

                /* Adjust the average QP for the frame based on bits consumption */
                /* Initialize the QP for each picture type according to the average QP of the SCD pic */
                ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] = (WORD32)i8_avg_qp;

                ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] = (WORD32)i8_avg_qp_q6;

                ps_rate_control_api->i4_frames_since_last_scd = 0;

                ps_rate_control_api->f_p_to_i_comp_ratio = 1.0f;
                /* Reset the number of header bits in a scene change */
                //init_prev_header_bits(ps_rate_control_api->ps_bit_allocation, ps_rate_control_api->ps_pic_handling);
            }
            else if(i4_is_pause_to_resume)
            {
                reset_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[e_pic_type]);  //ELP_RC
            }
            if(i8_frame_sad && (!i4_suppress_bpic_update))
            {
                /********************************************************************
                Update the model of the correponding picture type
                NOTE: For SCD, we force the frame type from 'P' to that of a 'I'
                *********************************************************************/
                /* For very simple sequences no bits are consumed by texture. These frames
                do not add any information to the model and so not added.
                Update the model only when there is atleast 1 texture bit for every mb in a frame */
                WORD32 i4_tot_texture_bits_added_to_model = i4_tot_texture_bits;
                /*update the model only if bits consumed are zero. If this is zero qp for next frame has to be reduced until
                 * it provides some texture bits to update model*/

                if(i4_tot_texture_bits_added_to_model > 0 && (i4_flag_rc_model_update == 1))
                {
                    add_frame_to_rd_model(
                        ps_rate_control_api->aps_rd_model[e_pic_type],
                        i4_tot_texture_bits_added_to_model,
                        (WORD32)i8_avg_qp_q6,
                        i8_frame_sad,
                        u1_num_skips);

                    {
                        number_t temp =
                            get_linear_coefficient(ps_rate_control_api->aps_rd_model[e_pic_type]);
                        ps_frame_info->model_coeff_a_lin_wo_int.e = temp.e;
                        ps_frame_info->model_coeff_a_lin_wo_int.sm = temp.sm;
                    }
                }

                /******************************************************************
                Update the sad estimation module
                NOTE: For SCD, we force the frame type from 'P' to that of a 'I'
                ******************************************************************/
                update_actual_sad(
                    ps_rate_control_api->ps_est_sad, (UWORD32)i8_frame_sad, e_pic_type);
                /*: This will update I pic sad with current pic intra SAD. Now for non I-PIC the intra sad is coming same as
                 *best sad. This will corrupt intra frame sad. So not updating this. I frame SAD is updated only at I pic */

                /* Atleast one proper frame in added into the model. Until that
                keep using the initial QP */

                /*B frames immediatly encoded after scene cut may still belong to previous content, When B frames encoded after one P frame after SCD are guranteed to belong
                 * new scene, modeling these frames wrt previous B frames might give wrong results. To avoid this model for B frame is not queried unless it is guranteed that one B frame
                 * has been modeled with new content. So setting is_first_frm_coded for B frames with delay of one frame*/
                /*In HEVC implementation it is guranteed to encode new scene after scene cut I pic*/
                ps_rate_control_api->au1_is_first_frm_coded[e_pic_type] = 1;
            }

            if(i4_avg_activity)
            {
                /* Update the mb_level model */
                mb_update_frame_level(ps_rate_control_api->ps_mb_rate_control, i4_avg_activity);
            }
            /* Update the variable which denotes that a frame has been encountered */
            ps_rate_control_api->u1_is_first_frm = 0;
            ps_rate_control_api->i4_frames_since_last_scd++;
        }
    }
    return;
}
/* SGI & Enc Loop Parallelism related changes*/
/****************************************************************************
Function Name : update_frame_rc_get_frame_qp_info
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void update_frame_rc_get_frame_qp_info(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_is_scd,
    WORD32 i4_is_pause_to_resume,
    WORD32 i4_avg_frame_qp_q6,
    WORD32 i4_suppress_bpic_update,
    WORD32 i4_scene_num,
    WORD32 i4_num_frm_enc_in_scene)
{
    WORD32 i4_avg_qp = 0, i4_avg_qp_q6 = 0;

    i4_avg_qp = (i4_avg_frame_qp_q6 >> 6);
    i4_avg_qp_q6 = i4_avg_frame_qp_q6;

    if(i4_is_scd && (e_pic_type != I_PIC && e_pic_type != P_PIC))
    {
        i4_is_scd = 0;
    }

    if(e_pic_type == I_PIC)
    {
        ps_rate_control_api->i4_I_frame_qp_model = is_first_frame_coded(ps_rate_control_api);
    }
    if((i4_is_scd && i4_is_pause_to_resume))  //KISH
        i4_is_pause_to_resume = 0;

    if(i4_is_scd || ps_rate_control_api->u1_is_first_frm)
    {
        /* Save previous B-QP since some B-pics may follow detection of SCD */

        e_pic_type = I_PIC;

        /* Reset the SAD estimation module */
        reset_est_sad(ps_rate_control_api->ps_est_sad);

        /*remember the previous reference as SCD. This is required to trigger quering model for B
        * frames with delay one sub-gop*/
        ps_rate_control_api->i4_prev_ref_is_scd = 1;

        /* Reset the MB Rate control */
        init_mb_level_rc(ps_rate_control_api->ps_mb_rate_control);

        /* Adjust the average QP for the frame based on bits consumption */
        /* Initialize the QP for each picture type according to the average QP of the SCD pic */
        ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] = i4_avg_qp;

        ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] = i4_avg_qp_q6;
    }
    else if(i4_is_pause_to_resume)
    {
        /*pause to resume is guranteed to be P_PIC*/
        ASSERT(e_pic_type != I_PIC);

        /* re-set all models eccept for I PIC model */
        /*for(i=1;i<MAX_PIC_TYPE;i++)
        {
        reset_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[i]);
        ps_rate_control_api->au1_is_first_frm_coded[i] = 0;
        }*/
        /*resetting only current frame model instead of resetting all models*/
        /*TO DO: i4_is_pause_to_resume is misnomer, as even non I scd are also handled in similar way*/
        //reset_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[e_pic_type]);
        ps_rate_control_api->au1_is_first_frm_coded[e_pic_type] = 0;
        ps_rate_control_api->i4_frames_since_last_scd = 0;

        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][e_pic_type] = i4_avg_qp;
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type] = i4_avg_qp_q6;
        }
        /*also reset previous I pic Qp since it uses I frame qp for qp determination when model is reset*/
        if(e_pic_type == I_PIC)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] = i4_avg_qp;
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] = i4_avg_qp_q6;
        }
        else if(e_pic_type == P_PIC || e_pic_type == P1_PIC)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp * P_TO_I_RATIO) >> K_Q;
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp_q6 * P_TO_I_RATIO) >> K_Q;
        }
        else if(e_pic_type == B_PIC || e_pic_type == BB_PIC)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp * P_TO_I_RATIO * P_TO_I_RATIO) >> (K_Q + K_Q);
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp_q6 * P_TO_I_RATIO * P_TO_I_RATIO) >> (K_Q + K_Q);
        }
        else if(e_pic_type == B1_PIC || e_pic_type == B11_PIC)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp * P_TO_I_RATIO * P_TO_I_RATIO * P_TO_I_RATIO) >>
                (K_Q + K_Q + K_Q);
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp_q6 * P_TO_I_RATIO * P_TO_I_RATIO * P_TO_I_RATIO) >>
                (K_Q + K_Q + K_Q);
        }
        else if(e_pic_type == B2_PIC || e_pic_type == B22_PIC)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp * P_TO_I_RATIO * P_TO_I_RATIO * P_TO_I_RATIO * P_TO_I_RATIO) >>
                (K_Q + K_Q + K_Q + K_Q);
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][I_PIC] =
                ((LWORD64)i4_avg_qp_q6 * P_TO_I_RATIO * P_TO_I_RATIO * P_TO_I_RATIO *
                 P_TO_I_RATIO) >>
                (K_Q + K_Q + K_Q + K_Q);
        }
    }
    else
    {
#if 1 /* Prev QP updation has happened at the end of the get frame qp call itself */
        /******************************************************************
        Update the Qp used by the current frame
        ******************************************************************/
        if(!i4_suppress_bpic_update)
        {
            ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][e_pic_type] = i4_avg_qp;
            ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][e_pic_type] = i4_avg_qp_q6;
            trace_printf("Prev frame qp q6 update %d pic type %d", i4_avg_qp_q6, e_pic_type);
        }
#endif
    }

    if(i4_num_frm_enc_in_scene == 1)
    {
        WORD32 i4_i = 0;
        for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
        {
            if(ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][i4_i] == 0x7FFFFFFF)
            {
                ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][i4_i] = i4_avg_qp;
                ps_rate_control_api->ai4_prev_frm_qp_q6[i4_scene_num][i4_i] = i4_avg_qp_q6;
            }
        }
    }

    if((!i4_suppress_bpic_update))
    {
        /*B frames immediatly encoded after scene cut may still belong to previous content, When B frames encoded after one P frame after SCD are guranteed to belong
        * new scene, modeling these frames wrt previous B frames might give wrong results. To avoid this model for B frame is not queried unless it is guranteed that one B frame
        * has been modeled with new content. So setting is_first_frm_coded for B frames with delay of one frame*/
        /*In HEVC implementation it is guranteed to encode new scene after scene cut I pic*/
        //ps_rate_control_api->au1_is_first_frm_coded[e_pic_type] = 1; //KISH_ELP
    }

    /* Update the variable which denotes that a frame has been encountered */
    ps_rate_control_api->u1_is_first_frm = 0;

    /* Store the prev encoded picture type for restricting Qp swing */
    if((e_pic_type == I_PIC) || (e_pic_type == P_PIC))
    {
        ps_rate_control_api->prev_ref_pic_type = e_pic_type;
    }

    return;
}

/*update previous frame intra sad */
/****************************************************************************
Function Name : rc_update_prev_frame_intra_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_update_prev_frame_intra_sad(
    rate_control_api_t *ps_rate_control_api, WORD32 i4_intra_frame_sad)
{
    update_prev_frame_intra_sad(ps_rate_control_api->ps_est_sad, i4_intra_frame_sad);
}
/****************************************************************************
Function Name : rc_get_prev_frame_intra_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_get_prev_frame_intra_sad(rate_control_api_t *ps_rate_control_api)
{
    return get_prev_frame_intra_sad(ps_rate_control_api->ps_est_sad);
}
/*update previous frame sad */
/****************************************************************************
Function Name : rc_update_prev_frame_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_update_prev_frame_sad(
    rate_control_api_t *ps_rate_control_api, WORD32 i4_frame_sad, picture_type_e e_pic_type)
{
    update_prev_frame_sad(ps_rate_control_api->ps_est_sad, i4_frame_sad, e_pic_type);
}
/****************************************************************************
Function Name : rc_get_prev_frame_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_get_prev_frame_sad(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    return get_prev_frame_sad(ps_rate_control_api->ps_est_sad, e_pic_type);
}

/****************************************************************************
Function Name : reset_rc_for_pause_to_play_transition
Description   : In this mode it resets RC only for P and B picture, since the
                sequece has not changed but only the motion related changes would
                take impact
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void reset_rc_for_pause_to_play_transition(rate_control_api_t *ps_rate_control_api)
{
    WORD32 i;
    /* re-set model only for P and B frame */
    for(i = 1; i < MAX_PIC_TYPE; i++)
    {
        reset_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[i]);
    }
    /* Reset flag */
    for(i = 1; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->au1_is_first_frm_coded[i] = 0;
    }
}
/****************************************************************************
Function Name : get_rc_target_bits
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_rc_target_bits(rate_control_api_t *ps_rate_control_api)
{
    return (ps_rate_control_api->i4_prev_frm_est_bits);
}
/****************************************************************************
Function Name : get_orig_rc_target_bits
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_orig_rc_target_bits(rate_control_api_t *ps_rate_control_api)
{
    return (ps_rate_control_api->i4_orig_frm_est_bits);
}

#if NON_STEADSTATE_CODE
/******************************************************************************
MB Level API functions
******************************************************************************/
/****************************************************************************
Function Name : init_mb_rc_frame_level
Description   : Initialise the frame level details required for a mb level
Inputs        : u1_frame_qp - Frame Qp that is to be used to the current frame
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/

void init_mb_rc_frame_level(rate_control_api_t *ps_rate_control_api, UWORD8 u1_frame_qp)
{
    mb_init_frame_level(ps_rate_control_api->ps_mb_rate_control, u1_frame_qp);
}
#endif /* #if NON_STEADSTATE_CODE */

/****************************************************************************
Function Name : get_bits_to_stuff
Description   : Gets the bits to stuff to prevent Underflow of Encoder Buffer
Inputs        : Rate control API ctxt , total consumed bits
Globals       :
Processing    :
Outputs       : number of bits to stuff
Returns       : i4_bits_to_stuff
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_bits_to_stuff(
    rate_control_api_t *ps_rate_control_api, WORD32 i4_tot_consumed_bits, picture_type_e e_pic_type)
{
    WORD32 i4_bits_to_stuff;
    /* Get the CBR bits to stuff*/
    i4_bits_to_stuff =
        get_cbr_bits_to_stuff(ps_rate_control_api->ps_cbr_buffer, i4_tot_consumed_bits, e_pic_type);
    return i4_bits_to_stuff;
}

/****************************************************************************
Function Name : get_prev_frm_est_bits
Description   : Returns previous frame estimated bits
Inputs        : Rate control API ctxt
Globals       :
Processing    :
Outputs       : previous frame estimated bits
Returns       : i4_prev_frm_est_bits
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 get_prev_frm_est_bits(rate_control_api_t *ps_rate_control_api)
{
    return (ps_rate_control_api->i4_prev_frm_est_bits);
}

/****************************************************************************
Function Name : change_frm_rate_for_bit_alloc
Description   : Does the necessary changes only in the bit_allocation module
there is a change in frame rate
Inputs        : u4_frame_rate - new frame rate to be used
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void change_frm_rate_for_bit_alloc(rate_control_api_t *ps_rate_control_api, UWORD32 u4_frame_rate)
{
    if(ps_rate_control_api->e_rc_type != CONST_QP)
    {
        /* Bit Allocation Module: distribute the excess/deficit bits between the
           old and the new frame rate to all the remaining frames */
        change_remaining_bits_in_period(
            ps_rate_control_api->ps_bit_allocation,
            ba_get_bit_rate(ps_rate_control_api->ps_bit_allocation),
            u4_frame_rate,
            (WORD32 *)(ps_rate_control_api->au4_new_peak_bit_rate));
    }
}

/****************************************************************************
 * Function Name : rc_get_rem_bits_in_gop
 * Description   : API call to get remaining bits in GOP
 * *****************************************************************************/
WORD32 rc_get_rem_bits_in_period(rate_control_api_t *ps_rate_control_api)
{
    return (get_rem_bits_in_period(
        ps_rate_control_api->ps_bit_allocation, ps_rate_control_api->ps_pic_handling));
}

/****************************************************************************
  Function Name : flush_buf_frames
Description   : API call to flush the buffered up frames
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void flush_buf_frames(rate_control_api_t *ps_rate_control_api)
{
    flush_frame_from_pic_stack(ps_rate_control_api->ps_pic_handling);
}

/****************************************************************************
Function Name : rc_get_prev_header_bits
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_get_prev_header_bits(rate_control_api_t *ps_rate_control_api, WORD32 pic_type)
{
    return (get_prev_header_bits(ps_rate_control_api->ps_bit_allocation, pic_type));
}
/****************************************************************************
Function Name : rc_get_prev_P_QP
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_get_prev_P_QP(rate_control_api_t *ps_rate_control_api, WORD32 i4_scene_num)
{
    WORD32 i4_prev_qp = ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][P_PIC];
    i4_prev_qp =
        (ps_rate_control_api->i4_P_to_I_ratio * i4_prev_qp + (1 << (P_TO_I_RATIO_Q_FACTOR - 1))) >>
        P_TO_I_RATIO_Q_FACTOR;
    return (i4_prev_qp);
}
/****************************************************************************
Function Name : rc_put_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_put_sad(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_cur_intra_sad,
    WORD32 i4_cur_sad,
    WORD32 i4_cur_pic_type)
{
    sad_acc_put_sad(ps_rate_control_api->ps_sad_acc, i4_cur_intra_sad, i4_cur_sad, i4_cur_pic_type);
}
/****************************************************************************
Function Name : rc_get_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_get_sad(rate_control_api_t *ps_rate_control_api, WORD32 *pi4_sad)
{
    sad_acc_get_sad(ps_rate_control_api->ps_sad_acc, pi4_sad);
}
/****************************************************************************
Function Name : rc_update_ppic_sad
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_update_ppic_sad(
    rate_control_api_t *ps_rate_control_api, WORD32 i4_est_sad, WORD32 i4_prev_ppic_sad)
{
    return (update_ppic_sad(ps_rate_control_api->ps_est_sad, i4_est_sad, i4_prev_ppic_sad));
}
/****************************************************************************
Function Name : change_avg_bit_rate
Description   : Whenever the average bit rate changes, the excess bits is
between the changed bit rate and the old one is re-distributed
in the bit allocation module
Inputs        : u4_average_bit_rate - new average bit rate to be used
              : u4_peak_bit_rate - new peak bit rate to be used
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void change_avg_bit_rate(
    rate_control_api_t *ps_rate_control_api, UWORD32 u4_average_bit_rate, UWORD32 u4_peak_bit_rate)
{
    int i;

    if(ps_rate_control_api->e_rc_type != CONST_QP)
    {
        if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
        {
            ps_rate_control_api->au4_new_peak_bit_rate[0] = u4_average_bit_rate;
            ps_rate_control_api->au4_new_peak_bit_rate[1] = u4_average_bit_rate;
        }
        else
        {
            ps_rate_control_api->au4_new_peak_bit_rate[0] = u4_peak_bit_rate;
            ps_rate_control_api->au4_new_peak_bit_rate[1] = u4_peak_bit_rate;
        }
        /* Bit Allocation Module: distribute the excess/deficit bits between the
        old and the new frame rate to all the remaining frames */
        change_remaining_bits_in_period(
            ps_rate_control_api->ps_bit_allocation,
            u4_average_bit_rate,
            ba_get_frame_rate(ps_rate_control_api->ps_bit_allocation),
            (WORD32 *)(ps_rate_control_api->au4_new_peak_bit_rate));
    }
    //if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
    {
        UWORD32 u4_average_bit_rate_copy[MAX_NUM_DRAIN_RATES];
        /*DYNAMIC_RC*/
        //ps_rate_control_api->au4_new_peak_bit_rate[0]=u4_average_bit_rate;
        //ps_rate_control_api->au4_new_peak_bit_rate[1]=u4_average_bit_rate;
        for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        {
            u4_average_bit_rate_copy[i] = u4_average_bit_rate;
        }
        change_cbr_vbv_bit_rate(
            ps_rate_control_api->ps_cbr_buffer,
            (WORD32 *)(u4_average_bit_rate_copy),
            (WORD32)ps_rate_control_api->au4_new_peak_bit_rate[0]);
    }

    /* This is done only for average bitrate changing somewhere after the model stabilises.
       Here it is assumed that user will not do this call after first few frames.
       If we dont have this check, what would happen is since the model has not stabilised, also
       bitrate has changed before the first frame, we dont restrict the qp. Qp can go to
       very bad values after init qp since if swing is disabled

    */
    if(ps_rate_control_api->u1_is_first_frm == 0)
    {
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            /*This also makes sure the qp swing restrictions wont be applied at boundary of bitrate change*/
            ps_rate_control_api->au1_avg_bitrate_changed[i] = 1;
        }
    }
}

#if NON_STEADSTATE_CODE
/******************************************************************************
Control Level API functions
Logic: The control call sets the state structure of the rate control api
accordingly such that the next process call would implement the same.
******************************************************************************/

/******************************************************************************
Function Name   : change_inter_frm_int_call
Description     :
Arguments       :
Return Values   : void
Revision History:
Creation

  Assumptions   -

    Checks      -
*****************************************************************************/
void change_inter_frm_int_call(rate_control_api_t *ps_rate_control_api, WORD32 i4_inter_frm_int)
{
    pic_handling_register_new_inter_frm_interval(
        ps_rate_control_api->ps_pic_handling, i4_inter_frm_int);
}
/******************************************************************************
Function Name   : change_intra_frm_int_call
Description     :
Arguments       :
Return Values   : void
Revision History:
Creation

  Assumptions   -

    Checks      -
*****************************************************************************/
void change_intra_frm_int_call(rate_control_api_t *ps_rate_control_api, WORD32 i4_intra_frm_int)
{
    pic_handling_register_new_int_frm_interval(
        ps_rate_control_api->ps_pic_handling, i4_intra_frm_int);

    if(ps_rate_control_api->e_rc_type == VBR_STREAMING)
    {
        change_vsp_ifi(&ps_rate_control_api->s_vbr_str_prms, i4_intra_frm_int);
    }
}

/****************************************************************************
Function Name : change_frame_rate
Description   : Does the necessary changes whenever there is a change in
frame rate
Inputs        : u4_frame_rate - new frame rate to be used
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void change_frame_rate(
    rate_control_api_t *ps_rate_control_api,
    UWORD32 u4_frame_rate,
    UWORD32 u4_src_ticks,
    UWORD32 u4_tgt_ticks)
{
    if(ps_rate_control_api->e_rc_type != CONST_QP)
    {
        UWORD32 u4_frms_in_delay_prd =
            ((u4_frame_rate * get_cbr_buffer_delay(ps_rate_control_api->ps_cbr_buffer)) / 1000000);
        if((ps_rate_control_api->e_rc_type == VBR_STORAGE) ||
           (ps_rate_control_api->e_rc_type == VBR_STORAGE_DVD_COMP))
        {
            change_vbr_vbv_frame_rate(ps_rate_control_api->ps_vbr_storage_vbv, u4_frame_rate);
        }
        else if(ps_rate_control_api->e_rc_type == CBR_NLDRC)
        {
            change_cbr_vbv_tgt_frame_rate(ps_rate_control_api->ps_cbr_buffer, u4_frame_rate);
        }
        else if(ps_rate_control_api->e_rc_type == VBR_STREAMING)
        {
            UWORD32 au4_num_pics_in_delay_prd[MAX_PIC_TYPE];
            change_vsp_tgt_ticks(&ps_rate_control_api->s_vbr_str_prms, u4_tgt_ticks);
            change_vsp_src_ticks(&ps_rate_control_api->s_vbr_str_prms, u4_src_ticks);
            change_vsp_fidp(&ps_rate_control_api->s_vbr_str_prms, u4_frms_in_delay_prd);

            change_cbr_vbv_tgt_frame_rate(ps_rate_control_api->ps_cbr_buffer, u4_frame_rate);
            change_cbr_vbv_num_pics_in_delay_period(
                ps_rate_control_api->ps_cbr_buffer, au4_num_pics_in_delay_prd);
        }

        /* Bit Allocation Module: distribute the excess/deficit bits between the
        old and the new frame rate to all the remaining frames */
        change_remaining_bits_in_period(
            ps_rate_control_api->ps_bit_allocation,
            ba_get_bit_rate(ps_rate_control_api->ps_bit_allocation),
            u4_frame_rate,
            (WORD32 *)(ps_rate_control_api->au4_new_peak_bit_rate));
    }
}
/****************************************************************************
Function Name : change_init_qp
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void change_init_qp(
    rate_control_api_t *ps_rate_control_api, WORD32 *pi4_init_qp, WORD32 i4_scene_num)
{
    WORD32 i;
    /* Initialize the init_qp */
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->ai4_prev_frm_qp[i4_scene_num][i] = pi4_init_qp[i];
    }
}

/****************************************************************************
Function Name : change_min_max_qp
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void change_min_max_qp(rate_control_api_t *ps_rate_control_api, WORD32 *pi4_min_max_qp)
{
    WORD32 i;
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_rate_control_api->ai4_min_qp[i] = pi4_min_max_qp[(i << 1)];
        ps_rate_control_api->ai4_max_qp[i] = pi4_min_max_qp[(i << 1) + 1];
    }

    change_init_qp_max_qp(ps_rate_control_api->ps_init_qp, pi4_min_max_qp);
}
/****************************************************************************
Function Name : rc_get_frame_rate
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
/* Getter functions to get the current rate control parameters */
UWORD32 rc_get_frame_rate(rate_control_api_t *ps_rate_control_api)
{
    return (ba_get_frame_rate(ps_rate_control_api->ps_bit_allocation));
}
/****************************************************************************
Function Name : rc_get_bit_rate
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_bit_rate(rate_control_api_t *ps_rate_control_api)
{
    return (ba_get_bit_rate(ps_rate_control_api->ps_bit_allocation));
}
/****************************************************************************
Function Name : rc_get_peak_bit_rate
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_peak_bit_rate(rate_control_api_t *ps_rate_control_api, WORD32 i4_index)
{
    return (ps_rate_control_api->au4_new_peak_bit_rate[i4_index]);
}
/****************************************************************************
Function Name : rc_get_intra_frame_interval
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_intra_frame_interval(rate_control_api_t *ps_rate_control_api)
{
    return (pic_type_get_intra_frame_interval(ps_rate_control_api->ps_pic_handling));
}
/****************************************************************************
Function Name : rc_get_inter_frame_interval
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_inter_frame_interval(rate_control_api_t *ps_rate_control_api)
{
    return (pic_type_get_inter_frame_interval(ps_rate_control_api->ps_pic_handling));
}
/****************************************************************************
Function Name : rc_get_rc_type
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
rc_type_e rc_get_rc_type(rate_control_api_t *ps_rate_control_api)
{
    return (ps_rate_control_api->e_rc_type);
}
/****************************************************************************
Function Name : rc_get_bits_per_frame
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
WORD32 rc_get_bits_per_frame(rate_control_api_t *ps_rate_control_api)
{
    WORD32 i4_bits_per_frm;

    X_PROD_Y_DIV_Z(
        ba_get_bit_rate(ps_rate_control_api->ps_bit_allocation),
        (UWORD32)1000,
        ba_get_frame_rate(ps_rate_control_api->ps_bit_allocation),
        i4_bits_per_frm);

    return (i4_bits_per_frm);
}
/****************************************************************************
Function Name : rc_get_max_delay
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_max_delay(rate_control_api_t *ps_rate_control_api)
{
    return (get_cbr_buffer_delay(ps_rate_control_api->ps_cbr_buffer));
}
/****************************************************************************
Function Name : rc_get_seq_no
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_seq_no(rate_control_api_t *ps_rate_control_api)
{
    return (pic_type_get_disp_order_no(ps_rate_control_api->ps_pic_handling));
}
/****************************************************************************
Function Name : rc_get_rem_frames_in_gop
Description   :
Inputs        : ps_rate_control_api
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
UWORD32 rc_get_rem_frames_in_gop(rate_control_api_t *ps_rate_control_api)
{
    /* Get the rem_frms_in_gop & the frms_in_gop from the pic_type state struct */
    return (pic_type_get_rem_frms_in_gop(ps_rate_control_api->ps_pic_handling));
}

/****************************************************************************
  Function Name : flush_buf_frames
Description   : API call to flush the buffered up frames
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void post_encode_frame_skip(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    skip_encoded_frame(ps_rate_control_api->ps_pic_handling, e_pic_type);
}

/****************************************************************************
  Function Name : force_I_frame
  Description   : API call to force an I frame
 *****************************************************************************/
void force_I_frame(rate_control_api_t *ps_rate_control_api)
{
    set_force_I_frame_flag(ps_rate_control_api->ps_pic_handling);
}

/****************************************************************************
 * Function Name : rc_get_vbv_buf_fullness
 * Description   : API call to get VBV buffer fullness
 ******************************************************************************/
WORD32 rc_get_vbv_buf_fullness(rate_control_api_t *ps_rate_control_api)
{
    return (get_cur_vbv_buf_size(ps_rate_control_api->ps_vbr_storage_vbv));
}
/****************************************************************************
 * Function Name : rc_get_cur_peak_factor_2pass
 * Description   : API call to get current peak factor
 ******************************************************************************/
float rc_get_cur_peak_factor_2pass(rate_control_api_t *ps_rate_control_api)
{
    return (get_cur_peak_factor_2pass(ps_rate_control_api->ps_bit_allocation));
}
/****************************************************************************
 * Function Name : rc_get_min_complexity_factor_2pass
 * Description   : API call to get minimm complexity factor
 ******************************************************************************/
float rc_get_min_complexity_factor_2pass(rate_control_api_t *ps_rate_control_api)
{
    return (get_cur_min_complexity_factor_2pass(ps_rate_control_api->ps_bit_allocation));
}
/****************************************************************************
 * Function Name : rc_get_vbv_buf_size
 * Description   : API call to get VBV buffer size
 ******************************************************************************/
WORD32 rc_get_vbv_buf_size(rate_control_api_t *ps_rate_control_api)
{
    return (get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer));
}
/****************************************************************************
 * Function Name : rc_get_vbv_fulness_with_cur_bits
 * Description   : API call to get VBV buffer fullness with current bits
 ******************************************************************************/
WORD32 rc_get_vbv_fulness_with_cur_bits(rate_control_api_t *ps_rate_control_api, UWORD32 u4_bits)
{
    return (get_vbv_buf_fullness(ps_rate_control_api->ps_vbr_storage_vbv, u4_bits));
}
/****************************************************************************
 * Function Name : rc_set_avg_mb_act
 * Description   :
 ******************************************************************************/
void rc_set_avg_mb_act(rate_control_api_t *ps_rate_control_api, WORD32 i4_avg_activity)
{
    mb_update_frame_level(ps_rate_control_api->ps_mb_rate_control, i4_avg_activity);
    return;
}
/****************************************************************************
 * Function Name : rc_init_set_ebf
 * Description   : API call to set EBF
 ******************************************************************************/
void rc_init_set_ebf(rate_control_api_t *ps_rate_control_api, WORD32 i32_init_ebf)
{
    set_cbr_ebf(ps_rate_control_api->ps_cbr_buffer, i32_init_ebf);
}
#endif /* #if NON_STEADSTATE_CODE */

/****************************************************************************
Function Name : rc_get_qp_scene_change_bits
Description   : HEVC specific function to get scene change qp at scene cut location
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

WORD32 rc_get_qp_scene_change_bits(
    rate_control_handle ps_rate_control_api,
    WORD32 i4_total_bits,
    LWORD64 i8_satd_by_act_accum,
    WORD32 i4_num_pixel,
    void *offline_model_coeff,
    float f_i_to_average_rest,
    WORD32 i4_call_type)
{
    float f_trial_q_scale;
    WORD32 i4_tex_bits = 0, i4_header_bits = 0;
    WORD32 error = 0, min_error = 0x7FFFFFFF, i4_is_high_bitrate = 0;
    double *model_coeff, min_error_q_scale = (double)127;
    double min_scd_qscale, max_scd_q_scale;
    WORD32 i4_QP, i4_max_Qp, i4_min_Qp, i4_qp_selection_flag = 0;
    WORD32 i4_prev_best = -1;

    /*The qp calculation here is based on offline generated stat for around 30 frames belonging to different scene
      The I only mode of encode was done for the above sequence for qp range {8,51}. A quadratic and cubic curve was obtained
      based on the stat geneated.
      eq coeff*/
    float coeff_a, coeff_b, coeff_c, coeff_d, X, tex_bpp;
    float min_qp_qscale_multiplier =
        1; /*For fade-in fade-out case where scene starts with blank frame have higher min frame qp*/
    //float head_per;
    float normal_satd_act;
    float bpp = (float)get_bits_per_frame(ps_rate_control_api->ps_bit_allocation) / i4_num_pixel;

    if(i4_num_pixel > 5000000) /*UHD*/
    {
        if(bpp > 0.12) /*30mbp 2160 30p*/
            i4_is_high_bitrate = 1;
        else if(bpp > 0.06)
            i4_is_high_bitrate = 2;
        else if(bpp > 0.03)
            i4_is_high_bitrate = 3;
    }
    else
    {
        if(bpp > 0.16) /*10mbps 1080 30p*/
            i4_is_high_bitrate = 1;
        else if(bpp > 0.08)
            i4_is_high_bitrate = 2;
        else if(bpp > 0.04)
            i4_is_high_bitrate = 3;
    }
    /*Min qp and Max qp at scene cut is critical since offline models are not reliable always*/
    /*During fade-in fade-out when LAP places I frame on blank pictures but the content slowly changes to complicated content, Due to low
      spatial complxity of I pic a very low SCD qp will be allocated, qp swing restriction will not give enough frames to increase qp to high value
      to encode such fast motion inter pictiures .Hence whenever temporal complexity is very high assume some least spatial complexity so that very low qp
      is not chosen*/
    if(f_i_to_average_rest < I_TO_REST_VVFAST &&
       (i4_is_high_bitrate !=
        1)) /*The I_TO_AVERAGE RATIO generally comes very low, hence this wont be measure of extent on motion in inter pictures*/
    {
        WORD32 i4_min_num_pixel = i4_num_pixel;

        if(i4_num_pixel > 5000000)
        {
            i4_min_num_pixel = i4_min_num_pixel / 2;
        }

        if(i8_satd_by_act_accum <
           i4_num_pixel) /*In very fast motion case have min threshold for I frame, Assume atleast one unit per pixel sad*/
        {
            if(i4_is_high_bitrate == 2)
            {
                i8_satd_by_act_accum = (LWORD64)(i4_min_num_pixel / 2);
            }
            else if(i4_is_high_bitrate == 3)
            {
                i8_satd_by_act_accum = (LWORD64)(i4_min_num_pixel * 3.0f / 4.0f);
            }
            else
                i8_satd_by_act_accum = (LWORD64)(i4_min_num_pixel);

            min_qp_qscale_multiplier = (float)pow(
                (float)1.125f,
                (WORD32)6);  //this will make min qp for simple frame with high moiton 24 instead of 18
        }
    }
    min_scd_qscale = pow(2, (double)(ps_rate_control_api->u4_min_scd_hevc_qp - 4) / 6) *
                     min_qp_qscale_multiplier;
    max_scd_q_scale = pow(2, (double)(SCD_MAX_HEVC_QP - 4) / 6);
    i4_max_Qp = MAX_HEVC_QP;
    i4_min_Qp = ps_rate_control_api->u4_min_scd_hevc_qp;
    if((ps_rate_control_api->u1_bit_depth > 8) && (i4_call_type == 1))
    {
        i8_satd_by_act_accum = i8_satd_by_act_accum << (ps_rate_control_api->u1_bit_depth - 8);
        i4_max_Qp = i4_max_Qp + (6 * (ps_rate_control_api->u1_bit_depth - 8));
        i4_min_Qp = i4_min_Qp + (6 * (ps_rate_control_api->u1_bit_depth - 8));
        max_scd_q_scale = max_scd_q_scale * (1 << (ps_rate_control_api->u1_bit_depth - 8));
    }

    normal_satd_act = (float)i8_satd_by_act_accum / i4_num_pixel;

    {
        /* Max satd/act at L0 was taken at qp 18 for
        480p    - 4410520
        720p    - 9664235
        1080p   - 15735650
        4k      - 50316472
        A curve was generated using these points
        */

        float f_satd_by_Act_norm = GET_L0_SATD_BY_ACT_MAX_PER_PIXEL(i4_num_pixel);
        float f_weigh_factor = 0.0f;
        f_satd_by_Act_norm = f_satd_by_Act_norm * 0.75f;
        f_weigh_factor = GET_WEIGH_FACTOR_FOR_MIN_SCD_Q_SCALE(normal_satd_act, f_satd_by_Act_norm);
        CLIP(f_weigh_factor, 1.0f, 1.0f / MULT_FACTOR_SATD);
        min_scd_qscale = min_scd_qscale * f_weigh_factor;
        CLIP(min_scd_qscale, max_scd_q_scale, 1);
    }

    /*coeff value based on input resolution
        1920x1090 -> 207360,1280x720->921600,720x480->345600(unlike for I_REST_AVG_BIT_RATIO here 720x480 was considered as low resolution)
        ultra high res = num_pixek > 5000000
        high_res = num_pxel > 1500000
        mid res  = num_pixel > 600000
        low_res  = num_pixel < 600000
        The fit is based on HEVC qp value between 18 and 48 inclusive
        */
    /*adding coeff for ultra HD resolution*/
    /*
    High quality bpp vs nor satd/act/qp
    --------------------------------------
    480p  y = -0.1823x3 + 0.5258x2 + 1.7707x - 0.0394
    720p  y = -0.1458x3 + 0.4039x2 + 1.8817x - 0.0648
    1080p y = -0.4712x3 + 1.3818x2 + 1.2797x - 0.0262
    2160p y = -1.1234x3 + 2.6328x2 + 0.8817x - 0.0047


    Medium speed
    ------------
    480p  y = -0.1567x3 + 0.4222x2 + 1.8899x - 0.0537
    720p  y = -0.1417x3 + 0.3699x2 + 1.9611x - 0.0766
    1080p y = -0.4841x3 + 1.4123x2 + 1.2981x - 0.0321
    2160p y = -1.1989x3 + 2.7935x2 + 0.8648x - 0.0074

    High speed
    -------------
    480p  y = -0.1611x3 + 0.4418x2 + 1.8754x - 0.0524
    720p  y = -0.1455x3 + 0.3854x2 + 1.951x - 0.0753
    1080p y = -0.4908x3 + 1.4344x2 + 1.2848x - 0.031
    2160p y = -1.2037x3 + 2.8062x2 + 0.8551x - 0.0067
    */
    model_coeff = (double *)offline_model_coeff;
    coeff_a = (float)model_coeff[0];
    coeff_b = (float)model_coeff[1];
    coeff_c = (float)model_coeff[2];
    coeff_d = (float)model_coeff[3];
    for(i4_QP = i4_min_Qp; i4_QP < i4_max_Qp; i4_QP++)
    {
        /*needs to use the array for qp to qscale */

        f_trial_q_scale = (float)(pow(2.0, (i4_QP - 4.0) / 6.0));
        /* curve fit for texture bits*/
        X = (float)normal_satd_act / f_trial_q_scale;
        tex_bpp = ((coeff_a * X * X * X) + (coeff_b * X * X) + (coeff_c * X) + coeff_d);
        if(tex_bpp < (float)((1 << 30)) / i4_num_pixel)
            i4_tex_bits = (tex_bpp * i4_num_pixel);
        else
            i4_tex_bits = (1 << 30);
        i4_header_bits = 0;
        if(i4_tex_bits > 0)
        {
            /*QP increase can't cause increase in bits*/
            if(i4_prev_best != -1 && (i4_tex_bits > i4_prev_best))
            {
                min_error = 0x7FFFFFFF;
                i4_qp_selection_flag = 0;
            }
            /*consider texture bits to get header bits using obtained header percentage. Using header bits on overall bits targetted might not be correct*/
            error = i4_total_bits - (i4_tex_bits + i4_header_bits);
            if(abs(error) < abs(min_error))
            {
                min_error = error;
                min_error_q_scale = f_trial_q_scale;
                i4_qp_selection_flag = 1;
                i4_prev_best = i4_tex_bits;
            }
        }
    }
    if(!i4_qp_selection_flag)
    {
        min_error_q_scale = (WORD32)(min_scd_qscale + 0.5);
    }
    //if((ps_rate_control_api->u1_bit_depth > 8)&& (i4_call_type == 1))
    //  min_error_q_scale = min_error_q_scale / (1 << (ps_rate_control_api->u1_bit_depth - 8));

    /*offline stat generation range considered is mpeg2qp 5 to 161 or hevc qp 18 to 48*/
    CLIP(min_error_q_scale, (WORD32)(max_scd_q_scale + 0.5), (WORD32)(min_scd_qscale + .5));
    return ((WORD32)(min_error_q_scale * (1 << QSCALE_Q_FAC_3)));
}

/****************************************************************************
Function Name : rc_get_qp_for_scd_frame
Description   : Get qp for a scene cut frame
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_get_qp_for_scd_frame(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    LWORD64 i8_satd_act_accum,
    WORD32 i4_num_pels_in_frame,
    WORD32 i4_est_I_pic_head_bits,
    WORD32 i4_f_sim_lap_avg,
    void *offline_model_coeff,
    float i_to_avg_ratio,
    WORD32 i4_true_scd,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    frame_info_t *ps_frame_stat,
    WORD32 i4_rc_2_pass,
    WORD32 i4_is_not_an_I_pic,
    WORD32 i4_ref_first_pass,
    WORD32 i4_call_type,
    WORD32 *pi4_cur_est_tot_bits,
    WORD32 *pi4_tot_bits_estimated,
    WORD32 i4_use_offline_model_2pass,
    LWORD64 *pi8_i_tex_bits,
    float *pf_i_qs,
    WORD32 i4_best_br_id,
    WORD32 *pi4_estimate_to_calc_frm_error)
{
    WORD32 i4_qs_q3, i4_buf_based_min_bits, i4_buf_based_max_bits, i4_cur_est_tot_bits,
        i4_est_texture_bits, i4_get_error = 0;
    float f_bits_ratio;

    assign_complexity_coeffs(ps_rate_control_api->ps_bit_allocation, af_sum_weigh);

    i4_cur_est_tot_bits = get_scene_change_tot_frm_bits(
        ps_rate_control_api->ps_bit_allocation,
        ps_rate_control_api->ps_pic_handling,
        ps_rate_control_api->ps_cbr_buffer,
        i4_num_pels_in_frame,
        i4_f_sim_lap_avg,
        i_to_avg_ratio,
        i4_call_type,
        i4_is_not_an_I_pic,
        ps_rate_control_api->i4_is_infinite_gop);
    if(i4_call_type == 1)
    {
        *pi4_estimate_to_calc_frm_error = i4_cur_est_tot_bits;
    }

    /* vbv buffer position based error correction to keep away encoder buffer overflow at layer 0 pictures*/
    if(e_pic_type == I_PIC || e_pic_type == P_PIC || e_pic_type == P1_PIC)
    {
        WORD32 i4_cur_ebf = get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer);
        WORD32 i4_vbv_size = get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer);
        WORD32 i4_max_ebf = (WORD32)(i4_vbv_size * MAX_THRESHOLD_VBV_FRM_ERROR);
        WORD32 i4_drain_rate = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
        WORD32 i4_total_bits_allocted = i4_cur_est_tot_bits;
        WORD32 i4_total_bits_to_be_alloc;
        WORD32 i4_expected_ebf = (i4_cur_ebf + i4_total_bits_allocted - i4_drain_rate);
        /*if expected ebf is greater than max threashold, correct the allocation such that it never cross max
        but if it less than drain rate, atleast give drainrate bits*/
        if(i4_expected_ebf > i4_max_ebf)
        {
            i4_total_bits_to_be_alloc =
                MAX(i4_drain_rate, (i4_total_bits_allocted - (i4_expected_ebf - i4_max_ebf)));
            i4_cur_est_tot_bits = i4_total_bits_to_be_alloc;
        }
    }
    if(i4_call_type == 1)
    {
        i4_get_error = rc_get_estimate_bit_error(ps_rate_control_api);
    }
    if(i4_est_I_pic_head_bits != -1)
    /*get constraints from buffer*/
    {
        get_min_max_bits_based_on_buffer(
            ps_rate_control_api,
            e_pic_type,
            &i4_buf_based_min_bits,
            &i4_buf_based_max_bits,
            i4_get_error);
        if(i4_cur_est_tot_bits > i4_buf_based_max_bits)
            i4_cur_est_tot_bits = i4_buf_based_max_bits;
        if((i4_cur_est_tot_bits < i4_buf_based_min_bits) && (i_to_avg_ratio > 8.0))
            i4_cur_est_tot_bits = i4_buf_based_min_bits;
    }
    if(i4_est_I_pic_head_bits <
       0)  //indicates header bits data is not available. Assume default ratio
    {
        i4_est_texture_bits = (i4_cur_est_tot_bits * DEFAULT_TEX_PERCENTAGE_Q5) >> 5;
        i4_est_I_pic_head_bits = i4_cur_est_tot_bits - i4_est_texture_bits;
    }
    if((i4_cur_est_tot_bits - i4_est_I_pic_head_bits) < 0)
        i4_cur_est_tot_bits = i4_est_I_pic_head_bits;

    *pi4_tot_bits_estimated = i4_cur_est_tot_bits;

    if(i4_true_scd)
    {
        /*texture bits should be atleast 25% of header bits*/
        if(i4_cur_est_tot_bits < (1.25 * i4_est_I_pic_head_bits))
            i4_cur_est_tot_bits = (WORD32)(1.25 * i4_est_I_pic_head_bits);

        ps_rate_control_api->i4_scd_I_frame_estimated_tot_bits = i4_cur_est_tot_bits;
    }

    /* Get qp for scene cut frame based on offline generated data*/

    i4_qs_q3 = rc_get_qp_scene_change_bits(
        ps_rate_control_api,
        (i4_cur_est_tot_bits - i4_est_I_pic_head_bits),
        i8_satd_act_accum,
        i4_num_pels_in_frame,
        offline_model_coeff,
        i_to_avg_ratio,
        i4_call_type);

    if(i4_call_type)
        trace_printf(
            "i4_qp %d, i8_satd_act_accum %I64d,i_to_avg_ratio %f, "
            "i4_est_I_pic_head_bits %d i4_cur_est_tot_bits %d\n",
            i4_qp,
            i8_satd_act_accum,
            i_to_avg_ratio,
            i4_est_I_pic_head_bits,
            i4_cur_est_tot_bits);

    *pi4_cur_est_tot_bits = i4_cur_est_tot_bits;

    return (i4_qs_q3);
}

/****************************************************************************
Function Name : rc_set_num_scd_in_lap_window
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_num_scd_in_lap_window(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_num_scd_in_lap_window,
    WORD32 i4_num_frames_b4_scd)
{
    bit_allocation_set_num_scd_lap_window(
        ps_rate_control_api->ps_bit_allocation, i4_num_scd_in_lap_window, i4_num_frames_b4_scd);
}
/****************************************************************************
Function Name : rc_set_next_sc_i_in_rc_look_ahead
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_next_sc_i_in_rc_look_ahead(
    rate_control_api_t *ps_rate_control_api, WORD32 i4_next_sc_i_in_rc_look_ahead)
{
    bit_allocation_set_sc_i_in_rc_look_ahead(
        ps_rate_control_api->ps_bit_allocation, i4_next_sc_i_in_rc_look_ahead);
}

/****************************************************************************
 * Function Name : rc_update_mismatch_error
 * Description   : API call to update remaining bits in period based on error
 *                  between rdopt bits estimate and actual bits produced in entorpy
 * *****************************************************************************/
void rc_update_mismatch_error(rate_control_api_t *ps_rate_control_api, WORD32 i4_error_bits)
{
    bit_allocation_update_gop_level_bit_error(
        ps_rate_control_api->ps_bit_allocation, i4_error_bits);
    /*Also alter the encoder buffer fullness based on the error*/
    /*error = rdopt - entropy hence subtract form current buffer fullness*/
    update_cbr_buf_mismatch_bit(ps_rate_control_api->ps_cbr_buffer, i4_error_bits);
}
/****************************************************************************
Function Name : rc_set_estimate_status
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_set_estimate_status(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_tex_bits,
    WORD32 i4_hdr_bits,
    WORD32 i4_est_text_bits_ctr_get_qp)
{
    update_estimate_status(
        ps_rate_control_api->ps_bit_allocation,
        i4_tex_bits,
        i4_hdr_bits,
        i4_est_text_bits_ctr_get_qp);

    return i4_tex_bits;
}
/****************************************************************************
Function Name : rc_get_bpp_based_scene_cut_qp
Description   : bpp based qp for a scene cut frame
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_get_bpp_based_scene_cut_qp(
    rate_control_api_t *ps_rate_control_api,
    picture_type_e e_pic_type,
    WORD32 i4_num_pels_in_frame,
    WORD32 i4_f_sim_lap,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    WORD32 i4_call_type)
{
    WORD32 i4_cur_est_texture_bits, i4_cur_est_header_bits, i4_qp, i4_tot_bits,
        i4_buf_based_min_bits, i4_buf_based_max_bits;

    /* Reset the number of header bits in a scene change */
    //init_prev_header_bits(ps_rate_control_api->ps_bit_allocation, ps_rate_control_api->ps_pic_handling);

    /* Get the estimated header bits for the current encoded frame */

    assign_complexity_coeffs(ps_rate_control_api->ps_bit_allocation, af_sum_weigh);
    i4_cur_est_header_bits =
        get_cur_frm_est_header_bits(ps_rate_control_api->ps_bit_allocation, e_pic_type);

    /*get estimate of total bits that can be allocated to I frame based on offline generated data*/
    i4_tot_bits = get_scene_change_tot_frm_bits(
        ps_rate_control_api->ps_bit_allocation,
        ps_rate_control_api->ps_pic_handling,
        ps_rate_control_api->ps_cbr_buffer,
        i4_num_pels_in_frame,
        i4_f_sim_lap,
        (float)8.00,
        0,
        0,
        ps_rate_control_api->i4_is_infinite_gop);

    /* Getting the min and max texture bits based on buffer fullness  and constraining the
    bit allocation based on this */
    if(i4_call_type == 1)
    {
        get_min_max_bits_based_on_buffer(
            ps_rate_control_api, e_pic_type, &i4_buf_based_min_bits, &i4_buf_based_max_bits, 0);
        if(i4_tot_bits > i4_buf_based_max_bits)
            i4_tot_bits = i4_buf_based_max_bits;
        if(i4_tot_bits < i4_buf_based_min_bits)
            i4_tot_bits = i4_buf_based_min_bits;
    }
    /*Assume 30 percent header bits*/
    i4_cur_est_texture_bits = (i4_tot_bits * DEFAULT_TEX_PERCENTAGE_Q5) >> 5;

    /* Get the texture bits assigned to the current frame */
    i4_cur_est_header_bits = i4_tot_bits - i4_cur_est_texture_bits;

    if(i4_cur_est_texture_bits < 0)
        i4_cur_est_texture_bits = 0;

    /* Get the qp for the remaining bits allocated for that frame based on buffer status */
    i4_qp = get_init_qp_using_pels_bits_per_frame(
        ps_rate_control_api->ps_init_qp, I_PIC, i4_cur_est_texture_bits, i4_num_pels_in_frame);
    /* Make sure the qp is with in range */
    if(i4_qp < ps_rate_control_api->ai4_min_qp[e_pic_type])
    {
        i4_qp = ps_rate_control_api->ai4_min_qp[e_pic_type];
    }
    else if(i4_qp > ps_rate_control_api->ai4_max_qp[e_pic_type])
    {
        i4_qp = ps_rate_control_api->ai4_max_qp[e_pic_type];
    }

    return (i4_qp);
}
/****************************************************************************
Function Name : rc_reset_pic_model
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_reset_pic_model(rate_control_api_t *ps_rate_control_api, picture_type_e pic_type)
{
    reset_frm_rc_rd_model(ps_rate_control_api->aps_rd_model[pic_type]);
}
/****************************************************************************
Function Name : rc_reset_first_frame_coded_flag
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_reset_first_frame_coded_flag(
    rate_control_api_t *ps_rate_control_api, picture_type_e pic_type)
{
    ps_rate_control_api->au1_is_first_frm_coded[pic_type] = 0;
}
/****************************************************************************
Function Name : rc_get_scene_change_est_header_bits
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_get_scene_change_est_header_bits(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_num_pixels,
    WORD32 i4_fsim_lap,
    float af_sum_weigh[MAX_PIC_TYPE][3],
    float i_to_avg_ratio)
{
    WORD32 i4_est_tot_bits;

    assign_complexity_coeffs(ps_rate_control_api->ps_bit_allocation, af_sum_weigh);

    i4_est_tot_bits = get_scene_change_tot_frm_bits(
        ps_rate_control_api->ps_bit_allocation,
        ps_rate_control_api->ps_pic_handling,
        ps_rate_control_api->ps_cbr_buffer,
        i4_num_pixels,
        i4_fsim_lap,
        i_to_avg_ratio,
        0,
        0,
        ps_rate_control_api->i4_is_infinite_gop);
    /*return header bits based on default percentage*/
    return (i4_est_tot_bits - ((i4_est_tot_bits * DEFAULT_TEX_PERCENTAGE_Q5) >> 5));
}
/****************************************************************************
Function Name : rc_put_temp_comp_lap
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_put_temp_comp_lap(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_lap_fsim,
    LWORD64 i8_per_pixel_frm_hme_sad_q10,
    picture_type_e e_pic_type)
{
    ps_rate_control_api->i4_lap_f_sim = i4_lap_fsim;
    if(e_pic_type == P_PIC)
    {
        ps_rate_control_api->i8_per_pixel_p_frm_hme_sad_q10 = i8_per_pixel_frm_hme_sad_q10;
    }
}
/****************************************************************************
Function Name : rc_get_pic_distribution
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_get_pic_distribution(rate_control_api_t *ps_rate_control_api, WORD32 *ai4_pic_type)
{
    pic_type_get_frms_in_gop(ps_rate_control_api->ps_pic_handling, ai4_pic_type);
}
/****************************************************************************
Function Name : rc_get_actual_pic_distribution
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_get_actual_pic_distribution(rate_control_api_t *ps_rate_control_api, WORD32 *ai4_pic_type)
{
    pic_type_get_actual_frms_in_gop(ps_rate_control_api->ps_pic_handling, ai4_pic_type);
}
/****************************************************************************
Function Name : rc_reset_Kp_Kb
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_reset_Kp_Kb(
    rate_control_api_t *ps_rate_control_api,
    float f_i_to_avg_rest,
    WORD32 i4_num_active_pic_type,
    float f_curr_hme_sad_per_pixel,
    WORD32 *pi4_complexity_bin,
    WORD32 i4_rc_pass)
{
    reset_Kp_Kb(
        ps_rate_control_api->ps_bit_allocation,
        f_i_to_avg_rest,
        i4_num_active_pic_type,
        f_curr_hme_sad_per_pixel,
        ps_rate_control_api->f_max_hme_sad_per_pixel,
        pi4_complexity_bin,
        i4_rc_pass);
}

/****************************************************************************
Function Name : rc_reset_Kp_Kb
Description   : Get Kp and Kb values for offset at scene cut
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

WORD32 rc_get_kp_kb(rate_control_api_t *ps_rate_control_api, picture_type_e e_pic_type)
{
    return get_Kp_Kb(ps_rate_control_api->ps_bit_allocation, e_pic_type);
}
/****************************************************************************
Function Name : rc_get_ebf
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_get_ebf(rate_control_api_t *ps_rate_control_api)
{
    return (get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer));
}

/****************************************************************************
Function Name : rc_get_offline_normalized_complexity
Description   : The complexities of L1 are normalized with the highest offline
                global complexity
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
float rc_get_offline_normalized_complexity(
    WORD32 i4_intra_period, WORD32 i4_luma_pels, float f_per_pixel_complexity, WORD32 i4_pass_number)
{
    {
        if((i4_luma_pels) > 1500000)
        {
            if(i4_intra_period == 1)
            {
                f_per_pixel_complexity /= (float)3.69;
            }
            else
            {
                /*Full HD and above: Based on running few content, exact data needs to be plugged in*/
                f_per_pixel_complexity /= (float)2.25;
            }
        }
        else if((i4_luma_pels) > 700000)
        {
            if(i4_intra_period == 1)
            {
                f_per_pixel_complexity /= (float)4.28;
            }
            else
            {
                f_per_pixel_complexity /=
                    (float)2.6109;  //the max complexity observed for 720p content of netflix_fountain
            }
        }
        else
        {
            if(i4_intra_period == 1)
                f_per_pixel_complexity /= (float)4.91;
            else
                f_per_pixel_complexity /=
                    (float)3;  //the max complexity observed for 720p content of netflix_fountain
        }
    }
    if(f_per_pixel_complexity > 1.0)
        f_per_pixel_complexity = 1;
    return f_per_pixel_complexity;
}

/****************************************************************************
Function Name : rc_bit_alloc_detect_ebf_stuff_scenario
Description   : To estimate whether there will be a case of underflow based on
                estimated bit consumption and drain rate if there is probability
                of underflow then we will lower the HEVC qp's by 1 based
                on the warning flag.
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/

void rc_bit_alloc_detect_ebf_stuff_scenario(
    rate_control_api_t *ps_rate_control_api,
    WORD32 i4_num_frm_bef_scd_lap2,
    LWORD64 i8_total_bits_est_consu_lap2,
    WORD32 i4_max_inter_frm_int)
{
    WORD32 i4_peak_drain_rate;
    LWORD64 i8_ebf, i8_estimate_ebf_at_end;
    i4_peak_drain_rate = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
    i8_ebf = rc_get_ebf(ps_rate_control_api);
    i8_estimate_ebf_at_end =
        i8_ebf - (i4_num_frm_bef_scd_lap2 * i4_peak_drain_rate) + i8_total_bits_est_consu_lap2;

    ps_rate_control_api->i4_underflow_warning = 0;

    if(i8_estimate_ebf_at_end < (i4_max_inter_frm_int * i4_peak_drain_rate))
    {
        /*If underflow is imminent give a flag*/
        ps_rate_control_api->i4_underflow_warning = 1;
    }
}

/****************************************************************************
Function Name : bit_alloc_get_estimated_bits_for_pic
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

WORD32 bit_alloc_get_estimated_bits_for_pic(
    rate_control_api_t *ps_rate_contro_api,
    WORD32 i4_cur_frm_est_cl_sad,
    WORD32 i4_prev_frm_cl_sad,
    picture_type_e e_pic_type)
{
    WORD32 i4_prev_frame_bits, i4_curnt_frame_est_bits, i4_prev_frame_header_bits;
    get_prev_frame_total_header_bits(
        ps_rate_contro_api->ps_bit_allocation,
        &i4_prev_frame_bits,
        &i4_prev_frame_header_bits,
        e_pic_type);

    i4_curnt_frame_est_bits = (WORD32)(
        ((float)(i4_prev_frame_bits - i4_prev_frame_header_bits) * (float)i4_cur_frm_est_cl_sad /
         (float)i4_prev_frm_cl_sad) +
        i4_prev_frame_header_bits);
    return (i4_curnt_frame_est_bits);
}

/****************************************************************************
Function Name : rc_get_max_hme_sad_per_pixel
Description   : At init time based on parameters we pick the max hme sad per pixel.
Inputs        :
Globals       :
Processing    :
Outputs       :
Returns       :
Issues        :
Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)

*****************************************************************************/
void rc_get_max_hme_sad_per_pixel(rate_control_api_t *ps_rate_control_api, WORD32 i4_total_pixels)
{
    WORD32 i, i4_error = 0x7FFFFFFF, i4_temp_error, i4_res_index = 0, i4_br_index = 0;
    WORD32 i4_num_temporal_layers;
    /*Max hme sad per pixel based on resolutions, num. of temporal layers (0-3) and also bpp-> whether low bitrate or high bitrate*/
    float af_offline_hme_sad_per_pixel_480p[4][2] = {
        { 2.94f, 2.63f }, { 2.96f, 2.44f }, { 2.72f, 1.94f }, { 2.70f, 2.04f }
    };
    float af_offline_hme_sad_per_pixel_720p[4][2] = {
        { 3.37f, 2.97f }, { 3.35f, 2.77f }, { 3.18f, 2.40f }, { 2.94f, 1.83f }
    };
    float af_offline_hme_sad_per_pixel_1080p[4][2] = {
        { 3.24f, 2.78f }, { 3.17f, 2.46f }, { 2.91f, 1.98f }, { 2.75f, 1.65f }
    };
    float af_offline_hme_sad_per_pixel_2160p[4][2] = {
        { 2.56f, 2.11f }, { 2.47f, 1.92f }, { 2.19f, 1.46f }, { 2.00f, 1.21f }
    };

    /*Low BR or HBR is decided by comparing the bpp values as below*/
    float af_offline_bpp[4][2] = {
        { 0.30f, 0.09f }, { 0.25f, 0.06f }, { 0.16f, 0.04f }, { 0.12f, 0.02f }
    };

    /*Number of pixels in the picture for picking the closest resolution*/
    WORD32 ai4_pixels_res[4] = { 307200, 921600, 2073600, 8294400 };

    float f_bpp =
        (float)get_bits_per_frame(ps_rate_control_api->ps_bit_allocation) / i4_total_pixels;
    float f_max_hme_sad_per_pixel;

    i4_num_temporal_layers = ps_rate_control_api->i4_num_active_pic_type - 2;

    CLIP(i4_num_temporal_layers, 3, 0);

    /*Pick the closest resolution based on error*/
    for(i = 0; i < 4; i++)
    {
        i4_temp_error = abs(i4_total_pixels - ai4_pixels_res[i]);

        if(i4_temp_error < i4_error)
        {
            i4_error = i4_temp_error;
            i4_res_index = i;
        }
    }

    /*Decide whether LBR or HBR*/
    if((fabs(af_offline_bpp[i4_res_index][0] - f_bpp)) >
       (fabs(af_offline_bpp[i4_res_index][1] - f_bpp)))
    {
        i4_br_index = 1;
    }
    else
    {
        i4_br_index = 0;
    }

    /*After that pick the max hme sad*/
    switch(i4_res_index)
    {
    case 0:
        f_max_hme_sad_per_pixel =
            af_offline_hme_sad_per_pixel_480p[i4_num_temporal_layers][i4_br_index];
        break;
    case 1:
        f_max_hme_sad_per_pixel =
            af_offline_hme_sad_per_pixel_720p[i4_num_temporal_layers][i4_br_index];
        break;
    case 2:
        f_max_hme_sad_per_pixel =
            af_offline_hme_sad_per_pixel_1080p[i4_num_temporal_layers][i4_br_index];
        break;
    case 3:
        f_max_hme_sad_per_pixel =
            af_offline_hme_sad_per_pixel_2160p[i4_num_temporal_layers][i4_br_index];
        break;
    default:
        f_max_hme_sad_per_pixel =
            af_offline_hme_sad_per_pixel_1080p[i4_num_temporal_layers][i4_br_index];
        break;
    }

    ps_rate_control_api->f_max_hme_sad_per_pixel = f_max_hme_sad_per_pixel;
}

/****************************************************************************
Function Name : rc_update_pic_distn_lap_to_rc
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_update_pic_distn_lap_to_rc(
    rate_control_api_t *ps_rate_contro_api, WORD32 ai4_num_pic_type[MAX_PIC_TYPE])
{
    pic_type_update_frms_in_gop(ps_rate_contro_api->ps_pic_handling, ai4_num_pic_type);
}

/****************************************************************************
Function Name : rc_set_bits_based_on_complexity
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_bits_based_on_complexity(
    rate_control_api_t *ps_rate_contro_api, WORD32 i4_lap_window_comp, WORD32 i4_num_frames)
{
    set_bit_allocation_i_frames(
        ps_rate_contro_api->ps_bit_allocation,
        ps_rate_contro_api->ps_cbr_buffer,
        ps_rate_contro_api->ps_pic_handling,
        i4_lap_window_comp,
        i4_num_frames);
}
/****************************************************************************
Function Name : rc_set_avg_qscale_first_pass
Description   : Set the average qscale from first pass
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

void rc_set_avg_qscale_first_pass(
    rate_control_api_t *ps_rate_control_api, float f_average_qscale_1st_pass)
{
    ba_set_avg_qscale_first_pass(ps_rate_control_api->ps_bit_allocation, f_average_qscale_1st_pass);
}
/****************************************************************************
Function Name : rc_set_max_avg_qscale_first_pass
Description   : Set the maximum avergae Qscale in second pass as average Qscale
                of first pass + 6 This is for simple contents
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

void rc_set_max_avg_qscale_first_pass(
    rate_control_api_t *ps_rate_control_api, float f_max_average_qscale_1st_pass)
{
    ba_set_max_avg_qscale_first_pass(
        ps_rate_control_api->ps_bit_allocation, f_max_average_qscale_1st_pass);
}
/****************************************************************************
Function Name : rc_set_i_to_sum_api_ba
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_i_to_sum_api_ba(rate_control_api_t *ps_rate_control_api, float f_curr_i_to_sum)
{
    bit_alloc_set_curr_i_to_sum_i(ps_rate_control_api->ps_bit_allocation, f_curr_i_to_sum);
}
/****************************************************************************
Function Name : rc_set_p_to_i_complexity_ratio
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_p_to_i_complexity_ratio(rate_control_api_t *ps_rate_control_api, float f_p_to_i_ratio)
{
    ps_rate_control_api->f_p_to_i_comp_ratio = f_p_to_i_ratio;
}
/****************************************************************************
Function Name : rc_set_scd_in_period
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_scd_in_period(rate_control_api_t *ps_rate_control_api, WORD32 i4_scd_in_period)
{
    ps_rate_control_api->i4_scd_in_period_2_pass = i4_scd_in_period;
}
/****************************************************************************
Function Name : rc_ba_get_qp_offset_offline_data
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_ba_get_qp_offset_offline_data(
    rate_control_api_t *ps_rate_control_api,
    WORD32 ai4_offsets[5],
    float f_hme_sad_per_pixel,
    WORD32 i4_num_active_pic_type,
    WORD32 *pi4_complexity_bin)
{
    WORD32 i4_ratio;
    float f_ratio;

    CLIP(f_hme_sad_per_pixel, ps_rate_control_api->f_max_hme_sad_per_pixel, 0.01f);

    i4_ratio = (WORD32)(ps_rate_control_api->f_max_hme_sad_per_pixel / f_hme_sad_per_pixel);
    f_ratio = ps_rate_control_api->f_max_hme_sad_per_pixel / f_hme_sad_per_pixel;

    ba_get_qp_offset_offline_data(
        ai4_offsets, i4_ratio, f_ratio, i4_num_active_pic_type, pi4_complexity_bin);
}
/****************************************************************************
Function Name : rc_api_gop_level_averagae_q_scale_without_offset
Description   : Find the GOP level average of the Qscale
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

float rc_api_gop_level_averagae_q_scale_without_offset(rate_control_api_t *ps_rate_control_api)
{
    float f_hbd_qscale =
        ba_gop_info_average_qscale_gop_without_offset(ps_rate_control_api->ps_bit_allocation);

    return (f_hbd_qscale);
}

/****************************************************************************
Function Name : rc_getprev_ref_pic_type
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
picture_type_e rc_getprev_ref_pic_type(rate_control_api_t *ps_rate_control_api)
{
    return (ps_rate_control_api->prev_ref_pic_type);
}
/****************************************************************************
Function Name : rc_get_actual_intra_frame_int
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
WORD32 rc_get_actual_intra_frame_int(rate_control_api_t *ps_rate_control_api)
{
    WORD32 i4_int = pic_type_get_actual_intra_frame_interval(ps_rate_control_api->ps_pic_handling);
    return (i4_int);
}
/****************************************************************************
Function Name : rc_get_qscale_max_clip_in_second_pass
Description   : Get maximum qscale that is allowed based on average Qp for simple contents
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

float rc_get_qscale_max_clip_in_second_pass(rate_control_api_t *ps_rate_control_api)
{
    float i4_max_qscale =
        ba_get_qscale_max_clip_in_second_pass(ps_rate_control_api->ps_bit_allocation);
    return (i4_max_qscale);
}
/****************************************************************************
Function Name : rc_set_2pass_total_frames
Description   : Set the total number of frames in the stream
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

void rc_set_2pass_total_frames(rate_control_api_t *ps_rate_control_api, WORD32 i4_total_2pass_frames)
{
    bit_alloc_set_2pass_total_frames(ps_rate_control_api->ps_bit_allocation, i4_total_2pass_frames);
}
/****************************************************************************
Function Name : rc_set_2pass_avg_bit_rate
Description   : Set the average bit-rate based on consumption so far
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/

void rc_set_2pass_avg_bit_rate(
    rate_control_api_t *ps_rate_control_api, LWORD64 i8_2pass_avg_bit_rate)
{
    ba_set_2pass_avg_bit_rate(ps_rate_control_api->ps_bit_allocation, i8_2pass_avg_bit_rate);
}
/****************************************************************************
Function Name : rc_set_enable_look_ahead
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_set_enable_look_ahead(rate_control_api_t *ps_rate_control_api, WORD32 i4_enable_look_ahead)
{
    ba_set_enable_look_ahead(ps_rate_control_api->ps_bit_allocation, i4_enable_look_ahead);
}
/****************************************************************************
Function Name : rc_add_est_tot
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_add_est_tot(rate_control_api_t *ps_rate_control_api, WORD32 i4_tot_tex_bits)
{
    rc_modify_est_tot(ps_rate_control_api, i4_tot_tex_bits);
}
/****************************************************************************
Function Name : rc_init_buffer_info
Description   :
Inputs        : ps_rate_control_api

Revision History:
DD MM YYYY   Author(s)       Changes (Describe the changes made)
 *****************************************************************************/
void rc_init_buffer_info(
    rate_control_api_t *ps_rate_control_api,
    WORD32 *pi4_vbv_buffer_size,
    WORD32 *pi4_currEbf,
    WORD32 *pi4_maxEbf,
    WORD32 *pi4_drain_rate)
{
    *pi4_vbv_buffer_size = get_cbr_buffer_size(ps_rate_control_api->ps_cbr_buffer);
    *pi4_currEbf = get_cbr_ebf(ps_rate_control_api->ps_cbr_buffer) +
                   rc_get_estimate_bit_error(ps_rate_control_api);
    *pi4_maxEbf = get_cbr_max_ebf(ps_rate_control_api->ps_cbr_buffer);
    *pi4_drain_rate = get_buf_max_drain_rate(ps_rate_control_api->ps_cbr_buffer);
    return;
}
