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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* User include files */
#include "ittiam_datatypes.h"
#include "var_q_operator.h"
#include "rc_common.h"
#include "rc_cntrl_param.h"
#include "mem_req_and_acq.h"
#include "rc_rd_model.h"
#include "rc_rd_model_struct.h"

#if RC_FIXED_POINT
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
/******************************************************************************
  Function Name   : init_frm_rc_rd_model
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void init_frm_rc_rd_model(rc_rd_model_t *ps_rd_model, UWORD8 u1_max_frames_modelled)
{
    /* ps_rd_model = ps_rd_model + u1_pic_type; */

    ps_rd_model->u1_num_frms_in_model = 0;
    ps_rd_model->u1_curr_frm_counter = 0;
    ps_rd_model->u1_max_frms_to_model = u1_max_frames_modelled;

    ps_rd_model->model_coeff_a_quad.sm = 0;
    ps_rd_model->model_coeff_b_quad.sm = 0;
    ps_rd_model->model_coeff_c_quad.sm = 0;

    ps_rd_model->model_coeff_a_lin.sm = 0;
    ps_rd_model->model_coeff_b_lin.sm = 0;
    ps_rd_model->model_coeff_c_lin.sm = 0;

    ps_rd_model->model_coeff_a_lin_wo_int.sm = 0;
    ps_rd_model->model_coeff_b_lin_wo_int.sm = 0;
    ps_rd_model->model_coeff_c_lin_wo_int.sm = 0;

    ps_rd_model->model_coeff_a_quad.e = 0;
    ps_rd_model->model_coeff_b_quad.e = 0;
    ps_rd_model->model_coeff_c_quad.e = 0;

    ps_rd_model->model_coeff_a_lin.e = 0;
    ps_rd_model->model_coeff_b_lin.e = 0;
    ps_rd_model->model_coeff_c_lin.e = 0;

    ps_rd_model->model_coeff_a_lin_wo_int.e = 0;
    ps_rd_model->model_coeff_b_lin_wo_int.e = 0;
    ps_rd_model->model_coeff_c_lin_wo_int.e = 0;
}
/******************************************************************************
  Function Name   : reset_frm_rc_rd_model
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void reset_frm_rc_rd_model(rc_rd_model_t *ps_rd_model)
{
    ps_rd_model->u1_num_frms_in_model = 0;
    ps_rd_model->u1_curr_frm_counter = 0;

    ps_rd_model->model_coeff_a_quad.sm = 0;
    ps_rd_model->model_coeff_b_quad.sm = 0;
    ps_rd_model->model_coeff_c_quad.sm = 0;

    ps_rd_model->model_coeff_a_lin.sm = 0;
    ps_rd_model->model_coeff_b_lin.sm = 0;
    ps_rd_model->model_coeff_c_lin.sm = 0;

    ps_rd_model->model_coeff_a_lin_wo_int.sm = 0;
    ps_rd_model->model_coeff_b_lin_wo_int.sm = 0;
    ps_rd_model->model_coeff_c_lin_wo_int.sm = 0;

    ps_rd_model->model_coeff_a_quad.e = 0;
    ps_rd_model->model_coeff_b_quad.e = 0;
    ps_rd_model->model_coeff_c_quad.e = 0;

    ps_rd_model->model_coeff_a_lin.e = 0;
    ps_rd_model->model_coeff_b_lin.e = 0;
    ps_rd_model->model_coeff_c_lin.e = 0;

    ps_rd_model->model_coeff_a_lin_wo_int.e = 0;
    ps_rd_model->model_coeff_b_lin_wo_int.e = 0;
    ps_rd_model->model_coeff_c_lin_wo_int.e = 0;
}

#if ENABLE_QUAD_MODEL
/******************************************************************************
  Function Name   : find_model_coeffs
  Description     :
  Arguments       :
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
static UWORD8 find_model_coeffs(
    UWORD32 *pi4_res_bits,
    LWORD64 *pi8_sad_h264,
    WORD32 *pi4_avg_mpeg2_qp_q6,
    UWORD8 u1_num_frms,
    UWORD8 u1_model_used,
    WORD8 *pi1_frame_index,
    number_t *pmc_model_coeff,
    number_t *pmc_model_coeff_lin,
    number_t *pmc_model_coeff_lin_wo_int,
    rc_rd_model_t *ps_rd_model)
{
    UWORD32 i;
    UWORD8 u1_num_frms_used = 0;
    UWORD8 u1_frm_indx;

    number_t sum_y;
    number_t sum_x_y;
    number_t sum_x2_y;
    number_t sum_x;
    number_t sum_x2;
    number_t sum_x3;
    number_t sum_x4;
    number_t var_x2_y;
    number_t var_x_y;
    number_t var_x2_x;
    number_t var_x2_x2;
    number_t var_x_x;
    number_t x0, y0;
    number_t s_res_bits, s_sad_h264, s_avg_mpeg2_qp;
    number_t temp, temp1;

    number_t model_coeff_a, model_coeff_b, model_coeff_c, model_coeff_den;

    number_t s_num_frms_used;

    /* initilising */
    model_coeff_a.sm = 0;
    model_coeff_a.e = 0;
    model_coeff_b.sm = 0;
    model_coeff_b.e = 0;
    model_coeff_c.sm = 0;
    model_coeff_c.e = 0;

    sum_y.sm = 0;
    sum_x_y.sm = 0;
    sum_x2_y.sm = 0;
    sum_x.sm = 0;
    sum_x2.sm = 0;
    sum_x3.sm = 0;
    sum_x4.sm = 0;
    var_x2_y.sm = 0;
    var_x_y.sm = 0;
    var_x2_x.sm = 0;
    var_x2_x2.sm = 0;
    var_x_x.sm = 0;

    sum_y.e = 0;
    sum_x_y.e = 0;
    sum_x2_y.e = 0;
    sum_x.e = 0;
    sum_x2.e = 0;
    sum_x3.e = 0;
    sum_x4.e = 0;
    var_x2_y.e = 0;
    var_x_y.e = 0;
    var_x2_x.e = 0;
    var_x2_x2.e = 0;
    var_x_x.e = 0;

    for(i = 0; i < u1_num_frms; i++)
    {
        LWORD64 i8_local_sad_sm = 0;
        WORD32 i4_local_e = 0;
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        s_res_bits.sm = pi4_res_bits[u1_frm_indx];
        s_res_bits.e = 0;

        /*s_sad_h264.sm = pi8_sad_h264[u1_frm_indx];
        s_sad_h264.e  = 0;*/
        i8_local_sad_sm = pi8_sad_h264[u1_frm_indx];

        while(i8_local_sad_sm > 0x7FFFFFFF)
        {
            i8_local_sad_sm = i8_local_sad_sm / 2;
            i4_local_e++;
        }
        SET_VARQ_FRM_FIXQ(((WORD32)i8_local_sad_sm), s_sad_h264, -i4_local_e);

        /*fract_quant*/
        SET_VARQ_FRM_FIXQ(pi4_avg_mpeg2_qp_q6[u1_frm_indx], s_avg_mpeg2_qp, QSCALE_Q_FAC);

        y0 = s_res_bits;
        /*x0 = (float) (pi4_sad_h264[u1_frm_indx] /
                     (float)pui_avg_mpeg2_qp[u1_frm_indx]); */
        div32_var_q(s_sad_h264, s_avg_mpeg2_qp, &x0);

        /*
        sum_y    += y0;
        sum_x_y  += x0 * y0;
        sum_x2_y += x0 * x0 * y0;
        sum_x    += x0;
        sum_x2   += x0 * x0;
        sum_x3   += x0 * x0 * x0;
        sum_x4   += x0 * x0 * x0 * x0;
        */
        /* sum_y    += y0; */
        add32_var_q(sum_y, y0, &sum_y);
        /* sum_x_y  += x0 * y0; */
        mult32_var_q(x0, y0, &temp);
        add32_var_q(sum_x_y, temp, &sum_x_y);

        /* sum_x2_y += x0 * x0 * y0; */
        mult32_var_q(x0, temp, &temp);
        add32_var_q(sum_x2_y, temp, &sum_x2_y);

        /* sum_x    += x0; */
        add32_var_q(x0, sum_x, &sum_x);

        /* sum_x2   += x0 * x0; */
        mult32_var_q(x0, x0, &temp);
        add32_var_q(temp, sum_x2, &sum_x2);

        /* sum_x3   += x0 * x0 * x0; */
        mult32_var_q(x0, temp, &temp);
        add32_var_q(temp, sum_x3, &sum_x3);

        /* sum_x4   += x0 * x0 * x0 * x0; */
        mult32_var_q(x0, temp, &temp);
        add32_var_q(temp, sum_x4, &sum_x4);

        u1_num_frms_used++;
    }

    s_num_frms_used.sm = u1_num_frms_used;
    s_num_frms_used.e = 0;

    /* sum_y    /= u1_num_frms_used; */
    div32_var_q(sum_y, s_num_frms_used, &sum_y);
    /* sum_x_y  /= u1_num_frms_used; */
    div32_var_q(sum_x_y, s_num_frms_used, &sum_x_y);
    /* sum_x2_y /= u1_num_frms_used; */
    div32_var_q(sum_x2_y, s_num_frms_used, &sum_x2_y);

    /* sum_x    /= u1_num_frms_used; */
    div32_var_q(sum_x, s_num_frms_used, &sum_x);

    /* sum_x2   /= u1_num_frms_used; */
    div32_var_q(sum_x2, s_num_frms_used, &sum_x2);

    /* sum_x3   /= u1_num_frms_used; */
    div32_var_q(sum_x3, s_num_frms_used, &sum_x3);

    /* sum_x4   /= u1_num_frms_used; */
    div32_var_q(sum_x4, s_num_frms_used, &sum_x4);

