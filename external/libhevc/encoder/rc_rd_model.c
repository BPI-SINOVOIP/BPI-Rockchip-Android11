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

/****************************************************************************/
/* File Name         : rc_rd_model.c                                        */
/*                                                                          */
/* Description       : Implall the Functions to Model the                   */
/*                     Rate Distortion Behaviour of the Codec over the Last */
/*                     Few Frames.                                          */
/*                                                                          */
/* List of Functions : update_frame_rd_model                                */
/*                     estimate_mpeg2_qp_for_resbits                        */
/*                                                                          */
/* Issues / Problems : None                                                 */
/*                                                                          */
/* Revision History  :                                                      */
/*        DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*        21 06 2006   ittiam           Initial Version                      */
/****************************************************************************/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* System include files */
#include "ittiam_datatypes.h"
#include "rc_common.h"
#include "var_q_operator.h"
#include "mem_req_and_acq.h"
#include "rc_rd_model.h"
#include "rc_rd_model_struct.h"

#if !(RC_FIXED_POINT)

#if NON_STEADSTATE_CODE
WORD32 rc_rd_model_num_fill_use_free_memtab(
    rc_rd_model_t **pps_rc_rd_model, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static rc_rd_model_t s_rc_rd_model_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_rc_rd_model) = &s_rc_rd_model_temp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(rc_rd_model_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_rc_rd_model, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}

void init_frm_rc_rd_model(rc_rd_model_t *ps_rd_model, UWORD8 u1_max_frames_modelled)
{
    /*ps_rd_model = ps_rd_model + u1_pic_type;*/

    ps_rd_model->u1_num_frms_in_model = 0;
    ps_rd_model->u1_curr_frm_counter = 0;
    ps_rd_model->u1_max_frms_to_model = u1_max_frames_modelled;
    /*
    ps_rd_model->u1_min_frames_for_quad_model = u1_min_frames_for_quad_model;
    ps_rd_model->u1_min_frames_for_lin_model  = u1_min_frames_for_lin_model;
    */

    ps_rd_model->model_coeff_a_quad = 0;
    ps_rd_model->model_coeff_b_quad = 0;
    ps_rd_model->model_coeff_c_quad = 0;

    ps_rd_model->model_coeff_a_lin = 0;
    ps_rd_model->model_coeff_b_lin = 0;
    ps_rd_model->model_coeff_c_lin = 0;

    ps_rd_model->model_coeff_a_lin_wo_int = 0;
    ps_rd_model->model_coeff_b_lin_wo_int = 0;
    ps_rd_model->model_coeff_c_lin_wo_int = 0;
}

void reset_frm_rc_rd_model(rc_rd_model_t *ps_rd_model)
{
    /*ps_rd_model = ps_rd_model + u1_pic_type;*/

    ps_rd_model->u1_num_frms_in_model = 0;
    ps_rd_model->u1_curr_frm_counter = 0;
    ps_rd_model->model_coeff_a_quad = 0;
    ps_rd_model->model_coeff_b_quad = 0;
    ps_rd_model->model_coeff_c_quad = 0;

    ps_rd_model->model_coeff_a_lin = 0;
    ps_rd_model->model_coeff_b_lin = 0;
    ps_rd_model->model_coeff_c_lin = 0;

    ps_rd_model->model_coeff_a_lin_wo_int = 0;
    ps_rd_model->model_coeff_b_lin_wo_int = 0;
    ps_rd_model->model_coeff_c_lin_wo_int = 0;
}
#endif /* #if NON_STEADSTATE_CODE */

