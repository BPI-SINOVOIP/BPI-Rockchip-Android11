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
* \file fixed_point_error_bits.c
*
* \brief
*    This file contain error bits processing functions
*
* \date
*    15/02/2012
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

/* User include files */
#include "ittiam_datatypes.h"
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "var_q_operator.h"
#include "mem_req_and_acq.h"
#include "fixed_point_error_bits.h"

/*i4_max_tgt_frm_rate and i4_tgt_frm_rate hold same value so removing one*/
typedef struct error_bits_t
{
    /*    WORD32 i4_max_tgt_frm_rate; */ /*Max tgt frm rate so that dynamic change infrm rate can be handled */
    WORD32 i4_accum_frm_rate; /* Cur frm rate  */
    WORD32 i4_tgt_frm_rate; /* tgt frame rate*/
    WORD32 i4_tgt_frm_rate_incr; /* tgt frm rate increment */
    UWORD8 u1_compute_error_bits; /* flag to indicate 1 second is up*/
    WORD32 i4_accum_bitrate; /* Bitrate/frame rate value added over a period*/
    WORD32 i4_bitrate; /* bitrate */
} error_bits_t;

#if NON_STEADSTATE_CODE
WORD32 error_bits_num_fill_use_free_memtab(
    error_bits_t **pps_error_bits, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static error_bits_t s_error_bits_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_error_bits) = &s_error_bits_temp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(error_bits_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_error_bits, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}

/* ******************************************************************************/
/**
 * @brief Calculates the error bits due to fixed point divisions
 *
 * @param ps_error_bits
 * @param i4_tgt_frm_rate
 * @param i4_bitrate
 */
/* ******************************************************************************/
void init_error_bits(error_bits_t *ps_error_bits, WORD32 i4_tgt_frm_rate, WORD32 i4_bitrate)
{
    /* This value is incremented every at the end of every VOP by i4_tgt_frm_rate_incr*/
    /* Initializing the parameters*/
    ps_error_bits->i4_accum_frm_rate = 0;
    ps_error_bits->i4_tgt_frm_rate = i4_tgt_frm_rate;

    /* Value by which i4_accum_frm_rate is incremented every VOP*/
    ps_error_bits->i4_tgt_frm_rate_incr = 1000;

    /*Compute error bits is set to 1 at the end of 1 second*/
    ps_error_bits->u1_compute_error_bits = 0;
    ps_error_bits->i4_tgt_frm_rate = i4_tgt_frm_rate;
    ps_error_bits->i4_accum_bitrate = 0;
    ps_error_bits->i4_bitrate = i4_bitrate;
}

#endif /* #if NON_STEADSTATE_CODE */

/* ******************************************************************************/
/**
 * @brief Updates the error state
 *
 * @param ps_error_bits
 */
/* ******************************************************************************/
void update_error_bits(error_bits_t *ps_error_bits)
{
    WORD32 i4_bits_per_frame;

    X_PROD_Y_DIV_Z(
        ps_error_bits->i4_bitrate, 1000, ps_error_bits->i4_tgt_frm_rate, i4_bits_per_frame);

    if(ps_error_bits->u1_compute_error_bits == 1)
    {
        ps_error_bits->i4_accum_bitrate = 0;
        //ps_error_bits->i4_accum_frm_rate -= ps_error_bits->i4_tgt_frm_rate;
        ps_error_bits->i4_accum_frm_rate = 0;
    }
    /* This value is incremented every at the end of every VOP by
       i4_tgt_frm_rate_incr*/
    ps_error_bits->i4_accum_frm_rate += ps_error_bits->i4_tgt_frm_rate_incr;
    ps_error_bits->i4_accum_bitrate += i4_bits_per_frame;

    /* When current tgt frm rate is equal or greater than max tgt fram rate
      1 second is up , compute the error bits */
    if(ps_error_bits->i4_accum_frm_rate >= ps_error_bits->i4_tgt_frm_rate)
    {
        /* ps_error_bits->i4_accum_frm_rate -= ps_error_bits->i4_tgt_frm_rate; */
        ps_error_bits->u1_compute_error_bits = 1;
    }
    else
    {
        ps_error_bits->u1_compute_error_bits = 0;
    }
}

/* ******************************************************************************/
/**
 * @brief Returns the error bits for the current frame if there are any
 *
 * @param ps_error_bits
 *
 * @return
 */