#if !QUAD
    u1_model_used = LIN_MODEL;
#endif

    if((QUAD_MODEL == u1_model_used) && (u1_num_frms_used <= MIN_FRAMES_FOR_QUAD_MODEL))
    {
        u1_model_used = LIN_MODEL;
    }

    if(QUAD_MODEL == u1_model_used)
    {
        /* var_x2_y  = sum_x2_y - sum_x2 * sum_y; */
        mult32_var_q(sum_x2, sum_y, &temp);
        sub32_var_q(sum_x2_y, temp, &var_x2_y);

        /* var_x_y   = sum_x_y  - sum_x  * sum_y; */
        mult32_var_q(sum_x, sum_y, &temp);
        sub32_var_q(sum_x_y, temp, &var_x_y);

        /* var_x2_x  = sum_x3   - sum_x2 * sum_x; */
        mult32_var_q(sum_x2, sum_x, &temp);
        sub32_var_q(sum_x3, temp, &var_x2_x);

        /* var_x2_x2 = sum_x4   - sum_x2 * sum_x2; */
        mult32_var_q(sum_x2, sum_x2, &temp);
        sub32_var_q(sum_x4, temp, &var_x2_x2);

        /* var_x_x   = sum_x2   - sum_x  * sum_x; */
        mult32_var_q(sum_x, sum_x, &temp);
        sub32_var_q(sum_x2, temp, &var_x_x);

        /* model_coeff_den = (var_x2_x * var_x2_x - var_x2_x2 * var_x_x); */
        mult32_var_q(var_x2_x, var_x2_x, &temp);
        mult32_var_q(var_x2_x2, var_x_x, &temp1);
        sub32_var_q(temp, temp1, &model_coeff_den);

        if(0 != model_coeff_den.sm)
        {
            /* model_coeff_b   = (var_x_y * var_x2_x - var_x2_y * var_x_x); */
            mult32_var_q(var_x_y, var_x2_x, &temp);
            mult32_var_q(var_x2_y, var_x_x, &temp1);
            sub32_var_q(temp, temp1, &model_coeff_b);

            /* model_coeff_b   /= model_coeff_den; */
            div32_var_q(model_coeff_b, model_coeff_den, &model_coeff_b);

            /* model_coeff_a   = (var_x2_y * var_x2_x - var_x_y * var_x2_x2); */
            mult32_var_q(var_x2_y, var_x2_x, &temp);
            mult32_var_q(var_x_y, var_x2_x2, &temp1);
            sub32_var_q(temp, temp1, &model_coeff_a);

            /* model_coeff_a   /= model_coeff_den; */
            div32_var_q(model_coeff_a, model_coeff_den, &model_coeff_a);

            /*model_coeff_c   = sum_y - (model_coeff_a * sum_x) -
                              (model_coeff_b * sum_x2); */
            mult32_var_q(model_coeff_a, sum_x, &temp);
            mult32_var_q(model_coeff_b, sum_x2, &temp1);
            sub32_var_q(sum_y, temp, &model_coeff_c);
            sub32_var_q(model_coeff_c, temp1, &model_coeff_c);
            /* till here */
        }

        pmc_model_coeff[0] = model_coeff_b;
        /* pmc_model_coeff[0] = (float)(model_coeff_b.sm /pow(2,model_coeff_b.e)); */
        pmc_model_coeff[1] = model_coeff_a;
        /* pmc_model_coeff[1] = (float)(model_coeff_a.sm /pow(2,model_coeff_a.e)); */
        pmc_model_coeff[2] = model_coeff_c;
        /* pmc_model_coeff[2] = (float)(model_coeff_c.sm /pow(2,model_coeff_c.e)); */
    }

    if(NULL != pmc_model_coeff_lin)
    {
        /* var_x_y   = sum_x_y  - sum_x  * sum_y; */
        mult32_var_q(sum_x, sum_y, &temp);
        sub32_var_q(sum_x_y, temp, &var_x_y);

        /* var_x_x   = sum_x2   - sum_x  * sum_x; */
        mult32_var_q(sum_x, sum_x, &temp);
        sub32_var_q(sum_x2, temp, &var_x_x);

        if((0 != var_x_x.sm) && (u1_num_frms > 1))
        {
            /* model_coeff_b = (var_x_y / var_x_x); */
            div32_var_q(var_x_y, var_x_x, &model_coeff_b);

            /* model_coeff_c = sum_y - (model_coeff_b * sum_x); */
            mult32_var_q(model_coeff_b, sum_x, &temp);
            sub32_var_q(sum_y, temp, &model_coeff_c);

            model_coeff_a = model_coeff_b;

            pmc_model_coeff_lin[0] = model_coeff_b;
            /* pmc_model_coeff_lin[0] = (float)(model_coeff_b.sm /pow(2,model_coeff_b.e)); */

            pmc_model_coeff_lin[1] = model_coeff_a;
            /* pmc_model_coeff_lin[1] = (float)(model_coeff_a.sm /pow(2,model_coeff_a.e)); */

            pmc_model_coeff_lin[2] = model_coeff_c;
            /* pmc_model_coeff_lin[2] = (float)(model_coeff_c.sm /pow(2,model_coeff_c.e)); */
        }
    }

    /*  TO DO : FLOAT_TO_FIX */
    if(NULL != pmc_model_coeff_lin_wo_int)
    {
        UWORD8 u1_curr_frame_index;
        /* UWORD8 u1_avgqp_prvfrm; */
        number_t s_avgqp_prvfrm;
        /* UWORD32 u4_prevfrm_bits, u4_prevfrm_sad; */
        number_t s_prevfrm_bits, s_prevfrm_sad;
        WORD32 i4_local_e = 0;
        LWORD64 i8_local_sad_sm = 0;
        u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;
        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (UWORD8)(ps_rd_model->u1_max_frms_to_model - 1);
        else
            u1_curr_frame_index--;

        /* u1_avgqp_prvfrm = ps_rd_model->pu1_avg_mp2qp[u1_curr_frame_index]; */
        /*fract_quant*/
        SET_VARQ_FRM_FIXQ(
            ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index], s_avgqp_prvfrm, QSCALE_Q_FAC);

        /* u4_prevfrm_bits = ps_rd_model->pi4_res_bits[u1_curr_frame_index]; */
        s_prevfrm_bits.sm = ps_rd_model->pi4_res_bits[u1_curr_frame_index];
        s_prevfrm_bits.e = 0;

        /* u4_prevfrm_sad  = ps_rd_model->pi4_sad_h264[u1_curr_frame_index]; */
        /*s_prevfrm_sad.sm = ps_rd_model->pi8_sad[u1_curr_frame_index];
        s_prevfrm_sad.e  = 0;*/
        i8_local_sad_sm = ps_rd_model->pi8_sad[u1_curr_frame_index];
        while(i8_local_sad_sm > 0x7FFFFFFF)
        {
            i8_local_sad_sm = i8_local_sad_sm / 2;
            i4_local_e++;
        }
        SET_VARQ_FRM_FIXQ(((WORD32)i8_local_sad_sm), s_prevfrm_sad, -i4_local_e);

        if(0 != s_prevfrm_sad.sm)
        {
            /* model_coeff_a = (float)(u4_prevfrm_bits * u1_avgqp_prvfrm) / u4_prevfrm_sad; */
            mult32_var_q(s_prevfrm_bits, s_avgqp_prvfrm, &model_coeff_a);
            div32_var_q(model_coeff_a, s_prevfrm_sad, &model_coeff_a);
        }
        else
        {
            model_coeff_a.sm = 0;
            model_coeff_a.e = 0;
        }

        model_coeff_b.sm = 0;
        model_coeff_b.e = 0;
        model_coeff_c.sm = 0;
        model_coeff_c.e = 0;

        pmc_model_coeff_lin_wo_int[0] = model_coeff_b;
        pmc_model_coeff_lin_wo_int[1] = model_coeff_a;
        pmc_model_coeff_lin_wo_int[2] = model_coeff_c;
    }
    /* end of "TO DO : FLOAT_TO_FIX" */

    return u1_model_used;
}