#if ENABLE_QUAD_MODEL
static UWORD8 find_model_coeffs(
    UWORD32 *pi4_res_bits,
    UWORD32 *pi4_sad_h264,
    UWORD8 *pu1_num_skips,
    UWORD8 *pui_avg_mpeg2_qp,
    UWORD8 u1_num_frms,
    UWORD8 u1_model_used,
    WORD8 *pi1_frame_index,
    model_coeff *pmc_model_coeff,
    model_coeff *pmc_model_coeff_lin,
    model_coeff *pmc_model_coeff_lin_wo_int,
    rc_rd_model_t *ps_rd_model)
{
    UWORD32 i;
    UWORD8 u1_num_frms_used = 0;
    UWORD8 u1_frm_indx;

    float sum_y = 0;
    float sum_x_y = 0;
    float sum_x2_y = 0;
    float sum_x = 0;
    float sum_x2 = 0;
    float sum_x3 = 0;
    float sum_x4 = 0;
    float var_x2_y = 0;
    float var_x_y = 0;
    float var_x2_x = 0;
    float var_x2_x2 = 0;
    float var_x_x = 0;
    float x0, y0;
    float model_coeff_a, model_coeff_b, model_coeff_c, model_coeff_den;

    for(i = 0; i < u1_num_frms; i++)
    {
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        y0 = (float)(pi4_res_bits[u1_frm_indx]);
        x0 = (float)(pi4_sad_h264[u1_frm_indx] / (float)pui_avg_mpeg2_qp[u1_frm_indx]);

        sum_y += y0;
        sum_x_y += x0 * y0;
        sum_x2_y += x0 * x0 * y0;
        sum_x += x0;
        sum_x2 += x0 * x0;
        sum_x3 += x0 * x0 * x0;
        sum_x4 += x0 * x0 * x0 * x0;
        u1_num_frms_used++;
    }

    sum_y /= u1_num_frms_used;
    sum_x_y /= u1_num_frms_used;
    sum_x2_y /= u1_num_frms_used;
    sum_x /= u1_num_frms_used;
    sum_x2 /= u1_num_frms_used;
    sum_x3 /= u1_num_frms_used;
    sum_x4 /= u1_num_frms_used;

#if !QUAD
    u1_model_used = LIN_MODEL;
#endif

    if((QUAD_MODEL == u1_model_used) && (u1_num_frms_used <= MIN_FRAMES_FOR_QUAD_MODEL))
    {
        u1_model_used = LIN_MODEL;
    }

    if(QUAD_MODEL == u1_model_used)
    {
        var_x2_y = sum_x2_y - sum_x2 * sum_y;
        var_x_y = sum_x_y - sum_x * sum_y;
        var_x2_x = sum_x3 - sum_x2 * sum_x;
        var_x2_x2 = sum_x4 - sum_x2 * sum_x2;
        var_x_x = sum_x2 - sum_x * sum_x;

        model_coeff_den = (var_x2_x * var_x2_x - var_x2_x2 * var_x_x);

        if(0 != model_coeff_den)
        {
            model_coeff_b = (var_x_y * var_x2_x - var_x2_y * var_x_x);
            model_coeff_b /= model_coeff_den;

            model_coeff_a = (var_x2_y * var_x2_x - var_x_y * var_x2_x2);
            model_coeff_a /= model_coeff_den;

            model_coeff_c = sum_y - (model_coeff_a * sum_x) - (model_coeff_b * sum_x2);
        }

        pmc_model_coeff[0] = model_coeff_b;
        pmc_model_coeff[1] = model_coeff_a;
        pmc_model_coeff[2] = model_coeff_c;
    }

    if(NULL != pmc_model_coeff_lin)
    {
        var_x_y = sum_x_y - sum_x * sum_y;
        var_x_x = sum_x2 - sum_x * sum_x;

        if(0 != var_x_x)
        {
            model_coeff_a = (var_x_y / var_x_x);
            model_coeff_c = sum_y - (model_coeff_a * sum_x);
            /*model_coeff_b = 0;*/
            model_coeff_b = model_coeff_a;

            pmc_model_coeff_lin[0] = model_coeff_b;
            pmc_model_coeff_lin[1] = model_coeff_a;
            pmc_model_coeff_lin[2] = model_coeff_c;
        }
    }

    if(NULL != pmc_model_coeff_lin_wo_int)
    {
        UWORD8 u1_curr_frame_index;
        UWORD8 u1_avgqp_prvfrm;
        UWORD32 u4_prevfrm_bits, u4_prevfrm_sad;

        u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;
        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (MAX_FRAMES_MODELLED - 1);
        else
            u1_curr_frame_index--;

        u1_avgqp_prvfrm = ps_rd_model->pu1_avg_qp[u1_curr_frame_index];
        u4_prevfrm_bits = ps_rd_model->pi4_res_bits[u1_curr_frame_index];
        u4_prevfrm_sad = ps_rd_model->pi4_sad[u1_curr_frame_index];

        if(0 != u4_prevfrm_sad)
            model_coeff_a = (float)(u4_prevfrm_bits * u1_avgqp_prvfrm) / u4_prevfrm_sad;
        else
            model_coeff_a = 0;

        model_coeff_b = 0;
        model_coeff_c = 0;

        pmc_model_coeff_lin_wo_int[0] = model_coeff_b;
        pmc_model_coeff_lin_wo_int[1] = model_coeff_a;
        pmc_model_coeff_lin_wo_int[2] = model_coeff_c;
    }

    return u1_model_used;
}

