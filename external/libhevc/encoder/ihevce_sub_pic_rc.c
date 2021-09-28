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
* \file ihevce_sub_pic_rc.c
*
* \brief
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* List of Functions
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
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
#include "ihevc_debug.h"
#include "ihevc_structs.h"
#include "ihevc_platform_macros.h"
#include "ihevc_deblk.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_weighted_pred.h"
#include "ihevc_sao.h"
#include "ihevc_resi_trans.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_cabac_tables.h"

#include "ihevce_defs.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_checks.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_trace.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_entropy_interface.h"
#include "ihevce_enc_loop_structs.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_interface.h"
#include "ihevce_sub_pic_rc.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
/* @ brief : Qp deviation of -6 to 6 is mapped */
float qp_scale_dev[13] = { 0.5,   0.56,  0.630, 0.707, 0.794, 0.891, 1.00,
                           1.122, 1.259, 1.414, 1.587, 1.782, 2.00 };

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define IN_FRAME_RC_PRINT 0
#define IN_FRAME_RC_FRAME_NUM 4

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_sub_pic_rc_bits_fill \endif
*
* \brief
*   Sub-pic RC thread interface function
*
* \param[in] Frame process pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_sub_pic_rc_in_data(
    void *pv_multi_thrd_ctxt, void *pv_ctxt, void *pv_ctb_ipe_analyse, void *pv_frm_ctb_prms)
{
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;
    ihevce_enc_loop_ctxt_t *ps_ctxt = (ihevce_enc_loop_ctxt_t *)pv_ctxt;
    ipe_l0_ctb_analyse_for_me_t *ps_ctb_ipe_analyse =
        (ipe_l0_ctb_analyse_for_me_t *)pv_ctb_ipe_analyse;
    frm_ctb_ctxt_t *ps_frm_ctb_prms = (frm_ctb_ctxt_t *)pv_frm_ctb_prms;

    WORD32 j = 0;
    WORD32 i4_frm_id = ps_ctxt->i4_enc_frm_id;
    WORD32 i4_br_id = ps_ctxt->i4_bitrate_instance_num;
    WORD32 i4_thrd_id = ps_ctxt->thrd_id;
    WORD32 i4_ctb_count_flag = 0;
    WORD32 i4_is_intra_pic = (ISLICE == ps_ctxt->i1_slice_type);

    /*Accumalate all the variables in shared memory */
    for(j = 0; j < (MAX_CU_IN_CTB >> 2); j++)
    {
        ps_multi_thrd_ctxt->ai8_nctb_ipe_sad[i4_frm_id][i4_br_id][i4_thrd_id] +=
            ps_ctb_ipe_analyse->ai4_best_sad_8x8_l1_ipe[j];
        ps_multi_thrd_ctxt->ai8_nctb_me_sad[i4_frm_id][i4_br_id][i4_thrd_id] +=
            ps_ctb_ipe_analyse->ai4_best_sad_8x8_l1_me[j];

        ps_multi_thrd_ctxt->ai8_nctb_act_factor[i4_frm_id][i4_br_id][i4_thrd_id] +=
            ps_ctb_ipe_analyse->ai4_8x8_act_factor[j];
    }

    ps_multi_thrd_ctxt->ai8_nctb_l0_ipe_sad[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctb_ipe_analyse->i4_ctb_acc_satd;

    /*Accumalte L0 MPM bits for N CTB*/
    ps_multi_thrd_ctxt->ai8_nctb_mpm_bits_consumed[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctb_ipe_analyse->i4_ctb_acc_mpm_bits;

    /*Accumate the total bits and hdr bits for N Ctbs*/
    ps_multi_thrd_ctxt->ai8_nctb_bits_consumed[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctxt->u4_total_cu_bits;
    ps_multi_thrd_ctxt->ai8_acc_bits_consumed[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctxt->u4_total_cu_bits;
    ps_multi_thrd_ctxt->ai8_acc_bits_mul_qs_consumed[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctxt->u4_total_cu_bits_mul_qs;
    ps_multi_thrd_ctxt->ai8_nctb_hdr_bits_consumed[i4_frm_id][i4_br_id][i4_thrd_id] +=
        ps_ctxt->u4_total_cu_hdr_bits;

    /*Reset the total CU bits, accumalated for all CTBS*/
    ps_ctxt->u4_total_cu_bits = 0;
    ps_ctxt->u4_total_cu_hdr_bits = 0;
    ps_ctxt->u4_total_cu_bits_mul_qs = 0;

    /*Put mutex lock for incrementing cb count */
    osal_mutex_lock(ps_multi_thrd_ctxt->pv_sub_pic_rc_mutex_lock_hdl);

    ps_multi_thrd_ctxt->ai4_acc_ctb_ctr[i4_frm_id][i4_br_id] += 1;
    ps_multi_thrd_ctxt->ai4_ctb_ctr[i4_frm_id][i4_br_id] += 1;

    /*Check if the acc ctb counter across thread has reached the required threshold */
    if(ps_multi_thrd_ctxt->ai4_acc_ctb_ctr[i4_frm_id][i4_br_id] >=
       ps_ctxt->i4_num_ctb_for_out_scale)
    {
        i4_ctb_count_flag = 1;
        /*Reset accumalated CTB counter appropriately s */
        ps_multi_thrd_ctxt->ai4_acc_ctb_ctr[i4_frm_id][i4_br_id] = 0;
    }

    /*Variables to be sent in the queue after required ctb count is reached */
    if(1 == i4_ctb_count_flag)
    {
        WORD32 i4_temp_thrd_id;
        LWORD64 i8_nctb_l1_me_sad = 0, i8_nctb_l1_ipe_sad = 0;
        LWORD64 i8_nctb_l0_ipe_satd = 0, i8_nctb_l1_activity_fact = 0;
        LWORD64 i8_nctb_hdr_bits_consumed = 0, i8_nctb_l0_mpm_bits = 0;
        LWORD64 i8_nctb_bits_consumed = 0, i8_acc_bits_consumed = 0,
                i8_acc_bits_mul_qs_consumed = 0;
        LWORD64 i8_frame_l1_ipe_sad, i8_frame_l0_ipe_satd, i8_frame_l1_me_sad;
        LWORD64 i8_frame_l1_activity_fact, i8_frame_bits_estimated;

        for(i4_temp_thrd_id = 0; i4_temp_thrd_id < ps_ctxt->i4_num_proc_thrds; i4_temp_thrd_id++)
        {
            /*Accumalte only if thread id is valid */
            if(ps_multi_thrd_ctxt->ai4_thrd_id_valid_flag[i4_frm_id][i4_br_id][i4_temp_thrd_id] ==
               1)
            {
                /*store complexities for the ctbs across all threads till then  */
                i8_nctb_l1_me_sad +=
                    ps_multi_thrd_ctxt->ai8_nctb_me_sad[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_nctb_l1_ipe_sad +=
                    ps_multi_thrd_ctxt->ai8_nctb_ipe_sad[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_nctb_l0_ipe_satd +=
                    ps_multi_thrd_ctxt->ai8_nctb_l0_ipe_sad[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_nctb_l1_activity_fact +=
                    ps_multi_thrd_ctxt->ai8_nctb_act_factor[i4_frm_id][i4_br_id][i4_temp_thrd_id];

                /*Set encoder total and hdr bits and mpm bits for that N ctbs */
                i8_nctb_hdr_bits_consumed +=
                    ps_multi_thrd_ctxt
                        ->ai8_nctb_hdr_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_nctb_l0_mpm_bits +=
                    ps_multi_thrd_ctxt
                        ->ai8_nctb_mpm_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_nctb_bits_consumed +=
                    ps_multi_thrd_ctxt->ai8_nctb_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id];

                /*Set encoder total bits for ctbs till then */
                i8_acc_bits_consumed +=
                    ps_multi_thrd_ctxt->ai8_acc_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id];
                i8_acc_bits_mul_qs_consumed +=
                    ps_multi_thrd_ctxt
                        ->ai8_acc_bits_mul_qs_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id];

                /*Reset NCTB total and hdr, mpm  bits counter to zero */
                ps_multi_thrd_ctxt->ai8_nctb_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id] =
                    0;
                ps_multi_thrd_ctxt
                    ->ai8_nctb_hdr_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id] = 0;
                ps_multi_thrd_ctxt
                    ->ai8_nctb_mpm_bits_consumed[i4_frm_id][i4_br_id][i4_temp_thrd_id] = 0;
            }
        }

        /*Store all frame level params */
        i8_frame_l1_ipe_sad = ps_ctxt->i8_frame_l1_ipe_sad;
        i8_frame_l0_ipe_satd = ps_ctxt->i8_frame_l0_ipe_satd;
        i8_frame_l1_me_sad = ps_ctxt->i8_frame_l1_me_sad;
        i8_frame_l1_activity_fact = ps_ctxt->i8_frame_l1_activity_fact;
        i8_frame_bits_estimated = ps_ctxt->ai4_frame_bits_estimated[i4_frm_id][i4_br_id];

        /*If CU level RC is disabled reset the nctb and frame level factor */
        if(0 == ps_ctxt->i4_qp_mod)
        {
            i8_frame_l1_activity_fact = 0;
        }

        ASSERT(ps_ctxt->ai4_frame_bits_estimated[i4_frm_id][i4_br_id] != 0);

        {
            float bits_estimated, activity_ratio = 1;
            WORD32 i8_ctb_bits_estimated;
            float f_bit_deviation;
            WORD32 i4_tot_frame_ctb =
                ps_frm_ctb_prms->i4_num_ctbs_vert * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /*The QP limit can only increment/decrement by 3/1 */
            float f_qp_increase_limit = (1.414);
            //float f_qp_decrease_limit = (0.891);

            /*Frame level activity is set to 0 for cu-level rc off*/
            if(i8_frame_l1_activity_fact != 0)
                activity_ratio =
                    (float)(i8_frame_l1_activity_fact) / (float)(i8_nctb_l1_activity_fact);

            activity_ratio = 1;

            /*Estimate the bits to be consumed based on the intra and inter complexity */
            if(i4_is_intra_pic)
            {
                float sad_ratio = (float)(i8_nctb_l0_ipe_satd) / (float)(i8_frame_l0_ipe_satd);
                bits_estimated = sad_ratio * activity_ratio * ((float)i8_frame_bits_estimated);
            }
            else
            {
                float sad_ratio = (float)(i8_nctb_l1_me_sad) / (float)(i8_frame_l1_me_sad);
                bits_estimated = sad_ratio * activity_ratio * ((float)i8_frame_bits_estimated);
            }

            i8_ctb_bits_estimated = (i8_frame_bits_estimated / i4_tot_frame_ctb);

            /*for better control on both sides*/
            f_bit_deviation = ((i8_acc_bits_consumed * 1.0) / bits_estimated);
            //printf("\n dev = %f\t",f_bit_deviation);
            /* if consumed bits is higher than 7.5% or consumed bits is lower by 20%)*/
            if((f_bit_deviation > 1.075) ||
               ((f_bit_deviation < 0.8) &&
                (ps_ctxt->i4_is_model_valid == 0 ||
                 (ps_multi_thrd_ctxt->ai4_threshold_reached[i4_frm_id][i4_br_id]))))
            {
                float f_qscale_avg_factor;
                WORD32 i4_cu_qp_sub_pic_rc_curr;
                /*get the Qscale of Frame QP*/
                WORD32 i4_frm_qs_q3 =
                    (ps_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale
                         [ps_ctxt->i4_frame_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]);
                WORD32 i4_prev_qp = ps_ctxt->i4_frame_mod_qp;

                ps_multi_thrd_ctxt->ai4_threshold_reached[i4_frm_id][i4_br_id] = 1;

                /*Calculating Intra scale factor */
                if(i4_is_intra_pic)
                {
                    /*In case of lower QP, Qscale increase at every step is very low, which doesn't allow QP increase
                        to meet the rate, hence disable deviation clip below QP 4 for all bitdepth*/
                    if(i4_prev_qp > MIN_QP_NO_CLIP_DEV)
                    {
                        /* Clip the bits deviation such that it never cross +3 qp shifts from average QP so far coded with in-frame rc*/
                        if(f_bit_deviation > f_qp_increase_limit)
                        {
                            f_bit_deviation = f_qp_increase_limit;
                        }
                    }

                    /*The current qscale should do not deviate +/- 3 QP from the previous qscale */
                    f_qscale_avg_factor =
                        (((float)(i8_acc_bits_mul_qs_consumed * (1 << QSCALE_Q_FAC_3))) /
                         (i8_acc_bits_consumed * i4_frm_qs_q3));
                    i4_cu_qp_sub_pic_rc_curr =
                        f_qscale_avg_factor * f_bit_deviation * (1 << QP_LEVEL_MOD_ACT_FACTOR);
                }
                else /*Calculating Inter scale factor */
                {
                    /*In case of lower QP, Qscale increase at every step is very low, which doesn't allow QP increase
                        to meet the rate, hence disable deviation clip below QP 4 for all bitdepth*/
                    if(i4_prev_qp > MIN_QP_NO_CLIP_DEV)
                    {
                        /* Clip the bits deviation such that it never cross +3 qp shifts from average QP so far coded with in-frame rc*/
                        if(f_bit_deviation > f_qp_increase_limit)
                        {
                            f_bit_deviation = f_qp_increase_limit;
                        }
                    }

                    /*The current qscale should do not deviate +/- 3 QP from the previous qscale */
                    f_qscale_avg_factor =
                        (((float)(i8_acc_bits_mul_qs_consumed * (1 << QSCALE_Q_FAC_3))) /
                         (i8_acc_bits_consumed * i4_frm_qs_q3));
                    i4_cu_qp_sub_pic_rc_curr =
                        f_qscale_avg_factor * f_bit_deviation * (1 << QP_LEVEL_MOD_ACT_FACTOR);
                }
                //printf("Avg_qscale = %f\t qs_inq3 = %d",f_qscale_avg_factor,i4_frm_qs_q3);
                /*update of previous chunk QP in multi-thread context, so that all threads can use it from now onwards*/
                {
                    ps_multi_thrd_ctxt->ai4_prev_chunk_qp[i4_frm_id][i4_br_id] =
                        ps_ctxt->i4_frame_mod_qp;
                }
                /*Limit the qp from decreasing less than 6 compared to frame qp */
                {
                    osal_mutex_lock(ps_multi_thrd_ctxt->pv_sub_pic_rc_for_qp_update_mutex_lock_hdl);
                    ps_multi_thrd_ctxt->ai4_curr_qp_estimated[i4_frm_id][i4_br_id] =
                        i4_cu_qp_sub_pic_rc_curr;
                    osal_mutex_unlock(
                        ps_multi_thrd_ctxt->pv_sub_pic_rc_for_qp_update_mutex_lock_hdl);
                }
            }
        }
    }
    osal_mutex_unlock(ps_multi_thrd_ctxt->pv_sub_pic_rc_mutex_lock_hdl);
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_sub_pic_rc_qp_query \endif
*
* \brief
*   Sub-pic RC thread interface function
*
* \param[in] Frame process pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_sub_pic_rc_scale_query(void *pv_multi_thrd_ctxt, void *pv_ctxt)
{
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;
    ihevce_enc_loop_ctxt_t *ps_ctxt = (ihevce_enc_loop_ctxt_t *)pv_ctxt;
    WORD32 i4_mod_qp, i4_prev_qs;
    WORD32 i4_previous_chunk_qp;

    WORD32 i4_qp_delata_max_limit, i4_qp_delata_min_limit;

    osal_mutex_lock(ps_multi_thrd_ctxt->pv_sub_pic_rc_for_qp_update_mutex_lock_hdl);

    i4_mod_qp =
        (ps_ctxt->ps_rc_quant_ctxt
             ->pi4_qp_to_qscale[ps_ctxt->i4_frame_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]);
    i4_previous_chunk_qp =
        ps_multi_thrd_ctxt
            ->ai4_prev_chunk_qp[ps_ctxt->i4_enc_frm_id][ps_ctxt->i4_bitrate_instance_num];
    i4_prev_qs =
        (ps_ctxt->ps_rc_quant_ctxt
             ->pi4_qp_to_qscale[i4_previous_chunk_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]);
    /*Limit the qp_delta_scale if it exceeds the limit of QP51 and QP 1 */

    i4_qp_delata_max_limit =
        ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale * (1 << QP_LEVEL_MOD_ACT_FACTOR);
    i4_qp_delata_max_limit = i4_qp_delata_max_limit / i4_mod_qp;

    i4_qp_delata_min_limit =
        ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale * (1 << QP_LEVEL_MOD_ACT_FACTOR);
    i4_qp_delata_min_limit = i4_qp_delata_min_limit / i4_mod_qp;
    {
        /*For Non-I SCD and Frames after SCD*/
        /*The scale is tweeked to only increase qp (increased by 6) if the bits consumed is higher than bits
        estimated */
        ps_ctxt->i4_cu_qp_sub_pic_rc =
            ps_multi_thrd_ctxt
                ->ai4_curr_qp_estimated[ps_ctxt->i4_enc_frm_id][ps_ctxt->i4_bitrate_instance_num];
        /*Limit the Qscale */
        if(ps_ctxt->i4_cu_qp_sub_pic_rc > i4_qp_delata_max_limit)
        {
            ps_ctxt->i4_cu_qp_sub_pic_rc = i4_qp_delata_max_limit;
        }
        else if(ps_ctxt->i4_cu_qp_sub_pic_rc < i4_qp_delata_min_limit)
        {
            ps_ctxt->i4_cu_qp_sub_pic_rc = i4_qp_delata_min_limit;
        }

        ps_multi_thrd_ctxt
            ->ai4_curr_qp_estimated[ps_ctxt->i4_enc_frm_id][ps_ctxt->i4_bitrate_instance_num] =
            ps_ctxt->i4_cu_qp_sub_pic_rc;
    }

    /*Accumalate the CTB level QP here and feed to rc as average qp*/
    {
        WORD32 i4_mod_cur_qp, i4_mod_prev_qp;

        i4_mod_cur_qp =
            ((i4_mod_qp * ps_ctxt->i4_cu_qp_sub_pic_rc) + (1 << (QP_LEVEL_MOD_ACT_FACTOR - 1))) >>
            QP_LEVEL_MOD_ACT_FACTOR;

        /*Limit the qscale and qp */
        if(i4_mod_cur_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale)
        {
            i4_mod_cur_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale;
            ASSERT(0);
        }
        else if(i4_mod_cur_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale)
        {
            i4_mod_cur_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale;
            ASSERT(0);
        }

        i4_mod_cur_qp = ps_ctxt->ps_rc_quant_ctxt->pi4_qscale_to_qp[i4_mod_cur_qp];
        /*limit the prev qs*/
        if(i4_prev_qs > ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale)
        {
            i4_prev_qs = ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale;
        }
        else if(i4_prev_qs < ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale)
        {
            i4_prev_qs = ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale;
        }

        i4_mod_prev_qp = ps_ctxt->ps_rc_quant_ctxt->pi4_qscale_to_qp[i4_prev_qs];

        /*cur qp < prev qp, then allow only -1*/
        if(i4_mod_cur_qp < i4_mod_prev_qp)
        {
            i4_mod_cur_qp = i4_mod_prev_qp - 1;
            if(i4_mod_cur_qp < (ps_ctxt->i4_frame_qp - 6))
            {
                i4_mod_cur_qp = (ps_ctxt->i4_frame_qp - 6);
            }
        }

        /*In case of lower QP, Qscale increase at every step is very low, which doesn't allow QP increase
        to meet the rate, hence disable deviation clip below QP 4 for all bitdepth*/
        if(i4_mod_cur_qp > i4_mod_prev_qp)
        {
            i4_mod_cur_qp = MIN(i4_mod_prev_qp + 3, i4_mod_cur_qp);
        }

        /* CLIP to maintain Qp between user configured and min and max Qp values*/
        if(i4_mod_cur_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qp)
            i4_mod_cur_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;
        else if(i4_mod_cur_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qp)
            i4_mod_cur_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qp;

        /*Modify the qp based on delta*/
        ps_ctxt->i4_frame_mod_qp = i4_mod_cur_qp;
        ps_ctxt->i4_is_first_query = 0;
        if(ps_ctxt->i4_frame_mod_qp != ps_ctxt->i4_frame_qp)
        {
            ps_ctxt->i4_is_ctb_qp_modified = 1;
        }
    }

    ps_multi_thrd_ctxt->ai4_curr_qp_acc[ps_ctxt->i4_enc_frm_id][ps_ctxt->i4_bitrate_instance_num] +=
        ps_ctxt->i4_frame_mod_qp;

    osal_mutex_unlock(ps_multi_thrd_ctxt->pv_sub_pic_rc_for_qp_update_mutex_lock_hdl);

    return;
}