/******************************************************************************
  Function Name   : refine_set_of_points
  Description     :
  Arguments       :
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
static WORD8 refine_set_of_points(
    UWORD32 *pi4_res_bits,
    LWORD64 *pi8_sad_h264,
    WORD32 *pi4_avg_mpeg2_qp_q6,
    UWORD8 u1_num_frms,
    WORD8 *pi1_frame_index,
    number_t *ps_model_coeff,
    number_t *ps_avg_deviation)
{
    /* float  fl_avg_deviation, fl_estimated_bits, fl_deviation, x_val; */
    number_t s_avg_deviation, s_estimated_bits, s_deviation, x_val;
    /* number_t  ps_model_coeff[3]; */
    number_t s_sad_h264, s_avg_mpeg2_qp, s_res_bits;
    number_t temp, temp1;
    UWORD8 u1_return_value = 1;
    UWORD32 i;
    UWORD8 u1_num_frms_used, u1_frm_indx;
    number_t s_num_frms_used;

    /*
    convert_float_to_fix(pmc_model_coeff[0],&ps_model_coeff[0]);
    convert_float_to_fix(pmc_model_coeff[1],&ps_model_coeff[1]);
    convert_float_to_fix(pmc_model_coeff[2],&ps_model_coeff[2]);
    */

    u1_num_frms_used = 0;
    /* fl_avg_deviation = 0; */
    s_avg_deviation.sm = 0;
    s_avg_deviation.e = 0;

    for(i = 0; i < u1_num_frms; i++)
    {
        LWORD64 i8_local_sad_sm = 0;
        WORD32 i4_local_e = 0;
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];
        /*x_val = pi4_sad_h264[u1_frm_indx] /
                (float) pui_avg_mpeg2_qp[u1_frm_indx]; */
        /* s_sad_h264.sm = pi8_sad_h264[u1_frm_indx];
        s_sad_h264.e  = 0;*/

        i8_local_sad_sm = pi8_sad_h264[u1_frm_indx];
        while(i8_local_sad_sm > 0x7FFFFFFF)
        {
            i8_local_sad_sm = i8_local_sad_sm / 2;
            i4_local_e++;
        }
        SET_VARQ_FRM_FIXQ(((WORD32)i8_local_sad_sm), s_sad_h264, -i4_local_e);

        /*fract_quant*/
        SET_VARQ_FRM_FIXQ(pi4_avg_mpeg2_qp_q6[u1_frm_indx], s_avg_mpeg2_qp, QSCALE_Q_FAC);

        div32_var_q(s_sad_h264, s_avg_mpeg2_qp, &x_val);

        /*
        fl_estimated_bits = (pmc_model_coeff[0] * x_val * x_val ) +
                            (pmc_model_coeff[1] * x_val) +
                            (pmc_model_coeff[2]);
                            */
        mult32_var_q(x_val, x_val, &temp);
        mult32_var_q(temp, ps_model_coeff[0], &temp);
        mult32_var_q(x_val, ps_model_coeff[1], &temp1);
        add32_var_q(temp, temp1, &s_estimated_bits);
        add32_var_q(s_estimated_bits, ps_model_coeff[2], &s_estimated_bits);

        /*
        fl_deviation        = fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) /
                              (float) pi4_res_bits[u1_frm_indx];
                              */
        s_res_bits.sm = pi4_res_bits[u1_frm_indx];
        s_res_bits.e = 0;
        sub32_var_q(s_res_bits, s_estimated_bits, &temp);
        temp.sm = (temp.sm > 0) ? temp.sm : (-temp.sm);
        div32_var_q(temp, s_res_bits, &s_deviation);

        /* fl_deviation        = fl_deviation * fl_deviation; */
        mult32_var_q(s_deviation, s_deviation, &s_deviation);

        /* fl_avg_deviation    += fl_deviation;*/
        add32_var_q(s_avg_deviation, s_deviation, &s_avg_deviation);

        u1_num_frms_used++;
    }

    /* fl_avg_deviation /= u1_num_frms_used; */
    s_num_frms_used.sm = u1_num_frms_used;
    s_num_frms_used.e = 0;
    div32_var_q(s_avg_deviation, s_num_frms_used, &s_avg_deviation);

    /* fl_avg_deviation = sqrt(fl_avg_deviation); */
    /* fl_avg_deviation = (fl_avg_deviation); */

    for(i = 0; i < u1_num_frms; i++)
    {
        LWORD64 i8_local_sad_sm = 0;
        WORD32 i4_local_e = 0;
        if ((-1 == pi1_frame_index[i]) /*&&
            (i != 0)*/)
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        /*
        x_val = pi4_sad_h264[u1_frm_indx] /
                (float) pui_avg_mpeg2_qp[u1_frm_indx];
                */

        /* s_sad_h264.sm = pi8_sad_h264[u1_frm_indx];
        s_sad_h264.e  = 0;*/

        i8_local_sad_sm = pi8_sad_h264[u1_frm_indx];
        while(i8_local_sad_sm > 0x7FFFFFFF)
        {
            i8_local_sad_sm = i8_local_sad_sm / 2;
            i4_local_e++;
        }
        SET_VARQ_FRM_FIXQ(((WORD32)i8_local_sad_sm), s_sad_h264, -i4_local_e);

        /*fract_quant*/
        SET_VARQ_FRM_FIXQ(pi4_avg_mpeg2_qp_q6[u1_frm_indx], s_avg_mpeg2_qp, QSCALE_Q_FAC);

        div32_var_q(s_sad_h264, s_avg_mpeg2_qp, &x_val);

        /*
        fl_estimated_bits = (pmc_model_coeff[0] * x_val * x_val ) +
                            (pmc_model_coeff[1] * x_val) +
                            (pmc_model_coeff[2]);
                            */
        mult32_var_q(x_val, x_val, &temp);
        mult32_var_q(temp, ps_model_coeff[0], &temp);
        mult32_var_q(x_val, ps_model_coeff[1], &temp1);
        add32_var_q(temp, temp1, &s_estimated_bits);
        add32_var_q(s_estimated_bits, ps_model_coeff[2], &s_estimated_bits);

        /*
        fl_deviation      = fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) /
                            (float) pi4_res_bits[u1_frm_indx];
                            */
        s_res_bits.sm = pi4_res_bits[u1_frm_indx];
        s_res_bits.e = 0;
        sub32_var_q(s_res_bits, s_estimated_bits, &temp);
        temp.sm = (temp.sm > 0) ? temp.sm : (-temp.sm);
        div32_var_q(temp, s_res_bits, &s_deviation);

        /* to remove the sqrt function */
        /*fl_deviation = fl_deviation * fl_deviation; */
        mult32_var_q(s_deviation, s_deviation, &s_deviation);

        /*
        if (fl_deviation > (fl_avg_deviation))
        {
            pi1_frame_index[i] = -1;
        }
        */
        sub32_var_q(s_deviation, s_avg_deviation, &temp);
        if(temp.sm > 0)
        {
            pi1_frame_index[i] = -1;
        }
    }

    {
        number_t up_thr, lo_thr;

        /*
        if (fl_avg_deviation > 0.0625)
        u1_return_value = 0;
        */
        up_thr.sm = UP_THR_SM;
        up_thr.e = UP_THR_E;
        sub32_var_q(s_avg_deviation, up_thr, &temp);
        if(temp.sm > 0)
        {
            u1_return_value = 0;
        }

        /*
        if (fl_avg_deviation < 0.0225)
            u1_return_value = 2;
            */
        lo_thr.sm = LO_THR_SM;
        lo_thr.e = LO_THR_E;
        sub32_var_q(s_avg_deviation, lo_thr, &temp);
        if(temp.sm < 0)
        {
            u1_return_value = 2;
        }
    }
    *ps_avg_deviation = s_avg_deviation;
    return (u1_return_value);
}
/******************************************************************************
  Function Name   : calc_avg_sqr_dev_for_model
  Description     :
  Arguments       :
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
/* TO DO : FLOAT_TO_FIX */
static void calc_avg_sqr_dev_for_model(
    UWORD32 *pi4_res_bits,
    LWORD64 *pi8_sad_h264,
    WORD32 *pi4_avg_mpeg2_qp_q6,
    UWORD8 u1_num_frms,
    WORD8 *pi1_frame_index,
    number_t *ps_model_coeff,
    number_t *ps_avg_deviation)
{
    /* float  fl_avg_deviation, fl_estimated_bits, fl_deviation, x_val; */
    number_t s_avg_deviation, s_estimated_bits, s_deviation, x_val;
    /* UWORD8 u1_return_value = 1; */
    UWORD32 i;
    UWORD8 u1_num_frms_used, u1_frm_indx;

    number_t s_sad_h264;
    number_t s_avg_mpeg2_qp;
    number_t s_res_bits;
    number_t temp;
    number_t s_num_frms_used;

    u1_num_frms_used = 0;
    /* fl_avg_deviation = 0; */
    s_deviation.sm = 0;
    s_deviation.e = 0;

    s_avg_deviation.sm = 0;
    s_avg_deviation.e = 0;

    for(i = 0; i < u1_num_frms; i++)
    {
        LWORD64 i8_local_sad_sm;
        WORD32 i4_local_e = 0;
        if(-1 == pi1_frame_index[i])
            continue;

        u1_frm_indx = (UWORD8)pi1_frame_index[i];

        u1_frm_indx = (UWORD8)i;
        /*
        x_val = pi4_sad_h264[u1_frm_indx] /
                (float) pui_avg_mpeg2_qp[u1_frm_indx];
                */
        /* s_sad_h264.sm = pi8_sad_h264[u1_frm_indx];
        s_sad_h264.e  = 0;*/
        i8_local_sad_sm = pi8_sad_h264[u1_frm_indx];
        while(i8_local_sad_sm > 0x7FFFFFFF)
        {
            i8_local_sad_sm = i8_local_sad_sm / 2;
            i4_local_e++;
        }
        SET_VARQ_FRM_FIXQ(((WORD32)i8_local_sad_sm), s_sad_h264, -i4_local_e);
        /*fract_quant*/
        SET_VARQ_FRM_FIXQ(pi4_avg_mpeg2_qp_q6[u1_frm_indx], s_avg_mpeg2_qp, QSCALE_Q_FAC);

        div32_var_q(s_sad_h264, s_avg_mpeg2_qp, &x_val);

        /*fl_estimated_bits = (pmc_model_coeff[1] * x_val) +
                            (pmc_model_coeff[2]); */
        mult32_var_q(x_val, ps_model_coeff[1], &s_estimated_bits);
        add32_var_q(s_estimated_bits, ps_model_coeff[2], &s_estimated_bits);

        /*fl_deviation        = fabs(pi4_res_bits[u1_frm_indx] - fl_estimated_bits) /
                              (float) pi4_res_bits[u1_frm_indx]; */
        s_res_bits.sm = pi4_res_bits[u1_frm_indx];
        s_res_bits.e = 0;
        sub32_var_q(s_res_bits, s_estimated_bits, &temp);
        temp.sm = (temp.sm > 0) ? temp.sm : (-temp.sm);
        div32_var_q(temp, s_res_bits, &s_deviation);

        /* fl_deviation        = fl_deviation * fl_deviation; */
        mult32_var_q(s_deviation, s_deviation, &s_deviation);

        /* fl_avg_deviation    += fl_deviation; */
        add32_var_q(s_avg_deviation, s_deviation, &s_avg_deviation);

        u1_num_frms_used++;
    }

    /* fl_avg_deviation /= u1_num_frms_used; */
    s_num_frms_used.sm = u1_num_frms_used;
    s_num_frms_used.e = 0;
    div32_var_q(s_avg_deviation, s_num_frms_used, &s_avg_deviation);
    *ps_avg_deviation = s_avg_deviation;
}
/* end of "TO DO : FLOAT_TO_FIX" */
/******************************************************************************
  Function Name   : is_qp_available
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
static WORD32 is_qp_available(
    rc_rd_model_t *ps_rd_model, UWORD8 u1_curr_frame_index, WORD32 i4_num_frames_to_check)
{
    WORD32 i;
    /*fract_quant*/
    WORD32 i4_qp = ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index];
    WORD32 i4_num_frms = 0;

    for(i = 0; i < i4_num_frames_to_check; i++)
    {
        u1_curr_frame_index++;
        if(ps_rd_model->u1_max_frms_to_model == u1_curr_frame_index)
            u1_curr_frame_index = 0;
        /*fract_quant*/
        if(ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index] == i4_qp)
            i4_num_frms++;
    }
    if(i4_num_frms >= 2)
        return (1);
    else
        return (0);
}
/****************************************************************************/
/*                                                                          */
/*  Function Name : example_of_a_function                                   */
/*                                                                          */
/*  Description   : This function illustrates the use of C coding standards.*/
/*                  switch/case, if, for, block comments have been shown    */
/*                  here.                                                   */
/*  Inputs        : <What inputs does the function take?>                   */
/*  Globals       : <Does it use any global variables?>                     */
/*  Processing    : <Describe how the function operates - include algorithm */
/*                  description>                                            */
/*  Outputs       : <What does the function produce?>                       */
/*  Returns       : <What does the function return?>                        */
/*                                                                          */
/*  Issues        : <List any issues or problems with this function>        */
/*                                                                          */
/*  Revision History:                                                       */
/*                                                                          */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made) */
/*         13 07 2002   Ittiam          Draft                               */
/*                                                                          */
/****************************************************************************/
static void update_frame_rd_model(rc_rd_model_t *ps_rd_model)
{
    WORD8 pi1_frame_index[MAX_FRAMES_MODELLED];
    WORD8 pi1_frame_index_initial[MAX_FRAMES_MODELLED];
    UWORD32 u4_num_skips;

    UWORD8 u1_num_skips_temp;
    /*UWORD8  u1_avg_mpeg2_qp_temp, u1_min_mpeg2_qp, u1_max_mpeg2_qp; */
    /*WORD32  i4_avg_mpeg2_qp_temp, i4_min_mpeg2_qp, i4_max_mpeg2_qp;*/
    WORD32 i4_avg_mpeg2_qp_temp_q6, i4_min_mpeg2_qp_q6, i4_max_mpeg2_qp_q6;
    UWORD8 u1_num_frms_input, u1_num_active_frames, u1_reject_frame;

    /* UWORD8  u1_min2_mpeg2_qp, u1_max2_mpeg2_qp; */
    /* WORD32  i4_min2_mpeg2_qp, i4_max2_mpeg2_qp;*/
    WORD32 i4_min2_mpeg2_qp_q6, i4_max2_mpeg2_qp_q6;
    UWORD8 u1_min_qp_frame_indx, u1_max_qp_frame_indx;

    number_t model_coeff_array[3], model_coeff_array_lin[3];
    number_t model_coeff_array_lin_wo_int[3];
    WORD32 i;
    UWORD8 u1_curr_frame_index;

#if RC_MODEL_USED_BUG_FIX
    UWORD8 u1_lin_model_valid;
#endif

    number_t s_quad_avg_sqr_dev, s_lin_avg_sqr_dev;

    UWORD8 u1_check_model;

    model_coeff_array[0].sm = 0;
    model_coeff_array[0].e = 0;
    model_coeff_array[1].sm = 0;
    model_coeff_array[1].e = 0;
    model_coeff_array[2].sm = 0;
    model_coeff_array[2].e = 0;

    model_coeff_array_lin[0].sm = 0;
    model_coeff_array_lin[0].e = 0;
    model_coeff_array_lin[1].sm = 0;
    model_coeff_array_lin[1].e = 0;
    model_coeff_array_lin[2].sm = 0;
    model_coeff_array_lin[2].e = 0;

    model_coeff_array_lin_wo_int[0].sm = 0;
    model_coeff_array_lin_wo_int[0].e = 0;
    model_coeff_array_lin_wo_int[1].sm = 0;
    model_coeff_array_lin_wo_int[1].e = 0;
    model_coeff_array_lin_wo_int[2].sm = 0;
    model_coeff_array_lin_wo_int[2].e = 0;

    /* ps_rd_model += u1_pic_type; */

    u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;

    ps_rd_model->u1_model_used = QUAD_MODEL;

    if(0 == u1_curr_frame_index)
        u1_curr_frame_index = (UWORD8)(ps_rd_model->u1_max_frms_to_model - 1);
    else
        u1_curr_frame_index--;

    /************************************************************************/
    /* Rearrange data to be fed into a Linear Regression Module             */
    /* Module finds a,b,c such that                                         */
    /*      y = ax + bx^2 + c                                               */
    /************************************************************************/
    u4_num_skips = 0;
    u1_num_frms_input = 0;
    /*memset(ps_rd_model->au1_num_frames,   0, MPEG2_QP_ELEM);*/
    memset(pi1_frame_index, -1, MAX_FRAMES_MODELLED);
    /*i4_min_mpeg2_qp = MAX_MPEG2_QP;
    i4_max_mpeg2_qp = 0;*/

    i4_min_mpeg2_qp_q6 = (MAX_MPEG2_QP << QSCALE_Q_FAC);
    i4_max_mpeg2_qp_q6 = MIN_QSCALE_Q6;

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
        /* WORD32 i4_test1 = 0, i4_test2 = 0; NITT TBD */
        u1_reject_frame = 0;
        u1_num_skips_temp = ps_rd_model->pu1_num_skips[u1_curr_frame_index];
        /*fract_quant*/
        /*i4_avg_mpeg2_qp_temp = (ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index] >> QSCALE_Q_FAC);*/
        i4_avg_mpeg2_qp_temp_q6 = ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index];

        if((0 == u4_num_skips) && (0 != u1_num_skips_temp))
            u1_reject_frame = 1;
        if((1 == u4_num_skips) && (u1_num_skips_temp > 1))
            u1_reject_frame = 1;
        /* If there is already a frame having same qp reject the current frame */
        if(is_qp_available(ps_rd_model, u1_curr_frame_index, i))
            u1_reject_frame = 1;
        /*if (ps_rd_model->au1_num_frames[i4_avg_mpeg2_qp_temp]  >= 2)
        {
            u1_reject_frame = 1;
            i4_test2 = 1;
        }
        if(i4_test2 != i4_test1)
        {
            printf("Why am I here??\n");
        }*/

        if(0 == i)
            u1_reject_frame = 0;

        if(0 == u1_reject_frame)
        {
            pi1_frame_index[u1_num_frms_input] = (WORD8)u1_curr_frame_index;
            /* ps_rd_model->au1_num_frames[i4_avg_mpeg2_qp_temp] += 1; */

            /*if (i4_min_mpeg2_qp > i4_avg_mpeg2_qp_temp) i4_min_mpeg2_qp = i4_avg_mpeg2_qp_temp;
            if (i4_max_mpeg2_qp < i4_avg_mpeg2_qp_temp) i4_max_mpeg2_qp = i4_avg_mpeg2_qp_temp;*/

            if(i4_min_mpeg2_qp_q6 > i4_avg_mpeg2_qp_temp_q6)
                i4_min_mpeg2_qp_q6 = i4_avg_mpeg2_qp_temp_q6;
            if(i4_max_mpeg2_qp_q6 < i4_avg_mpeg2_qp_temp_q6)
                i4_max_mpeg2_qp_q6 = i4_avg_mpeg2_qp_temp_q6;
            u1_num_frms_input++;
        }

        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (UWORD8)(ps_rd_model->u1_max_frms_to_model - 1);
        else
            u1_curr_frame_index--;
    }

    /************************************************************************/
    /* Add Pivot Points to the Data set to be used for finding Quadratic    */
    /* Model Coeffs. These will help in constraining the shape of  Quadratic*/
    /* to adapt too much to the Local deviations.                           */
    /************************************************************************/
    /*i4_min2_mpeg2_qp     = i4_min_mpeg2_qp;
    i4_max2_mpeg2_qp     = i4_max_mpeg2_qp;*/

    i4_min2_mpeg2_qp_q6 = i4_min_mpeg2_qp_q6;
    i4_max2_mpeg2_qp_q6 = i4_max_mpeg2_qp_q6;

    u1_min_qp_frame_indx = INVALID_FRAME_INDEX;
    u1_max_qp_frame_indx = INVALID_FRAME_INDEX;

    /* Loop runnning over the Stored Frame Level Data
       to find frames of MinQp and MaxQp */
    for(; i < ps_rd_model->u1_num_frms_in_model; i++)
    {
        u1_num_skips_temp = ps_rd_model->pu1_num_skips[u1_curr_frame_index];
        /*fract_quant*/
        //i4_avg_mpeg2_qp_temp = ps_rd_model->ai4_avg_qp[u1_curr_frame_index];

        //i4_avg_mpeg2_qp_temp = (ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index] >> QSCALE_Q_FAC);

        i4_avg_mpeg2_qp_temp_q6 = ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index];

        if(((0 == u4_num_skips) && (0 != u1_num_skips_temp)) ||
           ((1 == u4_num_skips) && (u1_num_skips_temp > 1)))
            continue;
        /*
        if (i4_min2_mpeg2_qp > i4_avg_mpeg2_qp_temp)
        {
            i4_min2_mpeg2_qp     = i4_avg_mpeg2_qp_temp;
            u1_min_qp_frame_indx = u1_curr_frame_index;
        }
        if (i4_max2_mpeg2_qp < i4_avg_mpeg2_qp_temp)
        {
            i4_max2_mpeg2_qp     = i4_avg_mpeg2_qp_temp;
            u1_max_qp_frame_indx = u1_curr_frame_index;
        }
*/

        if(i4_min2_mpeg2_qp_q6 > i4_avg_mpeg2_qp_temp_q6)
        {
            i4_min2_mpeg2_qp_q6 = i4_avg_mpeg2_qp_temp_q6;
            u1_min_qp_frame_indx = u1_curr_frame_index;
        }
        if(i4_max2_mpeg2_qp_q6 < i4_avg_mpeg2_qp_temp_q6)
        {
            i4_max2_mpeg2_qp_q6 = i4_avg_mpeg2_qp_temp_q6;
            u1_max_qp_frame_indx = u1_curr_frame_index;
        }

        if(0 == u1_curr_frame_index)
            u1_curr_frame_index = (UWORD8)(ps_rd_model->u1_max_frms_to_model - 1);
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

    /* memcpy(pi1_frame_index_initial, pi1_frame_index, MAX_FRAMES_MODELLED); */
    {
        UWORD8 u1_k;
        for(u1_k = 0; u1_k < MAX_FRAMES_MODELLED; u1_k++)
        {
            pi1_frame_index_initial[u1_k] = pi1_frame_index[u1_k];
        }
    }

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        if(u1_num_frms_input < (MIN_FRAMES_FOR_QUAD_MODEL))
            ps_rd_model->u1_model_used = LIN_MODEL;
        if((WORD32)i4_max_mpeg2_qp_q6 < ((WORD32)(21 * i4_min_mpeg2_qp_q6) >> 4))
            ps_rd_model->u1_model_used = LIN_MODEL;
    }

    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        if(u1_num_frms_input < MIN_FRAMES_FOR_LIN_MODEL)
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
        if((WORD32)i4_max_mpeg2_qp_q6 < ((WORD32)(19 * i4_min_mpeg2_qp_q6) >> 4))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }

    /***** Call the Module to Return the Coeffs for the Fed Data *****/
    ps_rd_model->u1_model_used = find_model_coeffs(
        ps_rd_model->pi4_res_bits,
        ps_rd_model->pi8_sad,
        ps_rd_model->ai4_avg_qp_q6,
        u1_num_frms_input,
        ps_rd_model->u1_model_used,
        pi1_frame_index,
        model_coeff_array,
        model_coeff_array_lin,
        model_coeff_array_lin_wo_int,
        ps_rd_model);

    if((model_coeff_array_lin[2].sm > 0) || (model_coeff_array_lin[0].sm < 0))
    {
#if RC_MODEL_USED_BUG_FIX
        u1_lin_model_valid = 0;
#endif
    }
    else
    {
#if RC_MODEL_USED_BUG_FIX
        u1_lin_model_valid = 1;
#endif
        /* lin deviation calculation */
        calc_avg_sqr_dev_for_model(
            ps_rd_model->pi4_res_bits,
            ps_rd_model->pi8_sad,
            ps_rd_model->ai4_avg_qp_q6,
            u1_num_frms_input,
            pi1_frame_index_initial,
            model_coeff_array_lin,
            &s_lin_avg_sqr_dev);
    }

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        u1_check_model = refine_set_of_points(
            ps_rd_model->pi4_res_bits,
            ps_rd_model->pi8_sad,
            ps_rd_model->ai4_avg_qp_q6,
            u1_num_frms_input,
            pi1_frame_index,
            model_coeff_array,
            &s_quad_avg_sqr_dev);

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
            /* pi1_frame_index[0] = ps_rd_model->u1_curr_frm_counter; */

            ps_rd_model->u1_model_used = find_model_coeffs(
                ps_rd_model->pi4_res_bits,
                ps_rd_model->pi8_sad,
                ps_rd_model->ai4_avg_qp_q6,
                u1_num_frms_input,
                ps_rd_model->u1_model_used,
                pi1_frame_index,
                model_coeff_array,
                NULL,
                NULL,
                ps_rd_model);

            u1_check_model = refine_set_of_points(
                ps_rd_model->pi4_res_bits,
                ps_rd_model->pi8_sad,
                ps_rd_model->ai4_avg_qp_q6,
                u1_num_frms_input,
                pi1_frame_index,
                model_coeff_array,
                &s_quad_avg_sqr_dev);

            if((0 == u1_check_model))
            {
#if RC_MODEL_USED_BUG_FIX
                if((s_lin_avg_sqr_dev < s_quad_avg_sqr_dev) && (1 == u1_lin_model_valid))
#endif
                    ps_rd_model->u1_model_used = LIN_MODEL;
            }
        }
    }

    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        /* min_res_bits = model_coeff_c -  */
        /*               ((model_coeff_a * model_coeff_a) / (4 * model_coeff_b)); */

        if(model_coeff_array[0].sm < 0)
            ps_rd_model->u1_model_used = LIN_MODEL;

        /* if ((model_coeff_a * model_coeff_b) > 0) */
        /*    u1_model_used = LIN_MODEL; */

        ps_rd_model->model_coeff_b_quad = model_coeff_array[0];
        ps_rd_model->model_coeff_a_quad = model_coeff_array[1];
        ps_rd_model->model_coeff_c_quad = model_coeff_array[2];
    }
    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        if((model_coeff_array_lin[2].sm > 0) || (model_coeff_array_lin[0].sm < 0))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }
/* TO DO : FLOAT_TO_FIX */
#if RC_MODEL_USED_BUG_FIX
    {
        number_t s_quad_dev_thr;
        number_t s_lin_dev_thr;
        number_t s_diff;

        s_quad_dev_thr.sm = QUAD_DEV_THR_SM;
        s_quad_dev_thr.e = QUAD_DEV_THR_E;

        /* (s_quad_avg_sqr_dev > .25) */
        sub32_var_q(s_quad_avg_sqr_dev, s_quad_dev_thr, &s_diff);

        /* Another threshold of .25 on deviation i.e. deviation greater than 25%  */
        if((QUAD_MODEL == ps_rd_model->u1_model_used) && (s_diff.sm > 0))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;

        s_lin_dev_thr.sm = LIN_DEV_THR_SM;
        s_lin_dev_thr.e = LIN_DEV_THR_E;

        /* (s_lin_avg_sqr_dev > .25) */
        sub32_var_q(s_lin_avg_sqr_dev, s_lin_dev_thr, &s_diff);

        if((LIN_MODEL == ps_rd_model->u1_model_used) && (s_diff.sm > 0))
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
    }
#endif /* #if RC_MODEL_USED_BUG_FIX */
    /* end of "TO DO : FLOAT_TO_FIX" */
    ps_rd_model->model_coeff_b_lin = model_coeff_array_lin[0];
    ps_rd_model->model_coeff_a_lin = model_coeff_array_lin[1];
    ps_rd_model->model_coeff_c_lin = model_coeff_array_lin[2];
    ps_rd_model->model_coeff_b_lin_wo_int = model_coeff_array_lin_wo_int[0];
    ps_rd_model->model_coeff_a_lin_wo_int = model_coeff_array_lin_wo_int[1];
    ps_rd_model->model_coeff_c_lin_wo_int = model_coeff_array_lin_wo_int[2];
    /* ps_rd_model->u1_model_used = PREV_FRAME_MODEL; */
}
#endif