static WORD8 refine_set_of_points(
    UWORD32 *pi4_res_bits,
    UWORD32 *pi4_sad_h264,
    UWORD8 *pu1_num_skips,
    UWORD8 *pui_avg_mpeg2_qp,
    UWORD8 u1_num_frms,
    WORD8 *pi1_frame_index,
    model_coeff *pmc_model_coeff,
    float *pfl_avg_deviation)
{
    float fl_avg_deviation, fl_estimated_bits, fl_deviation, x_val;
    UWORD8 u1_return_value = 1;
    UWORD32 i;
    UWORD8 u1_num_frms_used, u1_frm_indx;

    u1_num_frms_used = 0;
    fl_avg_deviation = 0;
    for(i = 0; i < u1_num_frms; i++)
    {
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];
        x_val = pi4_sad_h264[u1_frm_indx] / (float)pui_avg_mpeg2_qp[u1_frm_indx];

        fl_estimated_bits = (pmc_model_coeff[0] * x_val * x_val) + (pmc_model_coeff[1] * x_val) +
                            (pmc_model_coeff[2]);

        fl_deviation =
            fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) / (float)pi4_res_bits[u1_frm_indx];
        fl_deviation = fl_deviation * fl_deviation;
        fl_avg_deviation += fl_deviation;
        u1_num_frms_used++;
    }

    fl_avg_deviation /= u1_num_frms_used;
    /*fl_avg_deviation = sqrt(fl_avg_deviation);*/
    fl_avg_deviation = (fl_avg_deviation);

    for(i = 0; i < u1_num_frms; i++)
    {
        if((-1 == pi1_frame_index[i]) && (i != 0))
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        x_val = pi4_sad_h264[u1_frm_indx] / (float)pui_avg_mpeg2_qp[u1_frm_indx];

        fl_estimated_bits = (pmc_model_coeff[0] * x_val * x_val) + (pmc_model_coeff[1] * x_val) +
                            (pmc_model_coeff[2]);

        fl_deviation =
            fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) / (float)pi4_res_bits[u1_frm_indx];

        fl_deviation = fl_deviation * fl_deviation;

        if(fl_deviation > (fl_avg_deviation))
        {
            pi1_frame_index[i] = -1;
        }
    }

    if(fl_avg_deviation > 0.0625)
        u1_return_value = 0;
    if(fl_avg_deviation < 0.0225)
        u1_return_value = 2;

    *pfl_avg_deviation = fl_avg_deviation;

    return (u1_return_value);
}
static void calc_avg_sqr_dev_for_model(
    UWORD32 *pi4_res_bits,
    UWORD32 *pi4_sad_h264,
    UWORD8 *pu1_num_skips,
    UWORD8 *pui_avg_mpeg2_qp,
    UWORD8 u1_num_frms,
    WORD8 *pi1_frame_index,
    model_coeff *pmc_model_coeff,
    float *pfl_avg_deviation)
{
    float fl_avg_deviation, fl_estimated_bits, fl_deviation, x_val;
    UWORD8 u1_return_value = 1;
    UWORD32 i;
    UWORD8 u1_num_frms_used, u1_frm_indx;

    u1_num_frms_used = 0;
    fl_avg_deviation = 0;
    for(i = 0; i < u1_num_frms; i++)
    {
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        u1_frm_indx = (UWORD8)i;
        x_val = pi4_sad_h264[u1_frm_indx] / (float)pui_avg_mpeg2_qp[u1_frm_indx];

        fl_estimated_bits = (pmc_model_coeff[1] * x_val) + (pmc_model_coeff[2]);

        fl_deviation =
            fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) / (float)pi4_res_bits[u1_frm_indx];
        fl_deviation = fl_deviation * fl_deviation;
        fl_avg_deviation += fl_deviation;
        u1_num_frms_used++;
    }

    fl_avg_deviation /= u1_num_frms_used;
    /*fl_avg_deviation = sqrt(fl_avg_deviation);*/
    fl_avg_deviation = (fl_avg_deviation);

    *pfl_avg_deviation = fl_avg_deviation;
    /*return (u1_return_value);*/
}
static void update_frame_rd_model(rc_rd_model_t *ps_rd_model)
{
    WORD8 pi1_frame_index[MAX_FRAMES_MODELLED], pi1_frame_index_initial[MAX_FRAMES_MODELLED];

    UWORD8 u1_num_skips_temp;
    UWORD8 u1_avg_mpeg2_qp_temp, u1_min_mpeg2_qp, u1_max_mpeg2_qp;
    UWORD8 u1_num_frms_input, u1_num_active_frames, u1_reject_frame;
    UWORD32 u4_num_skips;

    UWORD8 u1_min2_mpeg2_qp, u1_max2_mpeg2_qp;
    UWORD8 u1_min_qp_frame_indx, u1_max_qp_frame_indx;
    UWORD8 pu1_num_frames[MPEG2_QP_ELEM];
    model_coeff model_coeff_array[3], model_coeff_array_lin[3], model_coeff_array_lin_wo_int[3];
    UWORD32 i;
    UWORD8 u1_curr_frame_index;
    UWORD8 u1_quad_model_valid, u1_lin_model_valid;

    float fl_quad_avg_dev, fl_lin_avg_dev;

    UWORD8 u1_check_model;

    /*ps_rd_model += u1_pic_type;*/

    u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;

    ps_rd_model->u1_model_used = QUAD_MODEL;

    if(0 == u1_curr_frame_index)
        u1_curr_frame_index = (MAX_FRAMES_MODELLED - 1);
    else
        u1_curr_frame_index--;

    /************************************************************************/
    /* Rearrange data to be fed into a Linear Regression Module             */
    /* Module finds a,b,c such that                                         */
    /*      y = ax + bx^2 + c                                               */
    /************************************************************************/
    u4_num_skips = 0;
    u1_num_frms_input = 0;
    memset(pu1_num_frames, 0, MPEG2_QP_ELEM);
    memset(pi1_frame_index, -1, MAX_FRAMES_MODELLED);
    u1_min_mpeg2_qp = MAX_MPEG2_QP;
    u1_max_mpeg2_qp = 0;

    u1_num_active_frames = ps_rd_model->u1_num_frms_in_model;
    if(u1_num_active_frames > MAX_ACTIVE_FRAMES)
        u1_num_active_frames = MAX_ACTIVE_FRAMES;

    /************************************************************************/
    /* Choose the set of Points to be used for MSE fit of Quadratic model   */
    /* Points chosen are spread across the Qp range. Max of 2 points are    */
    /* chosen for a Qp.                                                     */
    /************************************************************************/
    for(i = 0; i < u1_num_active_frames; i++)
    {
        u1_reject_frame = 0;
        u1_num_skips_temp = ps_rd_model->pu1_num_skips[u1_curr_frame_index];
        u1_avg_mpeg2_qp_temp = ps_rd_model->pu1_avg_qp[u1_curr_frame_index];

        if((0 == u4_num_skips) && (0 != u1_num_skips_temp))
            u1_reject_frame = 1;
        if((1 == u4_num_skips) && (u1_num_skips_temp > 1))
            u1_reject_frame = 1;
        if(pu1_num_frames[u1_avg_mpeg2_qp_temp] >= 2)
            u1_reject_frame = 1;

        if(0 == i)
            u1_reject_frame = 0;

        if(0 == u1_reject_frame)
        {
            pi1_frame_index[u1_num_frms_input] = (WORD8)u1_curr_frame_index;
            pu1_num_frames[u1_avg_mpeg2_qp_temp] += 1;

            if(u1_min_mpeg2_qp > u1_avg_mpeg2_qp_temp)
                u1_min_mpeg2_qp = u1_avg_mpeg2_qp_temp;
            if(u1_max_mpeg2_qp < u1_avg_mpeg2_qp_temp)
                u1_max_mpeg2_qp = u1_avg_mpeg2_qp_temp;

            u1_num_frms_input++;
        }

        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (MAX_FRAMES_MODELLED - 1);
        else
            u1_curr_frame_index--;
    }

    /************************************************************************/
    /* Add Pivot Points to the Data set to be used for finding Quadratic    */
    /* Model Coeffs. These will help in constraining the shape of  Quadratic*/
    /* to adapt too much to the Local deviations.                           */
    /************************************************************************/
    u1_min2_mpeg2_qp = u1_min_mpeg2_qp;
    u1_max2_mpeg2_qp = u1_max_mpeg2_qp;
    u1_min_qp_frame_indx = INVALID_FRAME_INDEX;
    u1_max_qp_frame_indx = INVALID_FRAME_INDEX;

    /* Loop runnning over the Stored Frame Level Data
       to find frames of MinQp and MaxQp */
    for(; i < ps_rd_model->u1_num_frms_in_model; i++)
    {
        u1_num_skips_temp = ps_rd_model->pu1_num_skips[u1_curr_frame_index];
        u1_avg_mpeg2_qp_temp = ps_rd_model->pu1_avg_qp[u1_curr_frame_index];

        if(((0 == u4_num_skips) && (0 != u1_num_skips_temp)) ||
           ((1 == u4_num_skips) && (u1_num_skips_temp > 1)))
            continue;

        if(u1_min2_mpeg2_qp > u1_avg_mpeg2_qp_temp)
        {
            u1_min2_mpeg2_qp = u1_avg_mpeg2_qp_temp;
            u1_min_qp_frame_indx = u1_curr_frame_index;
        }
        if(u1_max2_mpeg2_qp < u1_avg_mpeg2_qp_temp)
        {
            u1_max2_mpeg2_qp = u1_avg_mpeg2_qp_temp;
            u1_max_qp_frame_indx = u1_curr_frame_index;
        }
        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (MAX_FRAMES_MODELLED - 1);
        else
            u1_curr_frame_index--;
    }

    /* Add the Chosen Points to the regression data set */
    if(INVALID_FRAME_INDEX != u1_min_qp_frame_indx)
    {
        pi1_frame_index[u1_num_frms_input] = (WORD8)u1_min_qp_frame_indx;
        u1_num_frms_input++;
    }
    if(INVALID_FRAME_INDEX != u1_max_qp_frame_indx)
    {
        pi1_frame_index[u1_num_frms_input] = (WORD8)u1_max_qp_frame_indx;
        u1_num_frms_input++;
    }
    memcpy(pi1_frame_index_initial, pi1_frame_index, MAX_FRAMES_MODELLED);

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        if(u1_num_frms_input < (MIN_FRAMES_FOR_QUAD_MODEL))
            ps_rd_model->u1_model_used = LIN_MODEL;
        if((WORD32)u1_max_mpeg2_qp < ((WORD32)(21 * u1_min_mpeg2_qp) >> 4))
            ps_rd_model->u1_model_used = LIN_MODEL;
    }

    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        if(u1_num_frms_input < MIN_FRAMES_FOR_LIN_MODEL)
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
        if((WORD32)u1_max_mpeg2_qp < ((WORD32)(19 * u1_min_mpeg2_qp) >> 4))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }

    /***** Call the Module to Return the Coeffs for the Fed Data *****/
    ps_rd_model->u1_model_used = find_model_coeffs(
        ps_rd_model->pi4_res_bits,
        ps_rd_model->pi4_sad,
        ps_rd_model->pu1_num_skips,
        ps_rd_model->pu1_avg_qp,
        u1_num_frms_input,
        ps_rd_model->u1_model_used,
        pi1_frame_index,
        model_coeff_array,
        model_coeff_array_lin,
        model_coeff_array_lin_wo_int,
        ps_rd_model);

    if((model_coeff_array_lin[2] > 0) || (model_coeff_array_lin[0] < 0))
        u1_lin_model_valid = 0;
    else
    {
        u1_lin_model_valid = 1;
        /* lin deviation calculation */
        calc_avg_sqr_dev_for_model(
            ps_rd_model->pi4_res_bits,
            ps_rd_model->pi4_sad,
            ps_rd_model->pu1_num_skips,
            ps_rd_model->pu1_avg_qp,
            u1_num_frms_input,
            pi1_frame_index_initial,
            model_coeff_array_lin,
            &fl_lin_avg_dev);
    }

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        u1_check_model = refine_set_of_points(
            ps_rd_model->pi4_res_bits,
            ps_rd_model->pi4_sad,
            ps_rd_model->pu1_num_skips,
            ps_rd_model->pu1_avg_qp,
            u1_num_frms_input,
            pi1_frame_index,
            model_coeff_array,
            &fl_quad_avg_dev);

        if(2 == u1_check_model)
        {
            ps_rd_model->u1_model_used = QUAD_MODEL;
        }
        else
        {
            /*******************************************************************/
            /* Make sure that some of the Pivot Points are used in the Refined */
            /* data set. 1. Previous Frame                                     */
            /*******************************************************************/
            /*pi1_frame_index[0] = ps_rd_model->u1_curr_frm_counter;*/

            ps_rd_model->u1_model_used = find_model_coeffs(
                ps_rd_model->pi4_res_bits,
                ps_rd_model->pi4_sad,
                ps_rd_model->pu1_num_skips,
                ps_rd_model->pu1_avg_qp,
                u1_num_frms_input,
                ps_rd_model->u1_model_used,
                pi1_frame_index,
                model_coeff_array,
                NULL,
                NULL,
                ps_rd_model);

            u1_check_model = refine_set_of_points(
                ps_rd_model->pi4_res_bits,
                ps_rd_model->pi4_sad,
                ps_rd_model->pu1_num_skips,
                ps_rd_model->pu1_avg_qp,
                u1_num_frms_input,
                pi1_frame_index,
                model_coeff_array,
                &fl_quad_avg_dev);

            if((0 == u1_check_model))
            {
#if RC_MODEL_USED_BUG_FIX
                if((fl_lin_avg_dev < fl_quad_avg_dev) && (1 == u1_lin_model_valid))
#endif
                    ps_rd_model->u1_model_used = LIN_MODEL;
            }
        }
    }

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        /*min_res_bits = model_coeff_c -
                       ((model_coeff_a * model_coeff_a) / (4 * model_coeff_b));*/

        if(model_coeff_array[0] < 0)
            ps_rd_model->u1_model_used = LIN_MODEL;

        /*if ((model_coeff_a * model_coeff_b) > 0)
             u1_model_used = LIN_MODEL;*/
    }
    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        if((model_coeff_array_lin[2] > 0) || (model_coeff_array_lin[0] < 0))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }

