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
* \file cbr_buffer_control.c
*
* \brief
*    This file contains all functions needed for cbr buffer control
* \date
* 06/05/2008
*
* \author
*    ittiam
*
*  \List of Functions
*                      init_cbr_buffer
*                      cbr_buffer_constraint_check
*                      get_cbr_buffer_status
*                      update_cbr_buffer
*
******************************************************************************
*/
/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>

/* User include files */
#include "assert.h"
#include "ittiam_datatypes.h"
#include "rc_cntrl_param.h"
#include "rc_common.h"
#include "mem_req_and_acq.h"
#include "fixed_point_error_bits.h"
#include "cbr_buffer_control.h"
#include "trace_support.h"
#include "var_q_operator.h"

#define MIN(x, y) ((x) < (y)) ? (x) : (y)
/*allow a maximum of 20 percent deviation when input is very large*/
#define VBR_MAX_BIT_DEV_SEC 50LL

typedef struct cbr_buffer_t
{
    WORD32 i4_buffer_size; /* Buffer size = Delay * Bitrate*/
    WORD32
    i4_drain_bits_per_frame[MAX_NUM_DRAIN_RATES]; /* Constant drain rate */
    WORD32 i4_ebf; /* Encoder Buffer Fullness */
    LWORD64
    i8_ebf_bit_alloc; /* current encoder buffer fulness that accounts precise bit consumption (not truncated to max buffer size at skip)*/
    LWORD64 i8_credit_level;
    WORD32 i4_upr_thr[MAX_PIC_TYPE]; /* Upper threshold of the Buffer */
    WORD32 i4_low_thr[MAX_PIC_TYPE]; /* Lower threshold of the Buffer */
    error_bits_handle
        aps_bpf_error_bits[MAX_NUM_DRAIN_RATES]; /* For error due to bits per frame calculation */
    WORD32
    i4_is_cbr_mode; /* Whether the buffer model is used for CBR or VBR streaming */
    /* Input parameters stored for initialisation */
    WORD32 ai4_bit_rate[MAX_NUM_DRAIN_RATES];
    WORD32 i4_max_delay;
    WORD32 ai4_num_pics_in_delay_period[MAX_PIC_TYPE];
    WORD32 i4_tgt_frm_rate;
    UWORD32 u4_max_vbv_buf_size;
    WORD32 i4_peak_drain_rate_frame;
    WORD32 u4_num_frms_in_delay;
    UWORD32 u4_vbr_max_bit_deviation;
    rc_type_e e_rc_type;
    WORD32 i4_vbr_no_peak_rate_duration_limit;
    LWORD64 i8_tot_frm_to_be_encoded;
    LWORD64
    i8_num_frames_encoded; /*need to track the number of frames encoded to calculate possible deviaiton allowed*/
    WORD32 i4_cbr_rc_pass;
    WORD32 i4_inter_frame_int;
    WORD32 i4_intra_frame_int;
    WORD32 i4_capped_vbr_on;
    float f_max_dur_peak_rate;
    LWORD64 i4_ebf_estimate;
} cbr_buffer_t;

#if NON_STEADSTATE_CODE
WORD32 cbr_buffer_num_fill_use_free_memtab(
    cbr_buffer_t **pps_cbr_buffer, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0, i;
    static cbr_buffer_t s_cbr_buffer_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_cbr_buffer) = &s_cbr_buffer_temp;

    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(cbr_buffer_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_cbr_buffer, e_func_type);
    }
    i4_mem_tab_idx++;

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        i4_mem_tab_idx += error_bits_num_fill_use_free_memtab(
            &pps_cbr_buffer[0]->aps_bpf_error_bits[i], &ps_memtab[i4_mem_tab_idx], e_func_type);
    }
    return (i4_mem_tab_idx);
}
static void set_upper_lower_vbv_threshold(cbr_buffer_t *ps_cbr_buffer, WORD32 i4_bits_per_frm)
{
    WORD32 i;
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_cbr_buffer->i4_upr_thr[i] =
            (WORD32)(((LWORD64)ps_cbr_buffer->i4_buffer_size >> 4) * UPPER_THRESHOLD_EBF_Q4);
        if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
        {
            /*lower threshold can be zero as there is no problem of studffing in this mode (VBR STORAGE)*/
            if(ps_cbr_buffer->i4_vbr_no_peak_rate_duration_limit)
                ps_cbr_buffer->i4_low_thr[i] = 0;
            else
                ps_cbr_buffer->i4_low_thr[i] = ps_cbr_buffer->i4_inter_frame_int * i4_bits_per_frm;
        }
        else
        {
            if(ps_cbr_buffer->i4_inter_frame_int == 1)
                ps_cbr_buffer->i4_low_thr[i] = 0;
            else
            {
                ps_cbr_buffer->i4_low_thr[i] = ps_cbr_buffer->i4_inter_frame_int * i4_bits_per_frm;
            }
        }
        /*For huge buffer low limit can be higher*/

        if(ps_cbr_buffer->i4_low_thr[i] < (ps_cbr_buffer->i4_buffer_size >> 6))
            ps_cbr_buffer->i4_low_thr[i] = (ps_cbr_buffer->i4_buffer_size >> 6);

        if(ps_cbr_buffer->i4_low_thr[i] > (ps_cbr_buffer->i4_buffer_size >> 3))  //KISH_DEBUG
            ps_cbr_buffer->i4_low_thr[i] = (ps_cbr_buffer->i4_buffer_size >> 3);
        ASSERT(ps_cbr_buffer->i4_upr_thr[i] > ps_cbr_buffer->i4_low_thr[i]);
    }
}
/* ******************************************************************************/
/**
 * @brief Initialise the CBR VBV buffer state.
 * This could however be used for VBR streaming VBV also
 *
 * @param ps_cbr_buffer
 * @param i4_buffer_delay
 * @param i4_tgt_frm_rate
 * @param i4_bit_rate
 * @param u4_num_pics_in_delay_prd
 * @param u4_vbv_buf_size
 */
/* ******************************************************************************/
void init_cbr_buffer(
    cbr_buffer_t *ps_cbr_buffer,
    WORD32 i4_buffer_delay,
    WORD32 i4_tgt_frm_rate,
    UWORD32 u4_bit_rate,
    UWORD32 *u4_num_pics_in_delay_prd,
    UWORD32 u4_vbv_buf_size,
    UWORD32 u4_intra_frm_int,
    rc_type_e e_rc_type,
    UWORD32 u4_peak_bit_rate,
    UWORD32 u4_num_frames_in_delay,
    float f_max_dur_peak_rate,
    LWORD64 i8_num_frames_to_encode,
    WORD32 i4_inter_frm_int,
    WORD32 i4_cbr_rc_pass,
    WORD32 i4_capped_vbr_flag)