/******************************************************************************
  Function Name   : estimate_bits_for_qp
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
UWORD32
    estimate_bits_for_qp(rc_rd_model_t *ps_rd_model, UWORD32 u4_estimated_sad, WORD32 i4_avg_qp_q6)
{
    /* float fl_num_bits; */
    number_t s_num_bits;
    number_t s_estimated_sad, s_avg_qp;

    /* number_t s_model_coeff_a, s_model_coeff_b, s_model_coeff_c; */
    WORD32 i4_temp;
    number_t x_val;

    /* ps_rd_model += u1_curr_pic_type; */
    s_estimated_sad.sm = u4_estimated_sad;
    s_estimated_sad.e = 0;
    /*fract_quant*/
    SET_VARQ_FRM_FIXQ(i4_avg_qp_q6, s_avg_qp, QSCALE_Q_FAC);
    /* initilising s_num_bits */
    s_num_bits.sm = 0;
    s_num_bits.e = 0;

    /*
    convert_float_to_fix(ps_rd_model->model_coeff_a, &s_model_coeff_a);
    convert_float_to_fix(ps_rd_model->model_coeff_b, &s_model_coeff_b);
    convert_float_to_fix(ps_rd_model->model_coeff_c, &s_model_coeff_c);
    */
    div32_var_q(s_estimated_sad, s_avg_qp, &x_val);
    {
        /* TO DO : FLOAT_TO_FIX */
        /* fl_num_bits = ps_rd_model->model_coeff_a_lin_wo_int * x_val; */
        mult32_var_q(ps_rd_model->model_coeff_a_lin_wo_int, x_val, &s_num_bits);
        /* end of "TO DO : FLOAT_TO_FIX" */
    }

    /* return ((UWORD32) fl_num_bits); */
    number_t_to_word32(s_num_bits, &i4_temp);
    if(i4_temp < 0)
        i4_temp = 0;
    return ((UWORD32)i4_temp);
}