#if RC_MODEL_USED_BUG_FIX
    /* Another threshold of .25 on deviation i.e. deviation greater than 25%  */
    if((QUAD_MODEL == ps_rd_model->u1_model_used) && (fl_quad_avg_dev > .25))
        ps_rd_model->u1_model_used = PREV_FRAME_MODEL;

    if((LIN_MODEL == ps_rd_model->u1_model_used) && (fl_lin_avg_dev > .25))
        ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
#endif /* #if RC_MODEL_USED_BUG_FIX */

    ps_rd_model->model_coeff_b_quad = model_coeff_array[0];
    ps_rd_model->model_coeff_a_quad = model_coeff_array[1];
    ps_rd_model->model_coeff_c_quad = model_coeff_array[2];

    ps_rd_model->model_coeff_b_lin = model_coeff_array_lin[0];
    ps_rd_model->model_coeff_a_lin = model_coeff_array_lin[1];
    ps_rd_model->model_coeff_c_lin = model_coeff_array_lin[2];

    ps_rd_model->model_coeff_b_lin_wo_int = model_coeff_array_lin_wo_int[0];
    ps_rd_model->model_coeff_a_lin_wo_int = model_coeff_array_lin_wo_int[1];
    ps_rd_model->model_coeff_c_lin_wo_int = model_coeff_array_lin_wo_int[2];

    /*ps_rd_model->u1_model_used = PREV_FRAME_MODEL;*/
}
#endif /* ENABLE_QUAD_MODEL */