{
    WORD32 i4_bits_per_frm[MAX_NUM_DRAIN_RATES];
    int i;

    /* Initially Encoder buffer fullness is zero */
    ps_cbr_buffer->i4_ebf = 0;
    ps_cbr_buffer->i4_ebf_estimate = 0;
    ps_cbr_buffer->i8_ebf_bit_alloc = 0;
    ps_cbr_buffer->i8_credit_level = 0;
    ps_cbr_buffer->e_rc_type = e_rc_type;
    ps_cbr_buffer->i4_capped_vbr_on = i4_capped_vbr_flag;
    /*If this is set to 1, it acts similar to storage VBR which allows peak rate to be sustained for infinite duration*/
    ps_cbr_buffer->i4_vbr_no_peak_rate_duration_limit = 0;
    ps_cbr_buffer->i8_num_frames_encoded = 0;
    ps_cbr_buffer->i8_tot_frm_to_be_encoded = i8_num_frames_to_encode;
    ps_cbr_buffer->i4_cbr_rc_pass = i4_cbr_rc_pass;
    ps_cbr_buffer->i4_inter_frame_int = i4_inter_frm_int;
    ps_cbr_buffer->i4_intra_frame_int = u4_intra_frm_int;
    ps_cbr_buffer->f_max_dur_peak_rate = f_max_dur_peak_rate;

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(u4_bit_rate, 1000, i4_tgt_frm_rate, i4_bits_per_frm[i]);
        /* Drain rate = bitrate/(framerate/1000) */
        ps_cbr_buffer->i4_drain_bits_per_frame[i] = i4_bits_per_frm[i];
        /* initialise the bits per frame error bits calculation */
        init_error_bits(ps_cbr_buffer->aps_bpf_error_bits[i], i4_tgt_frm_rate, u4_bit_rate);
    }

    /* Bitrate * delay = buffer size, divide by 1000 as delay is in ms*/
    if(e_rc_type == CBR_NLDRC) /* This would mean CBR mode */
    {
        //buffer size should be independent of initial delay
        //X_PROD_Y_DIV_Z(u4_bit_rate,i4_buffer_delay,1000,ps_cbr_buffer->i4_buffer_size);
        ps_cbr_buffer->i4_buffer_size = (WORD32)u4_vbv_buf_size;
        ps_cbr_buffer->i4_is_cbr_mode = 1;
        ps_cbr_buffer->i4_peak_drain_rate_frame = i4_bits_per_frm[0];
        /*In CBR the max file size deviaiton allowed is specified by buffer size*/
        ps_cbr_buffer->u4_vbr_max_bit_deviation = ps_cbr_buffer->i4_buffer_size;
    }
    else if(e_rc_type == VBR_STREAMING)
    {
        /*this is raw vbv buffer size, also initilize the buffer window to bit alloc (credit limit)*/
        ps_cbr_buffer->i4_buffer_size = (WORD32)u4_vbv_buf_size;
        /*if there is no limit on duration for which peak bitrate can be sustained, bits can be moved from any region to other region
          giving better quality*/
        if(f_max_dur_peak_rate < 0)
            ps_cbr_buffer->i4_vbr_no_peak_rate_duration_limit = 1;
        /*To avoid file size deviation in case of VBR mode of rate control, clip the max deviaiton allowed based on number of frames to enode*/
        {
            ULWORD64 u8_vbr_max_bit_deviation;
            ULWORD64 file_size = (ULWORD64)(
                (((LWORD64)u4_bit_rate * 1000) / i4_tgt_frm_rate) * i8_num_frames_to_encode);

            /*When f_max_dur_peak_rate is -ve, it implies user is not worried about duration for which peak is sustained, hence go with max possible value*/
            if(f_max_dur_peak_rate > 0)
                u8_vbr_max_bit_deviation = (ULWORD64)(f_max_dur_peak_rate * u4_bit_rate);
            else
                u8_vbr_max_bit_deviation = (ULWORD64)(VBR_MAX_BIT_DEV_SEC * u4_bit_rate);

            /*when num frames to encode is negative is -ve it implies total frames data is not available (as in case of live encoding)*/
            if(i8_num_frames_to_encode > 0)
            {
                /*allow atleast one second deviation or 12% of total file size whichever is higher*/
                if(u8_vbr_max_bit_deviation > (file_size >> 3))
                    u8_vbr_max_bit_deviation = (UWORD32)(file_size >> 3);

                /*allow atleast one second for shorter sequence*/
                if(u8_vbr_max_bit_deviation < u4_bit_rate)
                    u8_vbr_max_bit_deviation = u4_bit_rate;
            }
            else
            {
                /*the data of number of frames to be encoded is not available*/
                /*start off with one second delay, this will be later adjusted once large number of frames are encoded*/
                u8_vbr_max_bit_deviation = u4_bit_rate;
            }
            ps_cbr_buffer->u4_vbr_max_bit_deviation = u8_vbr_max_bit_deviation;
        }
        ps_cbr_buffer->i4_is_cbr_mode = 0;
        X_PROD_Y_DIV_Z(
            u4_peak_bit_rate, 1000, i4_tgt_frm_rate, ps_cbr_buffer->i4_peak_drain_rate_frame);
    }
    else
    {
        /*currently only two modes are supported*/
        ASSERT(e_rc_type == CONST_QP);
    }

    if(ps_cbr_buffer->i4_buffer_size > (WORD32)u4_vbv_buf_size)
    {
        ps_cbr_buffer->i4_buffer_size = u4_vbv_buf_size;
    }

    /* Uppr threshold for
        I frame = 1 * bits per frame
        P Frame = 4 * bits per frame.
        The threshold for I frame is only 1 * bits per frame as the threshold should
        only account for error in estimated bits.
        In P frame it should account for difference bets bits consumed by I(Scene change)
        and P frame I to P complexity is assumed to be 5. */
    /*HEVC_hierarchy*/
    if(e_rc_type != CONST_QP)
        set_upper_lower_vbv_threshold(ps_cbr_buffer, i4_bits_per_frm[0]);
    /* Storing the input parameters for using it for change functions */
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        ps_cbr_buffer->ai4_bit_rate[i] = u4_bit_rate;
    for(i = 0; i < MAX_PIC_TYPE; i++)
    {
        ps_cbr_buffer->ai4_num_pics_in_delay_period[i] = u4_num_pics_in_delay_prd[i];
    }
    ps_cbr_buffer->i4_tgt_frm_rate = i4_tgt_frm_rate;
    ps_cbr_buffer->i4_max_delay = i4_buffer_delay;
    ps_cbr_buffer->u4_max_vbv_buf_size = u4_vbv_buf_size;
    ps_cbr_buffer->u4_num_frms_in_delay = u4_num_frames_in_delay;
}
#endif /* #if NON_STEADSTATE_CODE */

/* ******************************************************************************/
/**
 * @brief Condition check for constrining the number of bits allocated based on bufer size
 *
 * @param ps_cbr_buffer
 * @param i4_tgt_bits
 * @param e_pic_type
 *
 * @return
 */
/* ******************************************************************************/
WORD32 cbr_buffer_constraint_check(
    cbr_buffer_t *ps_cbr_buffer,
    WORD32 i4_tgt_bits,
    picture_type_e e_pic_type,
    WORD32 *pi4_max_tgt_bits,
    WORD32 *pi4_min_tgt_bits)
{
    WORD32 i4_max_tgt_bits, i4_min_tgt_bits;
    WORD32 i4_drain_bits_per_frame = (e_pic_type == I_PIC)
                                         ? ps_cbr_buffer->i4_drain_bits_per_frame[0]
                                         : ps_cbr_buffer->i4_drain_bits_per_frame[1];
    WORD32 i4_error_bits = (e_pic_type == I_PIC)
                               ? get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[0])
                               : get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[1]);

    /*trace_printf("  ebf = %d  bebf = %d ",ps_cbr_buffer->i4_ebf,ps_cbr_buffer->i8_ebf_bit_alloc);*/
    /* Max tgt bits = Upper threshold - current encoder buffer fullness */
    i4_max_tgt_bits =
        (WORD32)(ps_cbr_buffer->i4_upr_thr[e_pic_type] - ps_cbr_buffer->i4_ebf_estimate);
    /* Max tgt bits cannot be negative */
    if(i4_max_tgt_bits < 0)
        i4_max_tgt_bits = 0;

    /* Min tgt bits , least number of bits in the Encoder after
    draining such that it is greater than lower threshold */
    i4_min_tgt_bits = (WORD32)(
        ps_cbr_buffer->i4_low_thr[e_pic_type] -
        (ps_cbr_buffer->i4_ebf_estimate - i4_drain_bits_per_frame - i4_error_bits));
    /*Min tgt bits cannot be negative*/
    if(i4_min_tgt_bits < 0)
        i4_min_tgt_bits = 0;

    /* current tgt bits should be between max and min tgt bits*/
    CLIP(i4_tgt_bits, i4_max_tgt_bits, i4_min_tgt_bits);
    pi4_min_tgt_bits[0] = i4_min_tgt_bits;
    pi4_max_tgt_bits[0] = i4_max_tgt_bits;
    return i4_tgt_bits;
}

