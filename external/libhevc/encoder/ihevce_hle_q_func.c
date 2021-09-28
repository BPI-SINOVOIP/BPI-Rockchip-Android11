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
* \file ihevce_hle_que_func.c
*
* \brief
*    This file contains Que finction of Hehg level encoder
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* List of Functions
*    <TODO: TO BE ADDED>
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
#include "ihevc_macros.h"
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
#include "ihevc_trans_tables.h"
#include "ihevc_trans_macros.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_buffer_que_interface.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_error_checks.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_q_get_free_buff \endif
*
* \brief
*    Gets a free buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] Que id of the buffer
* \param[in] pointer to return the buffer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_free_buff(
    void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode)
{
    /* local varaibles */
    WORD32 end_flag = 0;
    void *pv_buff = NULL;
    WORD32 i4_mres_single_out;
    enc_ctxt_t *ps_enc_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    i4_mres_single_out = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out;

    while(1 != end_flag)
    {
        /* acquire mutex lock */
        osal_mutex_lock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

        /* call the buffer api function */
        pv_buff =
            ihevce_buff_que_get_free_buf(ps_enc_ctxt->s_enc_ques.apv_q_hdl[i4_q_id], pi4_buff_id);

        /* release mutex lock */
        osal_mutex_unlock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

        /* if no free buffer is available */
        if(NULL == pv_buff)
        {
            /* check if the mode is blocking */
            if(BUFF_QUE_BLOCKING_MODE == i4_blocking_mode)
            {
                /* ------------------------------------------------- */
                /* Get free buffers are called by producers          */
                /* these producers threads will be put in pend state */
                /* ------------------------------------------------- */

                /* choose the semaphore based on Que Id */
                void *pv_sem_handle = NULL;

                /* input data Que : application's input data processing */
                /* thread is put to pend state                          */
                if(IHEVCE_INPUT_DATA_CTRL_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_data_sem_handle;
                }

                /* input ctrl Que : application's input ctrl processing */
                /* thread is put to pend state                          */
                if(IHEVCE_INPUT_ASYNCH_CTRL_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_ctrl_sem_handle;
                }

                if(IHEVCE_ENC_INPUT_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_inp_data_sem_hdl;
                }

                /* Output data Que : Output thread is put to pend state */
                if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
                {
                    if(1 == i4_mres_single_out)
                    {
                        pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_common_mres_sem_hdl;
                    }
                    else
                    {
                        pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_strm_sem_handle[0];
                    }
                }
                /* Recon data Que : Recon thread is put to pend state */
                if(IHEVCE_RECON_DATA_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_recon_sem_handle[0];
                }
                /* frm prs ent cod data Que : frame process is put to pend state */
                if(IHEVCE_FRM_PRS_ENT_COD_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                /* Pre encode/ encode data Que : pre enocde is put to pend state */
                if(IHEVCE_PRE_ENC_ME_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle;
                }
                /* ME/ENC Que : enc frame proc is put to pend state */
                if(IHEVCE_ME_ENC_RDOPT_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                if(IHEVCE_L0_IPE_ENC_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle;
                }
                /* output status queue should be used by both LAP and Frame
                   process in non blocking mode */
                if(IHEVCE_OUTPUT_STATUS_Q == i4_q_id)
                {
                    ASSERT(0);
                }

                /* go the pend state */
                osal_sem_wait(pv_sem_handle);
            }
            /* if non blocking then return NULL and break from loop */
            else
            {
                end_flag = 1;
            }
        }
        /* if valid free buffer is available then break from loop */
        else
        {
            end_flag = 1;
        }
    }

    return (pv_buff);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_set_buff_prod \endif
*
* \brief
*    Sets the buffer as produced in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] Que id of the buffer
* \param[in] buffer id which needs to be set as produced
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_set_buff_prod(void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 i4_buff_id)
{
    /* local varaibles */

    WORD32 i4_num_users = 0;
    WORD32 i4_mres_single_out;
    enc_ctxt_t *ps_enc_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    i4_mres_single_out = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out;

    /* acquire mutex lock */
    osal_mutex_lock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

    /* call the buffer api function */
    ihevce_buff_que_set_buf_prod(
        ps_enc_ctxt->s_enc_ques.apv_q_hdl[i4_q_id], i4_buff_id, i4_num_users);

    /* release mutex lock */
    osal_mutex_unlock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

    /* ------------------------------------------------------------- */
    /* after setting the buffer the consumers thread needs to be     */
    /* posted in case if that thread is in wait state                */
    /* currently this post is done unconditionally                   */
    /* ------------------------------------------------------------- */

    /* input command que : LAP & Frame process threads needs to posted */
    if(IHEVCE_INPUT_ASYNCH_CTRL_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle);
    }

    /* input data que : LAP thread needs to posted */
    if(IHEVCE_INPUT_DATA_CTRL_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle);
    }

    /* output stream data que :  Entropy processing thread needs to posted */
    if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
    {
        WORD32 i4_entropy_thrd_id;
        WORD32 i4_bufque_id;

        i4_bufque_id = (i4_q_id - IHEVCE_OUTPUT_DATA_Q);
        i4_entropy_thrd_id = i4_bufque_id;

        if(i4_bufque_id == 0)
        {
            i4_entropy_thrd_id = ps_enc_ctxt->i4_ref_mbr_id;
        }
        else if(i4_bufque_id == ps_enc_ctxt->i4_ref_mbr_id)
        {
            i4_entropy_thrd_id = 0;
        }

        if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
        {
            if(1 == i4_mres_single_out)
            {
                osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_ent_common_mres_sem_hdl);
            }
            else
            {
                osal_sem_post(
                    ps_enc_ctxt->s_thrd_sem_ctxt.apv_ent_cod_sem_handle[i4_entropy_thrd_id]);
            }
        }
    }

    /* output recon data que :  app's output data processing thread needs to posted */
    if(IHEVCE_RECON_DATA_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }
    /* output control que :  app's output processing thread needs to posted */
    if(IHEVCE_OUTPUT_STATUS_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_ctrl_sem_handle);
    }

    /* frm process entropy que :  entropy thread needs to posted */
    if(IHEVCE_FRM_PRS_ENT_COD_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.apv_ent_cod_sem_handle[0]);
    }
    /* pre-encode/encode que :  encode frame proc thread needs to posted */
    if(IHEVCE_PRE_ENC_ME_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }
    /* ME/ENC Que : enc frame proc needs to be posted */
    if(IHEVCE_ME_ENC_RDOPT_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }
    if(IHEVCE_L0_IPE_ENC_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }

    if(IHEVCE_ENC_INPUT_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_preenc_inp_data_sem_hdl);
    }

    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_get_filled_buff \endif