UWORD32 estimate_bits_for_qp(rc_rd_model_t *ps_rd_model, UWORD32 u4_estimated_sad, UWORD8 u1_avg_qp)
{
    float fl_num_bits;
    /*ps_rd_model += u1_curr_pic_type;*/

    {
        fl_num_bits =
            ps_rd_model->model_coeff_a_lin_wo_int * ((float)(u4_estimated_sad / u1_avg_qp));
    }

    return ((UWORD32)fl_num_bits);
}

UWORD8 find_qp_for_target_bits(
    rc_rd_model_t *ps_rd_model,
    UWORD32 u4_target_res_bits,
    UWORD32 u4_estimated_sad,
    UWORD8 u1_min_qp,
    UWORD8 u1_max_qp)
{
    UWORD8 u1_qp;
    float x_value, f_qp;
    /*ps_rd_model += u1_curr_pic_type;*/
#if ENABLE_QUAD_MODEL
    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        float det;
        det = (ps_rd_model->model_coeff_a_quad * ps_rd_model->model_coeff_a_quad) -
              (4 * (ps_rd_model->model_coeff_b_quad) *
               (ps_rd_model->model_coeff_c_quad - u4_target_res_bits));

        if(det > 0)
        {
            x_value = sqrt(det);

            x_value =
                (x_value - ps_rd_model->model_coeff_a_quad) / (2 * ps_rd_model->model_coeff_b_quad);
        }
        else
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }

    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        x_value = ((float)u4_target_res_bits - ps_rd_model->model_coeff_c_lin) /
                  (ps_rd_model->model_coeff_b_lin);
    }