/* ******************************************************************************/
/**
 * @brief constaints the bit allocation based on buffer size
 *
 * @param ps_cbr_buffer
 * @param i4_tgt_bits
 * @param e_pic_type
 *
 * @return
 */
/* ******************************************************************************/
WORD32 vbr_stream_buffer_constraint_check(
    cbr_buffer_t *ps_cbr_buffer,
    WORD32 i4_tgt_bits,
    picture_type_e e_pic_type,
    WORD32 *pi4_max_bits,
    WORD32 *pi4_min_bits)
{
    WORD32 i4_max_tgt_bits, i4_min_tgt_bits = 0;

    /* Max tgt bits = Upper threshold - current encoder buffer fullness */
    /*maximum target for a pic is amount of bits that can be transmitted to decoder buffer in delay assuming max drain rate
      This above limit has to be constrained wrt a single frame being accomodated in the buffer*/
    i4_max_tgt_bits = (WORD32)(
        (ps_cbr_buffer->u4_num_frms_in_delay * ps_cbr_buffer->i4_peak_drain_rate_frame) -
        ps_cbr_buffer->i4_ebf_estimate);
    /*the below check is necessary to make sure that a single frame to be accomodated in encoder buffer*/
    if(i4_max_tgt_bits > ps_cbr_buffer->i4_upr_thr[e_pic_type] - ps_cbr_buffer->i4_ebf_estimate)
    {
        i4_max_tgt_bits =
            (WORD32)(ps_cbr_buffer->i4_upr_thr[e_pic_type] - ps_cbr_buffer->i4_ebf_estimate);
    }

    /*In VBR streaming though encoder buffer underflow is not a problem, at any point of time the bitrate underconsumption
      cannot go below specified limit. Hence it is limited based on possible bitrate deviation allowed*/
    /*Enabling movement of stuffing bits always*/
    if(ps_cbr_buffer->i4_vbr_no_peak_rate_duration_limit)
    {
        /*If the content has underconsumed force it to consume atleast per frame bits so that end of encoding there wont be too much undersonsumption*/
        if(ps_cbr_buffer->i8_ebf_bit_alloc < 0 && ps_cbr_buffer->i4_cbr_rc_pass != 2)
            i4_min_tgt_bits = (ps_cbr_buffer->i4_drain_bits_per_frame[0] >> 1);
    }
    else
    {
        /*In this case buffer is always guranteed to be positive, to avoid stuffing give decent amount of min bits*/
        i4_min_tgt_bits = (WORD32)(ps_cbr_buffer->i4_low_thr[0] - ps_cbr_buffer->i8_ebf_bit_alloc);
    }

    /*Clip min target bit*/
    if(i4_min_tgt_bits < 0)
        i4_min_tgt_bits = 0;
    if(i4_tgt_bits < i4_min_tgt_bits)
        i4_tgt_bits = i4_min_tgt_bits;
    pi4_min_bits[0] = i4_min_tgt_bits;
    /* Max tgt bits cannot be negative */
    if(i4_max_tgt_bits < 0)
        i4_max_tgt_bits = 0;
    if(i4_tgt_bits > i4_max_tgt_bits)
        i4_tgt_bits = i4_max_tgt_bits;
    pi4_max_bits[0] = i4_max_tgt_bits;

    return i4_tgt_bits;
}

/* ******************************************************************************/
/**
 * @brief Verifies the buffer state and returns whether it is overflowing, underflowing or normal
 *
 * @param ps_cbr_buffer
 * @param i4_tot_consumed_bits
 * @param pi4_num_bits_to_prevent_overflow
 * @param e_pic_type
 *
 * @return
 */
/* ******************************************************************************/
vbv_buf_status_e get_cbr_buffer_status(
    cbr_buffer_t *ps_cbr_buffer,
    WORD32 i4_tot_consumed_bits,
    WORD32 *pi4_num_bits_to_prevent_overflow,
    picture_type_e e_pic_type,
    rc_type_e e_rc_type)
{
    vbv_buf_status_e e_buf_status;
    WORD32 i4_cur_enc_buf;
    WORD32 i4_error_bits = (e_pic_type == I_PIC)
                               ? get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[0])
                               : get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[1]);
    WORD32 i4_drain_bits_per_frame = (e_pic_type == I_PIC)
                                         ? ps_cbr_buffer->i4_drain_bits_per_frame[0]
                                         : ps_cbr_buffer->i4_drain_bits_per_frame[1];

    /* Add the tot consumed bits to the Encoder Buffer*/
    i4_cur_enc_buf = ps_cbr_buffer->i4_ebf + i4_tot_consumed_bits;

    /* If the Encoder exceeds the Buffer Size signal an Overflow*/
    if(i4_cur_enc_buf > ps_cbr_buffer->i4_buffer_size)
    {
        e_buf_status = VBV_OVERFLOW;
        i4_cur_enc_buf = ps_cbr_buffer->i4_buffer_size;
    }
    else
    {
        /* Subtract the constant drain bits and error bits due to fixed point implementation*/
        i4_cur_enc_buf -= (i4_drain_bits_per_frame + i4_error_bits);

        if(e_rc_type == VBR_STREAMING)
        {
            /*In VBR suffing scenerio will not occur*/
            if(i4_cur_enc_buf < 0)
                i4_cur_enc_buf = 0;
        }
        /* If the buffer is less than stuffing threshold an Underflow is signaled else its NORMAL*/
        if(i4_cur_enc_buf < 0)
        {
            e_buf_status = VBV_UNDERFLOW;
        }
        else
        {
            e_buf_status = VBV_NORMAL;
        }

        if(i4_cur_enc_buf < 0)
            i4_cur_enc_buf = 0;
    }

    /* The RC lib models the encoder buffer, but the VBV buffer characterises the decoder buffer */
    if(e_buf_status == VBV_OVERFLOW)
    {
        e_buf_status = VBV_UNDERFLOW;
    }
    else if(e_buf_status == VBV_UNDERFLOW)
    {
        e_buf_status = VBV_OVERFLOW;
    }

    pi4_num_bits_to_prevent_overflow[0] = (ps_cbr_buffer->i4_buffer_size - i4_cur_enc_buf);

    return e_buf_status;
}

/* ******************************************************************************/
/**
 * @brief Based on the bits consumed the buffer model is updated
 *
 * @param ps_cbr_buffer
 * @param i4_tot_consumed_bits
 * @param e_pic_type
 */