*
* \brief
*    Gets a next filled buffer from the que requested
*
* \param[in] high level encoder context pointer
* \param[in] Que id of the buffer
* \param[in] pointer to return the buffer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_q_get_filled_buff(
    void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 *pi4_buff_id, WORD32 i4_blocking_mode)
{
    /* local varaibles */
    WORD32 end_flag = 0;
    void *pv_buff = NULL;
    WORD32 i4_mres_single_out;
    enc_ctxt_t *ps_enc_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    i4_mres_single_out = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out;

    while(1 != end_flag)
    {
        /* acquire mutex lock */
        osal_mutex_lock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

        /* call the buffer api function */
        pv_buff =
            ihevce_buff_que_get_next_buf(ps_enc_ctxt->s_enc_ques.apv_q_hdl[i4_q_id], pi4_buff_id);

        /* release mutex lock */
        osal_mutex_unlock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

        /* if no free buffer is available */
        if(NULL == pv_buff)
        {
            /* check if the mode is blocking */
            if(BUFF_QUE_BLOCKING_MODE == i4_blocking_mode)
            {
                /* ------------------------------------------------- */
                /* Get filled buffers are called by consumers        */
                /* these consumer threads will be put in pend state */
                /* ------------------------------------------------- */

                /* choose the semaphore based on Que Id */
                void *pv_sem_handle = NULL;

                /* input data Que : LAP thread is put to pend state */
                if(IHEVCE_INPUT_DATA_CTRL_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle;
                }

                /* input ctrl Que : LAP thread is put to pend state */
                if(IHEVCE_INPUT_ASYNCH_CTRL_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle;
                }

                /* Output Stream data Que : Entropy processing */
                /* thread is put to pend state                              */
                if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
                {
                    WORD32 i4_entropy_thrd_id;
                    WORD32 i4_bufque_id;

                    i4_bufque_id = (i4_q_id - IHEVCE_OUTPUT_DATA_Q);
                    i4_entropy_thrd_id = i4_bufque_id;

                    if(i4_bufque_id == 0)
                    {
                        i4_entropy_thrd_id = ps_enc_ctxt->i4_ref_mbr_id;
                    }
                    else if(i4_bufque_id == ps_enc_ctxt->i4_ref_mbr_id)
                    {
                        i4_entropy_thrd_id = 0;
                    }

                    if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
                    {
                        if(1 == i4_mres_single_out)
                        {
                            pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_ent_common_mres_sem_hdl;
                        }
                        else
                        {
                            pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt
                                                .apv_ent_cod_sem_handle[i4_entropy_thrd_id];
                        }
                    }
                }

                /* Output Recon data Que : Frame processing */
                /* thread is put to pend state                       */
                if(IHEVCE_RECON_DATA_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                /* frm prs ent cod data Que : entropy thread is put to pend state */
                if(IHEVCE_FRM_PRS_ENT_COD_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.apv_ent_cod_sem_handle[0];
                }
                /* Output status Que : application's output processing */
                /* thread is put to pend state                         */
                if(IHEVCE_OUTPUT_STATUS_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_ctrl_sem_handle;
                }

                /* pre-encode/encode Que : encode frame proc thread  */
                if(IHEVCE_PRE_ENC_ME_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                /* ME/ENC Que : enc frame proc is put to pend state */
                if(IHEVCE_ME_ENC_RDOPT_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                if(IHEVCE_L0_IPE_ENC_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle;
                }
                /* This call will be made from pre-enc enc thread, hence when input is not available the caller thread should go to pend */
                if(IHEVCE_ENC_INPUT_Q == i4_q_id)
                {
                    pv_sem_handle = ps_enc_ctxt->s_thrd_sem_ctxt.pv_preenc_inp_data_sem_hdl;
                }

                /* go the pend state */
                osal_sem_wait(pv_sem_handle);
            }
            /* if non blocking then return NULL and break from loop */
            else
            {
                end_flag = 1;
            }
        }
        /* if valid filled buffer is available then break from loop */
        else
        {
            end_flag = 1;
        }
    }

    return (pv_buff);
}

/*!
******************************************************************************
* \if Function name : ihevce_q_rel_buf \endif
*
* \brief
*    Frees the buffer as in the que requested
*
* \param[in] high level encoder context pointer
* \param[in] Que id of the buffer
* \param[in] buffer id which needs to be freed
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
IV_API_CALL_STATUS_T ihevce_q_rel_buf(void *pv_enc_ctxt, WORD32 i4_q_id, WORD32 i4_buff_id)
{
    /* local varaibles */
    WORD32 i4_mres_single_out;
    enc_ctxt_t *ps_enc_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    i4_mres_single_out = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_mres_single_out;
    /* acquire mutex lock */
    osal_mutex_lock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

    /* call the buffer api function */
    ihevce_buff_que_rel_buf(ps_enc_ctxt->s_enc_ques.apv_q_hdl[i4_q_id], i4_buff_id);

    /* release mutex lock */
    osal_mutex_unlock(ps_enc_ctxt->s_enc_ques.pv_q_mutex_hdl);

    /* ------------------------------------------------------------- */
    /* after releasing the buffer the producer thread needs to be    */
    /* posted in case if that thread is in wait state                */
    /* currently this post is done unconditionally                   */
    /* ------------------------------------------------------------- */

    /* input data que :  app's input data producing thread needs to posted */
    if(IHEVCE_INPUT_DATA_CTRL_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_data_sem_handle);
    }

    /* input data control que :  app's command que producing thread needs to posted */
    if(IHEVCE_INPUT_ASYNCH_CTRL_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_inp_ctrl_sem_handle);
    }
    /*multiple input queue*/
    if(IHEVCE_ENC_INPUT_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_inp_data_sem_hdl);
    }

    /* output data que: Output thread needs to posted */
    if(IHEVCE_OUTPUT_DATA_Q == i4_q_id)
    {
        if(1 == i4_mres_single_out)
        {
            osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_out_common_mres_sem_hdl);
        }
        else
        {
            osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_strm_sem_handle[0]);
        }
    }
    /* Recon data que: Recon thread needs to posted */
    if(IHEVCE_RECON_DATA_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.apv_out_recon_sem_handle[0]);
    }
    /* output status que: LAP & Frame process threads needs to posted */
    if(IHEVCE_OUTPUT_STATUS_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_lap_sem_handle);
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle);
    }

    /* frm process entropy que :  Frame process needs to posted */
    if(IHEVCE_FRM_PRS_ENT_COD_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }
    /* pre-encode/encode Que : pre-encode frame proc needs to be posted */
    if(IHEVCE_PRE_ENC_ME_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle);
    }
    /* ME/ENC Que : enc frame proc needs to be posted */
    if(IHEVCE_ME_ENC_RDOPT_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_enc_frm_proc_sem_handle);
    }
    if(IHEVCE_L0_IPE_ENC_Q == i4_q_id)
    {
        osal_sem_post(ps_enc_ctxt->s_thrd_sem_ctxt.pv_pre_enc_frm_proc_sem_handle);
    }
    return (IV_SUCCESS);
}

/*!
******************************************************************************
* \if Function name : ihevce_force_end \endif
*
* \brief
*    Sets force end flag in enc_ctxt for all resolutions
*
* \param[in] high level encoder context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_force_end(ihevce_hle_ctxt_t *ps_hle_ctxt)
{
    enc_ctxt_t *ps_enc_ctxt;
    WORD32 i4_resolution_id = 0;
    WORD32 i4_num_res_layers = 0;
    ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[0];

    i4_num_res_layers = ps_enc_ctxt->ps_stat_prms->s_tgt_lyr_prms.i4_num_res_layers;
    for(i4_resolution_id = 0; i4_resolution_id < i4_num_res_layers; i4_resolution_id++)
    {
        ps_enc_ctxt = (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[i4_resolution_id];
        ps_enc_ctxt->s_multi_thrd.i4_force_end_flag = 1;
    }
}