#else
    ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
#endif

    if(PREV_FRAME_MODEL == ps_rd_model->u1_model_used)
    {
        x_value = (float)u4_target_res_bits / ps_rd_model->model_coeff_a_lin_wo_int;
    }

    if(0 != x_value)
        f_qp = u4_estimated_sad / x_value;
    else
        f_qp = 255;

    if(f_qp > 255)
        f_qp = 255;

    /* Truncating the QP to the Max and Min Qp values possible */
    if(f_qp < u1_min_qp)
        f_qp = u1_min_qp;
    if(f_qp > u1_max_qp)
        f_qp = u1_max_qp;

    u1_qp = (UWORD8)(f_qp + 0.5);

    return u1_qp;
}

void add_frame_to_rd_model(
    rc_rd_model_t *ps_rd_model,
    UWORD32 i4_res_bits,
    UWORD8 u1_avg_mp2qp,
    UWORD32 i4_sad_h264,
    UWORD8 u1_num_skips)
{
    UWORD8 u1_curr_frame_index;
    /*ps_rd_model += u1_curr_pic_type;*/
    u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;
    /*** Insert the Present Frame Data into the RD Model State Memory ***/
    ps_rd_model->pi4_res_bits[u1_curr_frame_index] = i4_res_bits;
    ps_rd_model->pi4_sad[u1_curr_frame_index] = i4_sad_h264;
    ps_rd_model->pu1_num_skips[u1_curr_frame_index] = u1_num_skips;
    ps_rd_model->pu1_avg_qp[u1_curr_frame_index] = u1_avg_mp2qp;

    ps_rd_model->u1_curr_frm_counter++;
    if(MAX_FRAMES_MODELLED == ps_rd_model->u1_curr_frm_counter)
        ps_rd_model->u1_curr_frm_counter = 0;

    if(ps_rd_model->u1_num_frms_in_model < ps_rd_model->u1_max_frms_to_model)
    {
        ps_rd_model->u1_num_frms_in_model++;
    }
    update_frame_rd_model(ps_rd_model);
}