/* ******************************************************************************/
void update_cbr_buffer(
    cbr_buffer_t *ps_cbr_buffer, WORD32 i4_tot_consumed_bits, picture_type_e e_pic_type)
{
    WORD32 i;
    WORD32 i4_error_bits = (e_pic_type == I_PIC)
                               ? get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[0])
                               : get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[1]);
    WORD32 i4_drain_bits_per_frame = (e_pic_type == I_PIC)
                                         ? ps_cbr_buffer->i4_drain_bits_per_frame[0]
                                         : ps_cbr_buffer->i4_drain_bits_per_frame[1];

    ps_cbr_buffer->i8_num_frames_encoded++;
    if(ps_cbr_buffer->e_rc_type == VBR_STREAMING && ps_cbr_buffer->i8_tot_frm_to_be_encoded < 0)
    {
        LWORD64 i8_max_bit_dev_allowed = ps_cbr_buffer->ai4_bit_rate[0];
        LWORD64 approx_file_size = ps_cbr_buffer->i8_num_frames_encoded *
                                   ps_cbr_buffer->ai4_bit_rate[0] * 1000 /
                                   ps_cbr_buffer->i4_tgt_frm_rate;
        if(i8_max_bit_dev_allowed < (approx_file_size >> 4))
            i8_max_bit_dev_allowed = (approx_file_size >> 4);

        /*have a max limit so that bit dev does not grow for very long sequence like 24 hours of encoding (max can be 20 second)*/
        if(i8_max_bit_dev_allowed > (VBR_MAX_BIT_DEV_SEC * ps_cbr_buffer->ai4_bit_rate[0]))
            i8_max_bit_dev_allowed = (VBR_MAX_BIT_DEV_SEC * ps_cbr_buffer->ai4_bit_rate[0]);

        ps_cbr_buffer->u4_max_vbv_buf_size = (UWORD32)i8_max_bit_dev_allowed;
    }
    /* Update the Encoder buffer with the total consumed bits*/
    if(ps_cbr_buffer->i4_is_cbr_mode != 0)
    {
        ps_cbr_buffer->i4_ebf += i4_tot_consumed_bits;
        ps_cbr_buffer->i8_ebf_bit_alloc += i4_tot_consumed_bits;

        /* Subtract the drain bits and error bits due to fixed point implementation*/
        ps_cbr_buffer->i4_ebf -= (i4_drain_bits_per_frame + i4_error_bits);
        ps_cbr_buffer->i8_ebf_bit_alloc -= (i4_drain_bits_per_frame + i4_error_bits);
    }
    else
    {
        ps_cbr_buffer->i4_ebf += i4_tot_consumed_bits;
        ps_cbr_buffer->i4_ebf -=
            ((MIN(ps_cbr_buffer->i4_peak_drain_rate_frame, ps_cbr_buffer->i4_ebf)) + i4_error_bits);

        ps_cbr_buffer->i8_ebf_bit_alloc += i4_tot_consumed_bits;
        ps_cbr_buffer->i8_ebf_bit_alloc -=
            (ps_cbr_buffer->i4_drain_bits_per_frame[0] + i4_error_bits);

        ps_cbr_buffer->i8_credit_level += i4_tot_consumed_bits;
        ps_cbr_buffer->i8_credit_level -=
            (ps_cbr_buffer->i4_drain_bits_per_frame[0] + i4_error_bits);
        /*To keep limit on duration for which peak rate can be sustained limit the accumulation of bits from simpler regions*/
        if(!ps_cbr_buffer->i4_vbr_no_peak_rate_duration_limit)
        {
            if(ps_cbr_buffer->i8_ebf_bit_alloc < 0)
                ps_cbr_buffer->i8_ebf_bit_alloc =
                    0; /*This will make VBR buffer believe that the bits are lost*/
        }
    }

    /*SS - Fix for lack of stuffing*/
    if(ps_cbr_buffer->i4_ebf < 0)
    {
        //trace_printf("Error: Should not be coming here with bit stuffing \n");
        ps_cbr_buffer->i4_ebf = 0;
    }

    if(ps_cbr_buffer->i4_ebf > ps_cbr_buffer->i4_buffer_size)
    {
        //trace_printf("Error: Frame should be skipped\n");
        ps_cbr_buffer->i4_ebf = ps_cbr_buffer->i4_buffer_size;
    }

    ps_cbr_buffer->i4_ebf_estimate = ps_cbr_buffer->i4_ebf;

    trace_printf(
        "VBR ebf = %d  bebf = %d  ", ps_cbr_buffer->i4_ebf, ps_cbr_buffer->i8_ebf_bit_alloc);
    /* Update the error bits */
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        update_error_bits(ps_cbr_buffer->aps_bpf_error_bits[i]);
}

/* ******************************************************************************/
/**
 * @brief If the buffer underflows then return the number of bits to prevent underflow
 *
 * @param ps_cbr_buffer
 * @param i4_tot_consumed_bits
 * @param e_pic_type
 *
 * @return
 */
/* ******************************************************************************/
WORD32 get_cbr_bits_to_stuff(
    cbr_buffer_t *ps_cbr_buffer, WORD32 i4_tot_consumed_bits, picture_type_e e_pic_type)
{
    WORD32 i4_bits_to_stuff;
    WORD32 i4_error_bits = (e_pic_type == I_PIC)
                               ? get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[0])
                               : get_error_bits(ps_cbr_buffer->aps_bpf_error_bits[1]);
    WORD32 i4_drain_bits_per_frame = (e_pic_type == I_PIC)
                                         ? ps_cbr_buffer->i4_drain_bits_per_frame[0]
                                         : ps_cbr_buffer->i4_drain_bits_per_frame[1];

    /* Stuffing bits got from the following equation
       Stuffing_threshold = ebf + tcb - drain bits - error bits + stuff_bits*/
    i4_bits_to_stuff =
        i4_drain_bits_per_frame + i4_error_bits - (ps_cbr_buffer->i4_ebf + i4_tot_consumed_bits);

    return i4_bits_to_stuff;
}

/* ******************************************************************************/
/**
 * @brief Change the state for change in bit rate
 *
 * @param ps_cbr_buffer
 * @param i4_bit_rate
 */
/* ******************************************************************************/
void change_cbr_vbv_bit_rate(
    cbr_buffer_t *ps_cbr_buffer, WORD32 *i4_bit_rate, WORD32 i4_peak_bitrate)
{
    WORD32 i4_bits_per_frm[MAX_NUM_DRAIN_RATES];
    int i;

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(i4_bit_rate[i], 1000, ps_cbr_buffer->i4_tgt_frm_rate, i4_bits_per_frm[i]);
        /* Drain rate = bitrate/(framerate/1000) */
        ps_cbr_buffer->i4_drain_bits_per_frame[i] = i4_bits_per_frm[i];

        /* initialise the bits per frame error bits calculation */
        change_bitrate_in_error_bits(ps_cbr_buffer->aps_bpf_error_bits[i], i4_bit_rate[i]);
    }
    X_PROD_Y_DIV_Z(
        i4_peak_bitrate,
        1000,
        ps_cbr_buffer->i4_tgt_frm_rate,
        ps_cbr_buffer->i4_peak_drain_rate_frame);
    /* Bitrate * delay = buffer size, divide by 1000 as delay is in ms*/
    //if(i4_bit_rate[0] == i4_bit_rate[1]) /* This would mean CBR mode */
    {
        X_PROD_Y_DIV_Z(
            i4_bit_rate[0],
            ps_cbr_buffer->i4_max_delay,
            1000,
            ps_cbr_buffer->i4_buffer_size);  //the delay term is supposed to remain constant
        //ps_cbr_buffer->i4_is_cbr_mode = 1;
        ps_cbr_buffer->u4_max_vbv_buf_size = ps_cbr_buffer->i4_buffer_size;
    }
    if(ps_cbr_buffer->i4_buffer_size > (WORD32)ps_cbr_buffer->u4_max_vbv_buf_size)
    {
        ps_cbr_buffer->i4_buffer_size = ps_cbr_buffer->u4_max_vbv_buf_size;
    }
    set_upper_lower_vbv_threshold(ps_cbr_buffer, i4_bits_per_frm[0]);
    if(ps_cbr_buffer->e_rc_type == CBR_NLDRC)
    {
        ps_cbr_buffer->u4_vbr_max_bit_deviation = ps_cbr_buffer->i4_buffer_size;
    }
    else
    {
        /*DCB: the deviaiton must be altered for VBR case, when bitrate is lowered quality might be bad because of this*/
        {
            ULWORD64 u8_vbr_max_bit_deviation =
                (ULWORD64)(ps_cbr_buffer->f_max_dur_peak_rate * i4_bit_rate[0]);
            ULWORD64 file_size = (ULWORD64)(
                (((LWORD64)i4_bit_rate[0] * 1000) / ps_cbr_buffer->i4_tgt_frm_rate) *
                (ps_cbr_buffer->i8_tot_frm_to_be_encoded - ps_cbr_buffer->i8_num_frames_encoded));
            /*When f_max_dur_peak_rate is -ve, it implies user is not worried about duration for which peak is sustained, hence go with max possible value*/
            if(ps_cbr_buffer->f_max_dur_peak_rate > 0)
                u8_vbr_max_bit_deviation =
                    (ULWORD64)(ps_cbr_buffer->f_max_dur_peak_rate * i4_bit_rate[0]);
            else
                u8_vbr_max_bit_deviation = VBR_MAX_BIT_DEV_SEC * i4_bit_rate[0];

            /*when num frames to encode is negative is -ve it implies total frames data is not available (as in case of live encoding)*/
            if(ps_cbr_buffer->i8_tot_frm_to_be_encoded > 0)
            {
                /*allow atleast one second deviation or 12% of total file size whichever is higher*/
                if(u8_vbr_max_bit_deviation > (file_size >> 3))
                    u8_vbr_max_bit_deviation = (UWORD32)(file_size >> 3);
            }
            else
            {
                u8_vbr_max_bit_deviation = (UWORD32)(file_size >> 3);
            }
            /*allow atleast one second for shorter sequence*/
            if(u8_vbr_max_bit_deviation < (ULWORD64)i4_bit_rate[0])
                u8_vbr_max_bit_deviation = (ULWORD64)i4_bit_rate[0];
            ps_cbr_buffer->u4_vbr_max_bit_deviation = u8_vbr_max_bit_deviation;
        }
    }

    /* Storing the input parameters for using it for change functions */
    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
        ps_cbr_buffer->ai4_bit_rate[i] = i4_bit_rate[i];
}
/* ******************************************************************************/
/**
 * @brief Update the state for change in number of pics in the delay period
 *
 * @param ps_cbr_buffer
 * @param u4_num_pics_in_delay_prd
 */