/******************************************************************************
  Function Name   : find_qp_for_target_bits
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
WORD32 find_qp_for_target_bits(
    rc_rd_model_handle ps_rd_model,
    UWORD32 u4_target_res_bits,
    UWORD32 u4_estimated_sad,
    WORD32 i4_max_qp_q6,
    WORD32 i4_min_qp_q6)
{
    WORD32 i4_qp_q6;
    /* float  x_value, f_qp; */
    number_t x_value, s_qp;
    /* number_t s_model_coeff_a, s_model_coeff_b, s_model_coeff_c; */
    number_t s_target_res_bits;
    number_t s_estimated_sad;
    number_t temp, temp3;
    number_t temp2, temp1;

    /* ps_rd_model += u1_curr_pic_type; */

    s_target_res_bits.sm = u4_target_res_bits;
    s_target_res_bits.e = 0;

    s_estimated_sad.sm = u4_estimated_sad;
    s_estimated_sad.e = 0;

    /* initilising default value */
    x_value.sm = 0;
    x_value.e = 0;

    /*
    convert_float_to_fix(ps_rd_model->model_coeff_a, &(ps_rd_model->s_model_coeff_a));
    convert_float_to_fix(ps_rd_model->model_coeff_b, &(ps_rd_model->s_model_coeff_b));
    convert_float_to_fix(ps_rd_model->model_coeff_c, &(ps_rd_model->s_model_coeff_c));
    */