/* ******************************************************************************/
WORD32 get_error_bits(error_bits_t *ps_error_bits)
{
    WORD32 i4_error_bits = 0;
    /*If 1s is up calcualte error for the last 1s worth fo frames*/
    if(ps_error_bits->u1_compute_error_bits == 1)
    {
        WORD32 i4_cur_bitrate;
        WORD32 i4_cur_frame_rate = ps_error_bits->i4_accum_frm_rate;
        /* For frame rates like 29.970, the current frame rate would be a multiple of
        1000 and every 100 seconds 3 frames would be dropped. So the error should be
        calculated based on actual frame rate. So for e.g. the iteration, there would be
        30 frames and so the bits allocated would be (30/29.970)*bitrate */
        X_PROD_Y_DIV_Z(
            ps_error_bits->i4_bitrate,
            i4_cur_frame_rate,
            ps_error_bits->i4_tgt_frm_rate,
            i4_cur_bitrate);
        /*Error = Actual bitrate - bits_per_frame * num of frames*/
        i4_error_bits = i4_cur_bitrate - ps_error_bits->i4_accum_bitrate;
    }
    return (i4_error_bits);
}
/* ******************************************************************************/
/**
 * @brief Change the bitrate value for error bits module
 *
 * @param ps_error_bits
 * @param i4_bitrate
 */
/* ******************************************************************************/
void change_bitrate_in_error_bits(error_bits_t *ps_error_bits, WORD32 i4_bitrate)
{
    /*here accum_bitrate would have accumulated the value based on old bit rate. after one second is elapsed
     * the error is calcluated based on new bit rate which would result in huge error value. to avoid this
     * accum_bitrate value is recalculated assuming new bitrate.
     */
    /*#ifdef DYNAMIC_RC*/
    WORD32 i4_old_bits_per_frame;
    WORD32 i4_new_bits_per_frame;
    WORD32 i4_frame_count;
    X_PROD_Y_DIV_Z(
        ps_error_bits->i4_bitrate, 1000, ps_error_bits->i4_tgt_frm_rate, i4_old_bits_per_frame);
    i4_frame_count = ps_error_bits->i4_accum_bitrate / i4_old_bits_per_frame;
    X_PROD_Y_DIV_Z(i4_bitrate, 1000, ps_error_bits->i4_tgt_frm_rate, i4_new_bits_per_frame);
    ps_error_bits->i4_accum_bitrate = i4_frame_count * i4_new_bits_per_frame;

    /*#endif*/
    /*change bit rate*/
    ps_error_bits->i4_bitrate = i4_bitrate;
    /*    ps_error_bits->i4_accum_bitrate=i4_bitrate;*/
}
/* ******************************************************************************/
/**
 * @brief Change the frame rate parameter for the error bits state
 *
 * @param ps_error_bits
 * @param i4_tgt_frm_rate
 */
/* ******************************************************************************/
/*IMPLEMENTATION NOT TESTED*/

void change_frm_rate_in_error_bits(error_bits_t *ps_error_bits, WORD32 i4_tgt_frm_rate)
{
    /* Value by which i4_accum_frm_rate is incremented every VOP*/
    /*accum_frm_rate is used to mark one second mark so a change in frame rate could alter this mark leading
     * to very high accum bitrate value. To avoid this accum_frame_rate is recalculated
     * according to new value
     */
    /*  WORD32 i4_frame_count;*/

    /*  ps_error_bits->i4_accum_frm_rate=(ps_error_bits->i4_accum_frm_rate*i4_tgt_frm_rate)/ps_error_bits->i4_tgt_frm_rate);*/

    if(ps_error_bits->i4_tgt_frm_rate != i4_tgt_frm_rate)
        X_PROD_Y_DIV_Z(
            ps_error_bits->i4_accum_frm_rate,
            i4_tgt_frm_rate,
            ps_error_bits->i4_tgt_frm_rate,
            ps_error_bits->i4_accum_frm_rate);

    /*round off the accum value to multiple of 1000*/
    ps_error_bits->i4_accum_frm_rate = ps_error_bits->i4_accum_frm_rate / 1000;
    ps_error_bits->i4_accum_frm_rate = ps_error_bits->i4_accum_frm_rate * 1000;

    /*   ps_error_bits->i4_tgt_frm_rate_incr = (ps_error_bits->i4_tgt_frm_rate
                                           * 1000)/ i4_tgt_frm_rate;
*/
    ps_error_bits->i4_tgt_frm_rate = i4_tgt_frm_rate;
}