WORD32 calc_per_frm_bits(
    rc_rd_model_t *ps_rd_model, /* array of model structs */
    UWORD16 *pu2_num_pics_of_a_pic_type, /* N1, N2,...Nk */
    UWORD8 *
        pu1_update_pic_type_model, /* flag which tells whether or not to update model coefficients of a particular pic-type */
    UWORD8 u1_num_pic_types, /* value of k */
    UWORD32 *
        pu4_num_skip_of_a_pic_type, /* the number of skips of that pic-type. It "may" be used to update the model coefficients at a later point. Right now it is not being used at all. */
    UWORD8 u1_base_pic_type, /* base pic type index wrt which alpha & beta are calculated */
    float *pfl_gamma, /* gamma_i = beta_i / alpha_i */
    float *pfl_eta,
    UWORD8
        u1_curr_pic_type, /* the current pic-type for which the targetted bits need to be computed */
    UWORD32
        u4_bits_for_sub_gop, /* the number of bits to be consumed for the remaining part of sub-gop */
    UWORD32 u4_curr_estimated_sad,
    UWORD8 *pu1_curr_pic_type_qp) /* output of this function */
{
    WORD32 i4_per_frm_bits_Ti;
    UWORD8 u1_i;
    rc_rd_model_t *ps_rd_model_of_pic_type;

    /* first part of this function updates all the model coefficients */
    /*for all the pic-types */
    {
        for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
        {
            if((0 != pu2_num_pics_of_a_pic_type[u1_i]) && (1 == pu1_update_pic_type_model[u1_i]))
            {
                /* ps_rd_model_of_pic_type = ps_rd_model + u1_i; */

                update_frame_rd_model(&ps_rd_model[u1_i]);
            }
        }
    }

    /* The second part of this function deals with solving the
    equation using all the pic-types models */

    {
        UWORD8 u1_combined_model_used;

        /* first choose the model to be used */
        u1_combined_model_used = QUAD_MODEL;

        for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
        {
            ps_rd_model_of_pic_type = ps_rd_model + u1_i;

            if((0 != pu2_num_pics_of_a_pic_type[u1_i]) &&
               (QUAD_MODEL != ps_rd_model_of_pic_type->u1_model_used))
            {
                u1_combined_model_used = LIN_MODEL;
                break;
            }
        }

        if(u1_combined_model_used == LIN_MODEL)
        {
            for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
            {
                ps_rd_model_of_pic_type = ps_rd_model + u1_i;

                if((0 != pu2_num_pics_of_a_pic_type[u1_i]) &&
                   (QUAD_MODEL != ps_rd_model_of_pic_type->u1_model_used) &&
                   (LIN_MODEL != ps_rd_model_of_pic_type->u1_model_used))
                {
                    u1_combined_model_used = PREV_FRAME_MODEL;
                    break;
                }
            }
        }

        /* solve the equation for the */
        {
            model_coeff eff_A;
            model_coeff eff_B;
            model_coeff eff_C;
            float fl_determinant;
            float fl_sad_by_qp_base;
            float fl_sad_by_qp_curr_frm;
            float fl_qp_curr_frm;
            float fl_bits_for_curr_frm;

            /* If the combined chosen model is quad model */
            if(QUAD_MODEL == u1_combined_model_used)
            {
                eff_A = 0.0;
                eff_B = 0.0;
                eff_C = 0.0;
                for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
                {
                    ps_rd_model_of_pic_type = ps_rd_model + u1_i;

                    eff_A +=
                        ((pfl_eta[u1_i] + pu2_num_pics_of_a_pic_type[u1_i] - 1) *
                         ps_rd_model_of_pic_type->model_coeff_a_quad * pfl_gamma[u1_i]);
                    eff_B +=
                        ((pfl_eta[u1_i] * pfl_eta[u1_i] + pu2_num_pics_of_a_pic_type[u1_i] - 1) *
                         ps_rd_model_of_pic_type->model_coeff_b_quad * pfl_gamma[u1_i] *
                         pfl_gamma[u1_i]);
                    eff_C +=
                        (pu2_num_pics_of_a_pic_type[u1_i] *
                         ps_rd_model_of_pic_type->model_coeff_c_quad);
                }
                eff_C -= u4_bits_for_sub_gop;

                fl_determinant = eff_A * eff_A - 4 * eff_B * eff_C;

                if(fl_determinant < 0)
                {
                    u1_combined_model_used =
                        PREV_FRAME_MODEL; /* TO BE replaced by LIN_MODEL later */
                }
                else
                {
                    fl_determinant = sqrt(fl_determinant);

                    fl_sad_by_qp_base = fl_determinant - eff_A;
                    fl_sad_by_qp_base = fl_sad_by_qp_base / (2 * eff_B);

                    fl_sad_by_qp_curr_frm =
                        fl_sad_by_qp_base * pfl_gamma[u1_curr_pic_type] * pfl_eta[u1_curr_pic_type];

                    ps_rd_model_of_pic_type = ps_rd_model + u1_curr_pic_type;

                    fl_bits_for_curr_frm =
                        ps_rd_model_of_pic_type->model_coeff_a_quad * fl_sad_by_qp_curr_frm +
                        ps_rd_model_of_pic_type->model_coeff_b_quad * fl_sad_by_qp_curr_frm *
                            fl_sad_by_qp_curr_frm +
                        ps_rd_model_of_pic_type->model_coeff_c_quad;
                }
            }

            /* If the combined chosen model is linear model with an intercept */
            if(LIN_MODEL == u1_combined_model_used)
            {
                eff_A = 0.0;
                eff_B = 0.0;
                eff_C = 0.0;
                for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
                {
                    ps_rd_model_of_pic_type = ps_rd_model + u1_i;

                    eff_A +=
                        ((pfl_eta[u1_i] + pu2_num_pics_of_a_pic_type[u1_i] - 1) *
                         ps_rd_model_of_pic_type->model_coeff_a_lin * pfl_gamma[u1_i]);

                    eff_C +=
                        (pu2_num_pics_of_a_pic_type[u1_i] *
                         ps_rd_model_of_pic_type->model_coeff_c_lin);
                }
                eff_C -= u4_bits_for_sub_gop;

                fl_determinant = (-(eff_C / eff_A));

                if((fl_determinant) <= 0)
                {
                    u1_combined_model_used = PREV_FRAME_MODEL;
                }
                else
                {
                    fl_sad_by_qp_base = fl_determinant;

                    fl_sad_by_qp_curr_frm =
                        fl_sad_by_qp_base * pfl_gamma[u1_curr_pic_type] * pfl_eta[u1_curr_pic_type];

                    ps_rd_model_of_pic_type = ps_rd_model + u1_curr_pic_type;

                    fl_bits_for_curr_frm =
                        ps_rd_model_of_pic_type->model_coeff_a_lin * fl_sad_by_qp_curr_frm +
                        ps_rd_model_of_pic_type->model_coeff_c_lin;
                }
            }

            /* If the combined chosen model is linear model without an intercept */
            if(PREV_FRAME_MODEL == u1_combined_model_used)
            {
                eff_A = 0.0;
                eff_B = 0.0;
                eff_C = 0.0;
                for(u1_i = 0; u1_i < u1_num_pic_types; u1_i++)
                {
                    ps_rd_model_of_pic_type = ps_rd_model + u1_i;

                    eff_A +=
                        ((pfl_eta[u1_i] + pu2_num_pics_of_a_pic_type[u1_i] - 1) *
                         ps_rd_model_of_pic_type->model_coeff_a_lin_wo_int * pfl_gamma[u1_i]);
                }

                fl_sad_by_qp_base = u4_bits_for_sub_gop / eff_A;

                fl_sad_by_qp_curr_frm =
                    fl_sad_by_qp_base * pfl_gamma[u1_curr_pic_type] * pfl_eta[u1_curr_pic_type];

                ps_rd_model_of_pic_type = ps_rd_model + u1_curr_pic_type;

                fl_bits_for_curr_frm =
                    ps_rd_model_of_pic_type->model_coeff_a_lin_wo_int * fl_sad_by_qp_curr_frm;
            }

            /* store the model that was finally used to calculate Qp.
            This is so that the same model is used in further calculations for this picture. */
            ps_rd_model_of_pic_type = ps_rd_model + u1_curr_pic_type;
            ps_rd_model_of_pic_type->u1_model_used = u1_combined_model_used;

            i4_per_frm_bits_Ti = (WORD32)(fl_bits_for_curr_frm + 0.5);

            if(fl_sad_by_qp_curr_frm > 0)
                fl_qp_curr_frm = (float)u4_curr_estimated_sad / fl_sad_by_qp_curr_frm;
            else
                fl_qp_curr_frm = 255;

            if(fl_qp_curr_frm > 255)
                fl_qp_curr_frm = 255;

            *pu1_curr_pic_type_qp = (fl_qp_curr_frm + 0.5);
        }
    }
    return (i4_per_frm_bits_Ti);
}