/* ******************************************************************************/
void change_cbr_vbv_num_pics_in_delay_period(
    cbr_buffer_t *ps_cbr_buffer, UWORD32 *u4_num_pics_in_delay_prd)
{
    WORD32 i;

    if(!ps_cbr_buffer->i4_is_cbr_mode)
    {
        ps_cbr_buffer->i4_buffer_size =
            u4_num_pics_in_delay_prd[0] * ps_cbr_buffer->i4_drain_bits_per_frame[0] +
            u4_num_pics_in_delay_prd[1] * ps_cbr_buffer->i4_drain_bits_per_frame[1];

        if(ps_cbr_buffer->i4_buffer_size > (WORD32)ps_cbr_buffer->u4_max_vbv_buf_size)
        {
            ps_cbr_buffer->i4_buffer_size = ps_cbr_buffer->u4_max_vbv_buf_size;
        }
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_cbr_buffer->i4_upr_thr[i] =
                ps_cbr_buffer->i4_buffer_size - (ps_cbr_buffer->i4_buffer_size >> 3);
        }

        /* Re-initilise the number of pics in delay period */
        for(i = 0; i < MAX_PIC_TYPE; i++)
        {
            ps_cbr_buffer->ai4_num_pics_in_delay_period[i] = u4_num_pics_in_delay_prd[i];
        }
    }
}
/* ******************************************************************************/
/**
 * @ modifies the ebf estimated parameter based on error
 *
 * @param ps_cbr_buffer
 * @param i4_bit_error
 */
/* ******************************************************************************/
void cbr_modify_ebf_estimate(cbr_buffer_t *ps_cbr_buffer, WORD32 i4_bit_error)
{
    ps_cbr_buffer->i4_ebf_estimate = ps_cbr_buffer->i4_ebf + i4_bit_error;
    if(ps_cbr_buffer->i4_ebf_estimate < 0)
    {
        ps_cbr_buffer->i4_ebf_estimate = 0;
    }
    else if(ps_cbr_buffer->i4_ebf_estimate > ps_cbr_buffer->i4_buffer_size)
    {
        ps_cbr_buffer->i4_ebf_estimate = ps_cbr_buffer->i4_buffer_size;
    }
}

/* ******************************************************************************/
/**
 * @ get the buffer size
 *
 * @param ps_cbr_buffer
 */
/* ******************************************************************************/

WORD32 get_cbr_buffer_size(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->i4_buffer_size);
}

#if NON_STEADSTATE_CODE
/* ******************************************************************************/
/**
 * @brief update the state for change in target frame rate
 *
 * @param ps_cbr_buffer
 * @param i4_tgt_frm_rate
 */
/* ******************************************************************************/
void change_cbr_vbv_tgt_frame_rate(cbr_buffer_t *ps_cbr_buffer, WORD32 i4_tgt_frm_rate)
{
    WORD32 i4_i, i4_bits_per_frm[MAX_NUM_DRAIN_RATES];
    int i;

    for(i = 0; i < MAX_NUM_DRAIN_RATES; i++)
    {
        X_PROD_Y_DIV_Z(ps_cbr_buffer->ai4_bit_rate[i], 1000, i4_tgt_frm_rate, i4_bits_per_frm[i]);
        /* Drain rate = bitrate/(framerate/1000) */
        ps_cbr_buffer->i4_drain_bits_per_frame[i] = i4_bits_per_frm[i];
        /* initialise the bits per frame error bits calculation */
        change_frm_rate_in_error_bits(ps_cbr_buffer->aps_bpf_error_bits[i], i4_tgt_frm_rate);
    }

    /* Bitrate * delay = buffer size, divide by 1000 as delay is in ms*/
    if(!ps_cbr_buffer->i4_is_cbr_mode)
    {
        /* VBR streaming case which has different drain rates for I and P */
        ps_cbr_buffer->i4_buffer_size = ps_cbr_buffer->ai4_num_pics_in_delay_period[0] *
                                            ps_cbr_buffer->i4_drain_bits_per_frame[0] +
                                        ps_cbr_buffer->ai4_num_pics_in_delay_period[1] *
                                            ps_cbr_buffer->i4_drain_bits_per_frame[1];
    }

    if(ps_cbr_buffer->i4_buffer_size > (WORD32)ps_cbr_buffer->u4_max_vbv_buf_size)
    {
        ps_cbr_buffer->i4_buffer_size = ps_cbr_buffer->u4_max_vbv_buf_size;
    }

    for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
    {
        /* Uppr threshold for
           I frame = 1 * bits per frame
           P Frame = 4 * bits per frame.
           The threshold for I frame is only 1 * bits per frame as the threshold should
           only account for error in estimated bits.
           In P frame it should account for difference bets bits consumed by I(Scene change)
           and P frame I to P complexity is assumed to be 5. */
        WORD32 i4_index;
        i4_index = i4_i > 0 ? 1 : 0;
        ps_cbr_buffer->i4_upr_thr[i4_i] =
            ps_cbr_buffer->i4_buffer_size - (ps_cbr_buffer->i4_buffer_size >> 3);

        /* For both I and P frame Lower threshold is equal to drain rate.
        Even if the encoder consumes zero bits it should have enough bits to drain*/
        ps_cbr_buffer->i4_low_thr[i4_i] = i4_bits_per_frm[i4_index];
    }

    /* Storing the input parameters for using it for change functions */
    ps_cbr_buffer->i4_tgt_frm_rate = i4_tgt_frm_rate;
}
/* ******************************************************************************/
/**
 * @brief update the state for change in buffer delay
 *
 * @param ps_cbr_buffer
 * @param i4_buffer_delay
 */