#if ENABLE_QUAD_MODEL
    if(QUAD_MODEL == ps_rd_model->u1_model_used)
    {
        /* float det; */
        number_t det;

        /*
        det = (ps_rd_model->model_coeff_a * ps_rd_model->model_coeff_a) -
              (4 * (ps_rd_model->model_coeff_b) *
               (ps_rd_model->model_coeff_c - u4_target_res_bits));
        */
        mult32_var_q(ps_rd_model->model_coeff_a_quad, ps_rd_model->model_coeff_a_quad, &temp);
        temp3.sm = 4;
        temp3.e = 0;
        mult32_var_q(temp3, ps_rd_model->model_coeff_b_quad, &temp1);
        sub32_var_q(ps_rd_model->model_coeff_c_quad, s_target_res_bits, &temp2);
        mult32_var_q(temp1, temp2, &temp1);
        sub32_var_q(temp, temp1, &det);

        /* x_value = sqrt(det); */
        sqrt32_var_q(det, &x_value);

        /* x_value = (x_value - ps_rd_model->model_coeff_a) /
            (2 * ps_rd_model->model_coeff_b);
        */
        sub32_var_q(x_value, ps_rd_model->model_coeff_a_quad, &temp);
        temp3.sm = 2;
        temp3.e = 0;
        mult32_var_q(temp3, ps_rd_model->model_coeff_b_quad, &temp1);
        div32_var_q(temp, temp1, &x_value);

        if(det.sm < 0 || x_value.sm < 0)
        {
            /* x_value = 0; */
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
        }
    }

    if(LIN_MODEL == ps_rd_model->u1_model_used)
    {
        /*
        x_value = ((float)u4_target_res_bits - ps_rd_model->model_coeff_c) /
                   (ps_rd_model->model_coeff_b);
        */
        sub32_var_q(s_target_res_bits, ps_rd_model->model_coeff_c_lin, &temp);
        div32_var_q(temp, ps_rd_model->model_coeff_b_lin, &x_value);
        if(x_value.sm < 0)
        {
            /* x_value = 0; */
            ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
        }
    }