model_coeff get_linear_coefficient(rc_rd_model_t *ps_rd_model)
{
    /*UWORD32 linear_coeff:
    linear_coeff = ps_rd_model->model_coeff_a_lin_wo_int;*/
    return (ps_rd_model->model_coeff_a_lin_wo_int);
}
#endif /* !(RC_FIXED_POINT) */
WORD32 rc_rd_model_dummy_for_avoiding_warnings(
    rc_rd_model_t **pps_rc_rd_model, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type)
{
    WORD32 i4_mem_tab_idx = 0;
    static rc_rd_model_t s_rc_rd_model_temp;

    /* Hack for al alloc, during which we dont have any state memory.
      Dereferencing can cause issues */
    if(e_func_type == GET_NUM_MEMTAB || e_func_type == FILL_MEMTAB)
        (*pps_rc_rd_model) = &s_rc_rd_model_temp;

    /*for src rate control state structure*/
    if(e_func_type != GET_NUM_MEMTAB)
    {
        fill_memtab(
            &ps_memtab[i4_mem_tab_idx], sizeof(rc_rd_model_t), MEM_TAB_ALIGNMENT, PERSISTENT, DDR);
        use_or_fill_base(&ps_memtab[0], (void **)pps_rc_rd_model, e_func_type);
    }
    i4_mem_tab_idx++;

    return (i4_mem_tab_idx);
}