/* ******************************************************************************/
void change_cbr_buffer_delay(cbr_buffer_t *ps_cbr_buffer, WORD32 i4_buffer_delay)
{
    WORD32 i4_i;

    /* Bitrate * delay = buffer size, divide by 1000 as delay is in ms*/
    if(ps_cbr_buffer->i4_is_cbr_mode)
    {
        X_PROD_Y_DIV_Z(
            ps_cbr_buffer->ai4_bit_rate[0], i4_buffer_delay, 1000, ps_cbr_buffer->i4_buffer_size);
    }

    if(ps_cbr_buffer->i4_buffer_size > (WORD32)ps_cbr_buffer->u4_max_vbv_buf_size)
    {
        ps_cbr_buffer->i4_buffer_size = ps_cbr_buffer->u4_max_vbv_buf_size;
    }

    for(i4_i = 0; i4_i < MAX_PIC_TYPE; i4_i++)
    {
        /* Uppr threshold for
           I frame = 1 * bits per frame
           P Frame = 4 * bits per frame.
           The threshold for I frame is only 1 * bits per frame as the threshold should
           only account for error in estimated bits.
           In P frame it should account for difference bets bits consumed by I(Scene change)
           and P frame I to P complexity is assumed to be 5. */
        ps_cbr_buffer->i4_upr_thr[i4_i] =
            ps_cbr_buffer->i4_buffer_size - (ps_cbr_buffer->i4_buffer_size >> 3);
    }

    /* Storing the input parameters for using it for change functions */
    ps_cbr_buffer->i4_max_delay = i4_buffer_delay;
}
/* ******************************************************************************/
/**
 * @brief update the state for change in buffer delay
 *
 * @param ps_cbr_buffer
 * @param i4_buffer_delay
 */
/* ******************************************************************************/
WORD32 get_cbr_buffer_delay(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->i4_max_delay);
}
/* ******************************************************************************/
/**
 * @brief get_cbr_ebf
 *
 * @param ps_cbr_buffer
  */
/* ******************************************************************************/
WORD32 get_cbr_ebf(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->i4_ebf);
}
/* ******************************************************************************/
/**
 * @brief get_cbr_max_ebf
 *
 * @param ps_cbr_buffer
 */
/* ******************************************************************************/
WORD32 get_cbr_max_ebf(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->i4_upr_thr[0]);
}
/* ******************************************************************************/
/**
 * @brief set_cbr_ebf
 *
 * @param ps_cbr_buffer
 * @param i32_init_ebf
 */
/* ******************************************************************************/
void set_cbr_ebf(cbr_buffer_t *ps_cbr_buffer, WORD32 i32_init_ebf)
{
    ps_cbr_buffer->i4_ebf = i32_init_ebf;
}
/* ******************************************************************************/
/**
 * @brief update_cbr_buf_mismatch_bit
 *
 * @param ps_cbr_buffer
 * @param i4_error_bits
 */
/* ******************************************************************************/
void update_cbr_buf_mismatch_bit(cbr_buffer_t *ps_cbr_buffer, WORD32 i4_error_bits)
{
    ps_cbr_buffer->i4_ebf -= i4_error_bits;
    ps_cbr_buffer->i8_ebf_bit_alloc -= i4_error_bits;
    ps_cbr_buffer->i8_credit_level -= i4_error_bits;
}
/* ******************************************************************************/
/**
 * @brief get encoded number of frames
 *
 * @param ps_cbr_buffer
  */
/* ******************************************************************************/
LWORD64 get_num_frms_encoded(cbr_buffer_t *ps_cbr_buffer)
{
    return ps_cbr_buffer->i8_num_frames_encoded;
}
/* ******************************************************************************/
/**
 * @brief get num frames to encode
 *
 * @param ps_cbr_buffer
  */
/* ******************************************************************************/
LWORD64 get_num_frms_to_encode(cbr_buffer_t *ps_cbr_buffer)
{
    return ps_cbr_buffer->i8_tot_frm_to_be_encoded;
}
/* ******************************************************************************/
/**
 * @brief get peak drain rate
 *
 * @param ps_cbr_buffer
 */
/* ******************************************************************************/
/* The buffer limit in bit allocation should be according to peak bitrate */
WORD32 get_buf_max_drain_rate(cbr_buffer_t *ps_cbr_buffer)
{
    if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
        return ps_cbr_buffer->i4_peak_drain_rate_frame;
    else if(ps_cbr_buffer->e_rc_type != CONST_QP)
    {
        ASSERT(
            ps_cbr_buffer->i4_peak_drain_rate_frame == ps_cbr_buffer->i4_drain_bits_per_frame[0]);
        return ps_cbr_buffer->i4_drain_bits_per_frame[0];
    }
    return ps_cbr_buffer->i4_drain_bits_per_frame[0];
}
/* ******************************************************************************/
/**
 * @brief get excess bits by moving in VBV buffer to enable bitrate greater than peak rate for shorter duration in very
 *  complex contents
 *
 * @param ps_cbr_buffer
 * @param i4_tgt_frm_rate
 */
/* ******************************************************************************/
WORD32 get_vbv_buffer_based_excess(
    cbr_buffer_t *ps_cbr_buffer,
    float f_complexity_peak_rate,
    float f_cur_bits_complexity,
    WORD32 bit_alloc_period,
    WORD32 i4_num_gops_for_excess)
{
    LWORD64 max_buffer_level = (LWORD64)((float)ps_cbr_buffer->i4_buffer_size * 0.8f);
    LWORD64 i8_excess_bits;
    /*LWORD64target_buf_level;*/
    WORD32
    num_frm_to_be_distributed;  //Number of frames for which excess bits should be distributed, using number of frames corresponding to buffer size for now

    if(ps_cbr_buffer->i4_upr_thr[0] <
       max_buffer_level) /*choose max allowed level to min(upper_threshold,80% of buffer*/
        max_buffer_level = ps_cbr_buffer->i4_upr_thr[0];

    if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
        max_buffer_level = (LWORD64)(
            ps_cbr_buffer->i4_peak_drain_rate_frame * ps_cbr_buffer->u4_num_frms_in_delay * 0.8f);

    if(f_cur_bits_complexity >
       0.9f) /*clip current to max of 80% of buffer size to avoid dangerous buffer level by end of GOP*/
        f_cur_bits_complexity = 0.9f;

    if(f_cur_bits_complexity < f_complexity_peak_rate || f_cur_bits_complexity < 0.1f ||
       ps_cbr_buffer->i4_buffer_size <
           ps_cbr_buffer->ai4_bit_rate
               [0])  //For buffer size less than 1 sec disable any contribution from buffer based for extra complex contents
    {
        /*For very low compleity content or Cavg do not allow buffer movement*/
        return 0;
    }

    i8_excess_bits = (LWORD64)(
        ((f_cur_bits_complexity - f_complexity_peak_rate) / (0.9f - f_complexity_peak_rate)) *
        (max_buffer_level - ps_cbr_buffer->i4_ebf));

    if(i8_excess_bits < 0)
        i8_excess_bits = 0;

    num_frm_to_be_distributed = (WORD32)(
        ((float)ps_cbr_buffer->i4_buffer_size / ps_cbr_buffer->ai4_bit_rate[0] *
         ps_cbr_buffer->i4_tgt_frm_rate / 1000) +
        0.5);
    /*Excess bits should be proportional to bit alloc period, shorter intra period should get in small incentives*/
    if(bit_alloc_period < num_frm_to_be_distributed)
        i8_excess_bits =
            (LWORD64)((float)i8_excess_bits * bit_alloc_period / num_frm_to_be_distributed);

    if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
    {
        if(i4_num_gops_for_excess > 1)
            i8_excess_bits = i8_excess_bits * i4_num_gops_for_excess;

        if(i8_excess_bits > (LWORD64)(
                                (float)ps_cbr_buffer->i4_peak_drain_rate_frame *
                                ps_cbr_buffer->u4_num_frms_in_delay * 0.8f))
            i8_excess_bits = (LWORD64)(
                (float)ps_cbr_buffer->i4_peak_drain_rate_frame *
                ps_cbr_buffer->u4_num_frms_in_delay * 0.8f);
    }
    trace_printf(
        "Excess bits %d %f %f num gops %d",
        i8_excess_bits,
        f_cur_bits_complexity,
        f_complexity_peak_rate,
        i4_num_gops_for_excess);

    return ((WORD32)i8_excess_bits);
}
/* ******************************************************************************/
/**
 * @brief get gop correction error bits for  the current gop. This will be added to rbip.
  *
 * @param ps_cbr_buffer
 * @param i4_lap_complexity_q7
 * @param i4_bit_alloc_period
 */