#else
    ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
#endif
    if(PREV_FRAME_MODEL == ps_rd_model->u1_model_used)
    {
        /* TO DO : FLOAT_TO_FIX */
        /* x_value = (float) u4_target_res_bits / ps_rd_model->model_coeff_a_lin_wo_int; */
        div32_var_q(s_target_res_bits, ps_rd_model->model_coeff_a_lin_wo_int, &x_value);
        /* end of "TO DO : FLOAT_TO_FIX" */
    }

    if(0 != x_value.sm)
    {
        /* f_qp = u4_estimated_sad / x_value; */
        div32_var_q(s_estimated_sad, x_value, &s_qp);
    }
    else
    {
        s_qp.sm = MAX_MPEG2_QP;
        s_qp.e = 0;
    }

    /*
    if (f_qp > MAX_MPEG2_QP)
    f_qp = MAX_MPEG2_QP;
    */
    temp3.sm = MAX_MPEG2_QP;
    temp3.e = 0;
    sub32_var_q(s_qp, temp3, &temp);
    if(temp.sm > 0)
    {
        s_qp = temp3;
    }
    convert_varq_to_fixq(s_qp, &i4_qp_q6, (WORD32)QSCALE_Q_FAC);
    /* Truncating the QP to the Max and Min Qp values possible */
    if(i4_qp_q6 < i4_min_qp_q6)
    {
        i4_qp_q6 = i4_min_qp_q6;
    }
    if(i4_qp_q6 > i4_max_qp_q6)
    {
        i4_qp_q6 = i4_max_qp_q6;
    }
    return (i4_qp_q6);
}
/******************************************************************************
  Function Name   : add_frame_to_rd_model
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void add_frame_to_rd_model(
    rc_rd_model_t *ps_rd_model,
    UWORD32 i4_res_bits,
    WORD32 i4_avg_mp2qp_q6,
    LWORD64 i8_sad_h264,
    UWORD8 u1_num_skips)
{
    UWORD8 u1_curr_frame_index, i4_same_bit_count = 0;
    /* ps_rd_model += u1_curr_pic_type; */
    u1_curr_frame_index = ps_rd_model->u1_curr_frm_counter;

    {
        WORD32 i;

        i = ps_rd_model->u1_num_frms_in_model - 1;
        while(i >= 0)
        {
            if(ps_rd_model->pi4_res_bits[i] == i4_res_bits)
                i4_same_bit_count++;
            i--;
        }
    }
    /* - the condition check is a temporary fix to avoid feeding zero into model.
    The change should be done so that 0 is not at all fed into model. When texture bit consumption becomes zero next frame qp should be explicitly decreased so that finite amount of texture
    bits is consumed and feeds valid data to model to come out of deadlock*/

    if(i4_same_bit_count < 3)
    {
        /*** Insert the Present Frame Data into the RD Model State Memory ***/
        ps_rd_model->pi4_res_bits[u1_curr_frame_index] = i4_res_bits;
        ps_rd_model->pi8_sad[u1_curr_frame_index] = i8_sad_h264;
        ps_rd_model->pu1_num_skips[u1_curr_frame_index] = u1_num_skips;
        ps_rd_model->ai4_avg_qp[u1_curr_frame_index] = (i4_avg_mp2qp_q6 >> QSCALE_Q_FAC);
        ps_rd_model->ai4_avg_qp_q6[u1_curr_frame_index] = i4_avg_mp2qp_q6;

        ps_rd_model->u1_curr_frm_counter++;
        if(ps_rd_model->u1_max_frms_to_model == ps_rd_model->u1_curr_frm_counter)
            ps_rd_model->u1_curr_frm_counter = 0;

        if(ps_rd_model->u1_num_frms_in_model < ps_rd_model->u1_max_frms_to_model)
        {
            ps_rd_model->u1_num_frms_in_model++;
        }
        update_frame_rd_model(ps_rd_model);
    }
}
/******************************************************************************
  Function Name   : get_linear_coefficient
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
number_t get_linear_coefficient(rc_rd_model_t *ps_rd_model)
{
    return (ps_rd_model->model_coeff_a_lin_wo_int);
}
/******************************************************************************
  Function Name   : set_linear_coefficient
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
void set_linear_coefficient(rc_rd_model_t *ps_rd_model, number_t model_coeff_a_lin_wo_int)
{
    ps_rd_model->model_coeff_a_lin_wo_int = model_coeff_a_lin_wo_int;
    ps_rd_model->u1_model_used = PREV_FRAME_MODEL;
}
/******************************************************************************
  Function Name   : is_model_valid
  Description     :
  Arguments       : ps_rd_model
  Return Values   : void
  Revision History:
                    Creation
*****************************************************************************/
WORD32 is_model_valid(rc_rd_model_t *ps_rd_model)
{
    /*return 1 if atleast one data point is availbale: this is required because frames with zero texture consumption is not updated in model*/
    if(ps_rd_model->u1_num_frms_in_model > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

#endif /* #if RC_FIXED_POINT */