/* ******************************************************************************/
WORD32 get_error_bits_for_desired_buf(
    cbr_buffer_t *ps_cbr_buffer, WORD32 i4_lap_complexity_q7, WORD32 i4_bit_alloc_period)
{
    if(ps_cbr_buffer->e_rc_type == CBR_NLDRC)
    {
        LWORD64 error_bits = 0, complexity_mov_buf_size = 0;
        LWORD64 i8_default_bits_in_period, i8_max_additional_bits_in_period;
        LWORD64 i8_buf_based_limit_red, i8_buf_based_limit_inc, i8_buf_diff_bits;
        float buf_diff, abs_lap_complexity;

        /*calculate default allocation*/
        i8_default_bits_in_period = (LWORD64)ps_cbr_buffer->ai4_bit_rate[0] * 1000 *
                                    i4_bit_alloc_period / ps_cbr_buffer->i4_tgt_frm_rate;

        /*In case of VBR give additional bits according to peak bitrate*/
        if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
        {
            i8_max_additional_bits_in_period =
                ((LWORD64)ps_cbr_buffer->i4_peak_drain_rate_frame * i4_bit_alloc_period) -
                i8_default_bits_in_period;
            ASSERT(i8_max_additional_bits_in_period >= 0);
            if(i8_max_additional_bits_in_period > (i8_default_bits_in_period))
            {
                /*clip max bits that can be given to 2x bitrate since its too riskly to give more than that in single pass encoding
                  where long future is not known*/
                i8_max_additional_bits_in_period = (i8_default_bits_in_period);
            }
        }
        else
        {
            i8_max_additional_bits_in_period = i8_default_bits_in_period;
        }
        {
            float X = ((float)i4_lap_complexity_q7 / 128);
            float desired_buf_level;
            /*For CBR VBV buffer size is "complexity_mov_buf_size" and In case of VBR it is determined by bit deviaiton*/
            if(ps_cbr_buffer->e_rc_type == CBR_NLDRC)
            {
                complexity_mov_buf_size = (LWORD64)ps_cbr_buffer->i4_upr_thr[0];
            }
            else if(ps_cbr_buffer->e_rc_type == VBR_STREAMING)
            {
                complexity_mov_buf_size = ps_cbr_buffer->u4_vbr_max_bit_deviation;
            }
            abs_lap_complexity = X;

            if(ps_cbr_buffer->i4_cbr_rc_pass == 2)
                desired_buf_level = COMP_TO_BITS_MAP_2_PASS(X, complexity_mov_buf_size);
            else
                desired_buf_level = COMP_TO_BITS_MAP(X, complexity_mov_buf_size);

            if(desired_buf_level < 0)
                desired_buf_level = 0;
            /*map complexity to buffer level*/

            error_bits = (LWORD64)(desired_buf_level - ps_cbr_buffer->i8_ebf_bit_alloc);
            i8_buf_diff_bits = error_bits;
            /*For VBR its possible that i8_ebf_bit_alloc can go below 0, that the extent of giving should only be desired - cur( = 0 for cur < 0)*/
            buf_diff = (float)error_bits / complexity_mov_buf_size;

            /*clipping based on buffer size should depend on gop size. Assuming 7% of gop of gop = 32, calculate for other GOP intervals max 7% while giving from buffer and 10%
              while stealing from buffer(for GOP of 32)*/
            /*try to be conservative when giving extra bits to gop and limit while reducing bits to GOP needs to be higher inorder to be buffer compliant if necessary*/
            i8_buf_based_limit_red =
                ((LWORD64)complexity_mov_buf_size * i4_bit_alloc_period * 12) >> 12;
            i8_buf_based_limit_inc = ((LWORD64)complexity_mov_buf_size * i4_bit_alloc_period * 8) >>
                                     12;

            /*(shd be 7 even if GOP size goes lesser)*/
            if(i8_buf_based_limit_red < (((LWORD64)complexity_mov_buf_size * 10) >> 7))
                i8_buf_based_limit_red = (((LWORD64)complexity_mov_buf_size * 10) >> 7);
            if(i8_buf_based_limit_inc < (((LWORD64)complexity_mov_buf_size * 10) >> 7))
                i8_buf_based_limit_inc = (((LWORD64)complexity_mov_buf_size * 10) >> 7);

            /*if error bits is too high it is given in stages so that buffer is utilized for entire complex content*/
            /*error bits should not exceed ten 7% of buffer*/
            /*error bits can be max equal to bitrate*/
            if(error_bits > 0)
            {
                /*if lap compleixty is higher and buffer allows give the bits*/
                error_bits = (WORD32)(abs_lap_complexity * i8_max_additional_bits_in_period);
                /*if lap complexity is too simple do not give additional bits to make sure that simple scenes never get additional bits whatsoever*/
                if(abs_lap_complexity < 0.2f && ps_cbr_buffer->i8_ebf_bit_alloc >= 0)
                {
                    error_bits = 0;
                }
                if(error_bits > i8_buf_diff_bits)
                    error_bits = i8_buf_diff_bits;

                if(error_bits > i8_buf_based_limit_inc)
                {
                    error_bits = i8_buf_based_limit_inc;
                }
                /*If buffer is already half filled be conservative. Allocate 1.5 times bits
                 else allocate twice the bits*/
                if(ps_cbr_buffer->i8_ebf_bit_alloc >
                   (LWORD64)(ps_cbr_buffer->i4_buffer_size * 0.75))
                {
                    if(error_bits > (i8_max_additional_bits_in_period >> 1))
                    {
                        error_bits = (i8_max_additional_bits_in_period >> 1);
                    }
                }
                else
                {
                    if(error_bits > i8_max_additional_bits_in_period)
                    {
                        error_bits = i8_max_additional_bits_in_period;
                    }
                }
            }
            else
            {
                error_bits = (WORD32)(buf_diff * (i8_default_bits_in_period >> 1));
                if(error_bits < -i8_buf_based_limit_red)
                {
                    error_bits = -i8_buf_based_limit_red;
                }
                /*when buffer level needs to reduce bits in period*/
                /*If current level is less than half min bits in period = 70% of constant bit in period else 50%*/
                if(ps_cbr_buffer->i8_ebf_bit_alloc > (ps_cbr_buffer->i4_buffer_size >> 1))
                {
                    if(error_bits < -(i8_default_bits_in_period >> 1))
                    {
                        error_bits = -(i8_default_bits_in_period >> 1);
                    }
                }
                else
                {
                    if(error_bits < -((i8_default_bits_in_period * 5) >> 4))
                    {
                        error_bits = -((i8_default_bits_in_period * 5) >> 4);
                    }
                }
            }
        }
        return (WORD32)error_bits;
    }
    else
    {
        LWORD64 max_excess_bits, default_allocation_for_period, comp_based_excess = 0;
        LWORD64 i8_excess_bits = 0, bit_dev_so_far, credit_limit_level;
        LWORD64 Ravg_dur, num_intra_period_in_Ravg_dur,
            num_intra_in_clip;  //duration for which Ravg has to be met, for shorter slips this can be equal to clip duration
        LWORD64 i8_buf_based_limit_red, i8_buf_based_limit_inc;
        float comp_to_bit_mapped, X;

        /*default allocation for period in absence of complexity based bit allocation*/
        default_allocation_for_period =
            ps_cbr_buffer->i4_drain_bits_per_frame[0] * i4_bit_alloc_period;

        bit_dev_so_far = ps_cbr_buffer->i8_ebf_bit_alloc;
        credit_limit_level = ps_cbr_buffer->i8_credit_level;
        Ravg_dur =
            ps_cbr_buffer->u4_vbr_max_bit_deviation * 5 / ps_cbr_buffer->i4_drain_bits_per_frame[0];
        if(Ravg_dur > 20 * ps_cbr_buffer->i8_tot_frm_to_be_encoded / 100)
            Ravg_dur = 20 * ps_cbr_buffer->i8_tot_frm_to_be_encoded / 100;
        if(Ravg_dur <= 0)
            Ravg_dur = 1;
        /*map the complexity to bits ratio*/
        X = (float)i4_lap_complexity_q7 / 128;
        if(ps_cbr_buffer->i4_cbr_rc_pass == 2)
            comp_to_bit_mapped = COMP_TO_BITS_MAP_2_PASS(X, 1.0f);
        else
            comp_to_bit_mapped = COMP_TO_BITS_MAP(X, 1.0f);

        comp_to_bit_mapped *= 10;  //mapping it to absolute peak bitrate

        /*calculate the number of bit alloc periods over which the credit limit needs to build up*/
        num_intra_in_clip = ps_cbr_buffer->i8_tot_frm_to_be_encoded / i4_bit_alloc_period;
        num_intra_period_in_Ravg_dur = Ravg_dur / i4_bit_alloc_period;
        //ASSERT(ps_cbr_buffer->i8_tot_frm_to_be_encoded > i4_bit_alloc_period);
        if(ps_cbr_buffer->i8_tot_frm_to_be_encoded < i4_bit_alloc_period)
        {
            num_intra_period_in_Ravg_dur = 1;
            num_intra_in_clip = 1;
        }
        if(num_intra_period_in_Ravg_dur <= 0)
        {
            num_intra_period_in_Ravg_dur = 1;
        }
        /*max excess bits possible according to given peak bitrate*/
        {
            max_excess_bits = (ps_cbr_buffer->i4_peak_drain_rate_frame -
                               ps_cbr_buffer->i4_drain_bits_per_frame[0]) *
                              i4_bit_alloc_period;
            /*constrain max excess bits allocated to a region if buffer is already at critical level*/
            /*assume room for 20% over-consumption due to mismatch between allocation and consumption*/
            if(ps_cbr_buffer->i4_ebf >
               (ps_cbr_buffer->i4_upr_thr[0] - (WORD32)(max_excess_bits * 0.2)))
            {
                max_excess_bits = (LWORD64)(max_excess_bits * 0.8);
            }
        }
        /*clipping based on buffer size should depend on gop size. Assuming 7% of gop of gop = 32, calculate for other GOP intervals max 7% while giving from buffer and 10%
            while stealing from buffer(for GOP of 32)*/
        /*try to be conservative when giving extra bits to gop and limit while reducing bits to GOP needs to be higher inorder to be buffer compliant if necessary*/
        i8_buf_based_limit_red =
            ((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * i4_bit_alloc_period * 12) >> 12;
        i8_buf_based_limit_inc =
            ((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * i4_bit_alloc_period * 8) >> 12;

        /*(shd be 7 even if GOP size goes lesser)*/
        if(i8_buf_based_limit_red < (((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * 10) >> 7))
            i8_buf_based_limit_red = (((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * 10) >> 7);
        if(i8_buf_based_limit_inc < (((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * 10) >> 7))
            i8_buf_based_limit_inc = (((LWORD64)ps_cbr_buffer->u4_vbr_max_bit_deviation * 10) >> 7);

        /*The credit limit is not completly built, hence the average  operating bitrate will be lesser than average*/
        //if(ps_cbr_buffer->i8_ebf_bit_alloc >= 0)
        //Disabling this to avoid under-consumption of bits since mostly contents will end with simpler sequence
        if(1 != ps_cbr_buffer->i4_capped_vbr_on)
        {
            /*adjust the excess bits to account for deviation in bitrate
            If bit deviation is positive then overconsumption, hence resuce the default bit allocation*/

            /* In capped vbr mode this is not calculated as there is no constraint to meet the configured bitrate */
            i8_excess_bits -= (bit_dev_so_far / num_intra_period_in_Ravg_dur);
        }
        /*allocate bits based on complexity*/
        /*comp_to_bit_mapped less than 1 implies a content that requires less than average bitrate,
          hence due to sign reversal we tend to steal bits*/
        comp_based_excess = (LWORD64)((comp_to_bit_mapped - 1) * default_allocation_for_period);

        if(1 != ps_cbr_buffer->i4_capped_vbr_on)
        {
            /*clip the complexity based on intra period and credit limit buffer size so that when credit limit is lower not everything is used for first GOP*/
            if(comp_based_excess > i8_buf_based_limit_inc)
            {
                comp_based_excess = i8_buf_based_limit_inc;
            }
            else if(comp_based_excess < -i8_buf_based_limit_red)
            {
                comp_based_excess = -i8_buf_based_limit_red;
            }

            /*when the credit limit is fully used, stop giving extra*/
            if(credit_limit_level > ps_cbr_buffer->u4_vbr_max_bit_deviation)
            {
                if(comp_based_excess < 0)
                    i8_excess_bits += comp_based_excess;
            }
            /*when credit limit is almost full (80 percent full)*/
            else if(credit_limit_level > (LWORD64)(ps_cbr_buffer->u4_vbr_max_bit_deviation * 0.8f))
            {
                /*follow smooth transition, at 80% utilized the excess should be 100 percent, it should move to zero percent as it approaches 100% utlization*/
                if(comp_based_excess > 0)
                    i8_excess_bits += (LWORD64)(
                        ((ps_cbr_buffer->u4_vbr_max_bit_deviation - credit_limit_level) /
                         (0.2f * ps_cbr_buffer->u4_vbr_max_bit_deviation)) *
                        comp_based_excess);
                else
                    i8_excess_bits += comp_based_excess;
            }
            else if(credit_limit_level > (LWORD64)(ps_cbr_buffer->u4_vbr_max_bit_deviation * 0.2f))
            {
                i8_excess_bits += comp_based_excess;
            }
            /*When credit limit is almost unutilized*/
            else if(
                credit_limit_level < (WORD32)(ps_cbr_buffer->u4_vbr_max_bit_deviation * 0.2f) &&
                credit_limit_level > 0)
            {
                if(comp_based_excess < 0)
                    i8_excess_bits += (LWORD64)(
                        (credit_limit_level / (0.2f * ps_cbr_buffer->u4_vbr_max_bit_deviation)) *
                        comp_based_excess);
                else
                    i8_excess_bits += comp_based_excess;
            }
            /*If the credit limit still uutilized stop drawing bits from simpler content*/
            else if(credit_limit_level <= 0)
            {
                if(comp_based_excess > 0)
                    i8_excess_bits += comp_based_excess;
            }
            else
                ASSERT(0);
        }
        else
        {
            /* In capped vbr mode excess bits will be based on complexity of content alone*/
            i8_excess_bits = comp_based_excess;
        }

        /*Clip the excess bits such that it will never violate peak bitrate and also Rmin*/
        if(i8_excess_bits > max_excess_bits)
            i8_excess_bits = max_excess_bits;
        /*assuming atleast 0.4 times average bitrate even for the simplest content*/
        if(i8_excess_bits < -(default_allocation_for_period * 0.6f))
            i8_excess_bits = (LWORD64)(-(default_allocation_for_period * 0.6f));

        ASSERT(i8_excess_bits <= 0x7FFFFFFF);
        return (WORD32)i8_excess_bits;
    }
}
/* ******************************************************************************/
/**
 * @brief get_rc_type.
  *
 * @param ps_cbr_buffer
 */
/* ******************************************************************************/
rc_type_e get_rc_type(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->e_rc_type);
}
/* ******************************************************************************/
/**
 * @brief cbr_get_delay_frames
  *
 * @param ps_cbr_buffer
 */
/* ******************************************************************************/
UWORD32 cbr_get_delay_frames(cbr_buffer_t *ps_cbr_buffer)
{
    return (ps_cbr_buffer->u4_num_frms_in_delay);
}
#endif /* #if NON_STEADSTATE_CODE */
