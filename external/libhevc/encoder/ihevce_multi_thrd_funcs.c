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
/**
*******************************************************************************
* @file
*  ihevce_multi_thread_funcs.c
*
* @brief
*  Contains functions related to Job Ques and others, required for multi threading
*
* @author
*  Ittiam
*
* @par List of Functions:
*  <TODO: TO BE ADDED>
*
* @remarks
*  None
*
*******************************************************************************
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
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_entropy_structs.h"
#include "ihevce_cmn_utils_instr_set_router.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_global_tables.h"
#include "ihevce_dep_mngr_interface.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_ipe_structs.h"
#include "ihevce_coarse_me_pass.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/********************************************************************/
/*Macros                                                            */
/********************************************************************/
#define MULT_FACT 100

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief Function Pops out the next Job in the appropriate Job Que
*
* @par Description: Does under mutex lock to ensure thread safe
*
* @param[inout] pv_multi_thrd_ctxt
*  Pointer to Multi thread context
*
* @param[in] i4_job_type
*   Job type from which a job needs to be popped out
*
* @param[in] i4_blocking_mode
*   Mode of operation
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void *ihevce_pre_enc_grp_get_next_job(
    void *pv_multi_thrd_ctxt, WORD32 i4_job_type, WORD32 i4_blocking_mode, WORD32 i4_ping_pong)
{
    /* Local variables */
    multi_thrd_ctxt_t *ps_multi_thrd;
    job_queue_handle_t *ps_job_queue_hdl;
    void *pv_next = NULL;
    UWORD8 au1_in_dep_cmp[MAX_IN_DEP] = { 0 };
    void *pv_job_q_mutex_hdl_pre_enc = NULL;

    /* Derive local variables */
    ps_multi_thrd = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;
    ps_job_queue_hdl =
        (job_queue_handle_t *)&ps_multi_thrd->as_job_que_preenc_hdls[i4_ping_pong][i4_job_type];

    /* lock the mutex for Q access */
    /* As design must facilitate for parallelism in each stage,
    It is recommended to have seperate mutex for each stage*/
    if(i4_job_type < ME_JOB_LYR4)
    {
        pv_job_q_mutex_hdl_pre_enc = ps_multi_thrd->pv_job_q_mutex_hdl_pre_enc_decomp;
    }
    else if(i4_job_type < IPE_JOB_LYR0)
    {
        pv_job_q_mutex_hdl_pre_enc = ps_multi_thrd->pv_job_q_mutex_hdl_pre_enc_hme;
    }
    else
    {
        pv_job_q_mutex_hdl_pre_enc = ps_multi_thrd->pv_job_q_mutex_hdl_pre_enc_l0ipe;
    }

    osal_mutex_lock(pv_job_q_mutex_hdl_pre_enc);
    /* Get the next */
    pv_next = ps_job_queue_hdl->pv_next;

    /* Update the next by checking input dependency */
    if(NULL != pv_next)
    {
        job_queue_t *ps_job_queue = (job_queue_t *)pv_next;

        /* check for input dependencies to be resolved            */
        /* this can be blocking or non blocking based on use case */
        /* if non blocking then the function returns NULL         */

        if(1 == i4_blocking_mode)
        {
            volatile WORD32 mem_diff;
            volatile UWORD8 *pu1_ref_buf = &au1_in_dep_cmp[0];
            volatile UWORD8 *pu1_curr_buf = &ps_job_queue->au1_in_dep[0];

            mem_diff = memcmp((void *)pu1_ref_buf, (void *)pu1_curr_buf, MAX_IN_DEP);

            /* wait until all dependency is resolved */
            while(0 != mem_diff)
            {
                mem_diff = memcmp((void *)pu1_ref_buf, (void *)pu1_curr_buf, MAX_IN_DEP);
            }

            /* update the next job in the queue */
            ps_job_queue_hdl->pv_next = ps_job_queue->pv_next;
        }
        else
        {
            /* check for input dependency resolved */
            if((0 != memcmp(&au1_in_dep_cmp[0], &ps_job_queue->au1_in_dep[0], MAX_IN_DEP)))
            {
                /* return null */
                pv_next = NULL;
            }
            else
            {
                /* update the next job in the queue */
                ps_job_queue_hdl->pv_next = ps_job_queue->pv_next;
            }
        }
    }

    /* unlock the mutex */
    osal_mutex_unlock(pv_job_q_mutex_hdl_pre_enc);

    /* Return */
    return (pv_next);

} /* End of get_next_job */

/**
*******************************************************************************
*
* @brief Function Pops out the next Job in the appropriate Job Que
*
* @par Description: Does under mutex lock to ensure thread safe
*
* @param[inout] pv_multi_thrd_ctxt
*  Pointer to Multi thread context
*
* @param[in] i4_job_type
*   Job type from which a job needs to be popped out
*
* @param[in] i4_blocking_mode
*   Mode of operation
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void *ihevce_enc_grp_get_next_job(
    void *pv_multi_thrd_ctxt, WORD32 i4_job_type, WORD32 i4_blocking_mode, WORD32 i4_curr_frm_id)
{
    /* Local variables */
    multi_thrd_ctxt_t *ps_multi_thrd;
    job_queue_handle_t *ps_job_queue_hdl;
    void *pv_next = NULL;
    void *pv_job_q_mutex_hdl_enc_grp;
    UWORD8 au1_in_dep_cmp[MAX_IN_DEP] = { 0 };

    /* Derive local variables */
    ps_multi_thrd = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;

    if(ME_JOB_ENC_LYR == i4_job_type)
    {
        pv_job_q_mutex_hdl_enc_grp = ps_multi_thrd->pv_job_q_mutex_hdl_enc_grp_me;

        ps_job_queue_hdl = (job_queue_handle_t *)&ps_multi_thrd->aps_cur_out_me_prms[i4_curr_frm_id]
                               ->as_job_que_enc_hdls[i4_job_type];
    }
    else
    {
        pv_job_q_mutex_hdl_enc_grp = ps_multi_thrd->pv_job_q_mutex_hdl_enc_grp_enc_loop;
        ps_job_queue_hdl =
            (job_queue_handle_t *)&ps_multi_thrd->aps_cur_inp_enc_prms[i4_curr_frm_id]
                ->as_job_que_enc_hdls[i4_job_type];
    }

    /* lock the mutex for Q access */
    osal_mutex_lock(pv_job_q_mutex_hdl_enc_grp);

    /* Get the next */
    pv_next = ps_job_queue_hdl->pv_next;

    /* Update the next by checking input dependency */
    if(NULL != pv_next)
    {
        job_queue_t *ps_job_queue = (job_queue_t *)pv_next;

        /* check for input dependencies to be resolved            */
        /* this can be blocking or non blocking based on use case */
        /* if non blocking then the function returns NULL         */

        if(1 == i4_blocking_mode)
        {
            volatile WORD32 mem_diff;
            volatile UWORD8 *pu1_ref_buf = &au1_in_dep_cmp[0];
            volatile UWORD8 *pu1_curr_buf = &ps_job_queue->au1_in_dep[0];

            mem_diff = memcmp((void *)pu1_ref_buf, (void *)pu1_curr_buf, MAX_IN_DEP);

            /* wait until all dependency is resolved */
            while(0 != mem_diff)
            {
                mem_diff = memcmp((void *)pu1_ref_buf, (void *)pu1_curr_buf, MAX_IN_DEP);
            }

            /* update the next job in the queue */
            ps_job_queue_hdl->pv_next = ps_job_queue->pv_next;
        }
        else
        {
            /* check for input dependency resolved */
            if((0 != memcmp(&au1_in_dep_cmp[0], &ps_job_queue->au1_in_dep[0], MAX_IN_DEP)))
            {
                /* return null */
                pv_next = NULL;
            }
            else
            {
                /* update the next job in the queue */
                ps_job_queue_hdl->pv_next = ps_job_queue->pv_next;
            }
        }
    }

    /* unlock the mutex */
    osal_mutex_unlock(pv_job_q_mutex_hdl_enc_grp);

    /* Return */
    return (pv_next);

} /* End of get_next_job */

/**
*******************************************************************************
*
* @brief Set the output dependency to done state
*
* @par Description: same as brief
*
* @param[inout] pv_multi_thrd_ctxt
*  Pointer to Multi thread context
*
* @param[in] ps_curr_job
*  Current finished Job pointer
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void ihevce_pre_enc_grp_job_set_out_dep(
    void *pv_multi_thrd_ctxt, job_queue_t *ps_curr_job, WORD32 i4_ping_pong)
{
    /* local vareiables */
    WORD32 ctr;
    multi_thrd_ctxt_t *ps_multi_thrd;

    ps_multi_thrd = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;

    /* loop over number output dependencies */
    for(ctr = 0; ctr < ps_curr_job->i4_num_output_dep; ctr++)
    {
        UWORD8 *pu1_ptr;

        pu1_ptr = (UWORD8 *)ps_multi_thrd->aps_job_q_pre_enc[i4_ping_pong];
        pu1_ptr += ps_curr_job->au4_out_ofsts[ctr];
        *pu1_ptr = 0;
    }

    return;
}

/**
*******************************************************************************
*
* @brief Set the output dependency to done state
*
* @par Description: same as brief
*
* @param[inout] pv_multi_thrd_ctxt
*  Pointer to Multi thread context
*
* @param[in] ps_curr_job
*   Current finished Job pointer
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void ihevce_enc_grp_job_set_out_dep(
    void *pv_multi_thrd_ctxt, job_queue_t *ps_curr_job, WORD32 i4_curr_frm_id)
{
    /* local vareiables */
    WORD32 ctr;
    UWORD8 *pu1_ptr;
    multi_thrd_ctxt_t *ps_multi_thrd;

    ps_multi_thrd = (multi_thrd_ctxt_t *)pv_multi_thrd_ctxt;

    if(ME_JOB_ENC_LYR == ps_curr_job->i4_task_type)
    {
        pu1_ptr = (UWORD8 *)ps_multi_thrd->aps_cur_out_me_prms[i4_curr_frm_id]->ps_job_q_enc;
    }
    else
    {
        pu1_ptr = (UWORD8 *)ps_multi_thrd->aps_cur_inp_enc_prms[i4_curr_frm_id]->ps_job_q_enc;
    }

    /* loop over number output dependencies */
    for(ctr = 0; ctr < ps_curr_job->i4_num_output_dep; ctr++)
    {
        WORD32 i4_off;
        i4_off = ps_curr_job->au4_out_ofsts[ctr];
        pu1_ptr[i4_off] = 0;
    }

    return;
}

/**
*******************************************************************************
*
* @brief Function prepares the Job Queues for all the passes of encoder
*
* @par Description: Based on picture type sets the input and output dependency
*
* @param[inout] pv_enc_ctxt
*  Pointer to encoder context
*
* @param[in] ps_curr_inp
*  Current Input buffer pointer
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void ihevce_prepare_job_queue(
    void *pv_enc_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp, WORD32 i4_curr_frm_id)
{
    /* local variables */
    enc_ctxt_t *ps_ctxt;
    job_queue_t *ps_me_job_queue_lyr0;
    job_queue_t *ps_enc_loop_job_queue;
    WORD32 pass;
    WORD32 num_jobs, col_tile_ctr;
    WORD32 num_ctb_vert_rows;
    WORD32 i4_pic_type;
    WORD32 i;  //counter for bitrate
    WORD32 i4_num_bitrate_instances;
    WORD32 i4_num_tile_col;

    /* derive local varaibles */
    ps_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    num_ctb_vert_rows = ps_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert;
    i4_num_bitrate_instances = ps_ctxt->i4_num_bitrates;

    i4_num_tile_col = 1;
    if(1 == ps_ctxt->ps_tile_params_base->i4_tiles_enabled_flag)
    {
        i4_num_tile_col = ps_ctxt->ps_tile_params_base->i4_num_tile_cols;
    }
    /* memset the entire job que buffer to zero */
    memset(
        ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]->ps_job_q_enc,
        0,
        MAX_NUM_VERT_UNITS_FRM * NUM_ENC_JOBS_QUES * i4_num_tile_col * sizeof(job_queue_t));

    /* get the start address of  Job queues */
    ps_me_job_queue_lyr0 = ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]->ps_job_q_enc;
    ps_enc_loop_job_queue = ps_me_job_queue_lyr0 + (i4_num_tile_col * MAX_NUM_VERT_UNITS_FRM);

    /* store the JOB queue in the Job handle */
    ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
        ->as_job_que_enc_hdls[ME_JOB_ENC_LYR]
        .pv_next = (void *)ps_me_job_queue_lyr0;
    /* store the JOB queue in the Job handle for reenc */
    ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
        ->as_job_que_enc_hdls_reenc[ME_JOB_ENC_LYR]
        .pv_next = (void *)ps_me_job_queue_lyr0;

    for(i = 0; i < i4_num_bitrate_instances; i++)
    {
        ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
            ->as_job_que_enc_hdls[ENC_LOOP_JOB + i]
            .pv_next = (void *)ps_enc_loop_job_queue;
        ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
            ->as_job_que_enc_hdls_reenc[ENC_LOOP_JOB + i]
            .pv_next = (void *)ps_enc_loop_job_queue;
        ps_enc_loop_job_queue += (i4_num_tile_col * MAX_NUM_VERT_UNITS_FRM);
    }

    i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;

    //prepare ME JOB queue first
    //for(pass = 0; pass < NUM_ENC_JOBS_QUES; pass++)
    {
        job_queue_t *ps_job_queue_curr;
        job_queue_t *ps_job_queue_next;
        WORD32 ctr;
        WORD32 inp_dep;
        WORD32 out_dep;
        WORD32 num_vert_units;
        HEVCE_ENC_JOB_TYPES_T task_type;

        pass = 0;  //= ENC_LOOP_JOB

        {
            /* num_ver_units of finest layer is stored at (num_hme_lyrs - 1)th index */
            num_vert_units = num_ctb_vert_rows;
            task_type = ME_JOB_ENC_LYR;
            ps_job_queue_curr = ps_me_job_queue_lyr0;
            ps_job_queue_next =
                (job_queue_t *)ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
                    ->as_job_que_enc_hdls[ENC_LOOP_JOB]
                    .pv_next;
            inp_dep = 0;
            out_dep = 1;  //set reference bit-rate's input dependency
        }

        if((ME_JOB_ENC_LYR == pass) &&
           ((IV_I_FRAME == i4_pic_type) || (IV_IDR_FRAME == i4_pic_type)) && !L0ME_IN_OPENLOOP_MODE)
        {
            //continue;
        }
        else
        {
            /* loop over all the vertical rows */
            for(num_jobs = 0; num_jobs < num_vert_units; num_jobs++)
            {
                /* loop over all the column tiles */
                for(col_tile_ctr = 0; col_tile_ctr < i4_num_tile_col; col_tile_ctr++)
                {
                    ULWORD64 u8_temp;

                    {
                        ps_job_queue_curr->s_job_info.s_me_job_info.i4_vert_unit_row_no = num_jobs;
                        ps_job_queue_curr->s_job_info.s_me_job_info.i4_tile_col_idx = col_tile_ctr;
                    }

                    ps_job_queue_curr->pv_next = (void *)(ps_job_queue_curr + 1);

                    ps_job_queue_curr->i4_task_type = task_type;

                    ps_job_queue_curr->i4_num_input_dep = inp_dep;

                    /* set the entire input dep buffer to default value 0 */
                    memset(&ps_job_queue_curr->au1_in_dep[0], 0, sizeof(UWORD8) * MAX_IN_DEP);

                    /* set the input dep buffer to 1 for num inp dep */
                    if(0 != inp_dep)
                    {
                        memset(&ps_job_queue_curr->au1_in_dep[0], 1, sizeof(UWORD8) * inp_dep);
                    }

                    ps_job_queue_curr->i4_num_output_dep = out_dep;

                    /* set the entire offset buffer to default value */
                    memset(
                        &ps_job_queue_curr->au4_out_ofsts[0], 0xFF, sizeof(UWORD32) * MAX_OUT_DEP);

                    for(ctr = 0; ctr < out_dep; ctr++)
                    {
                        /* col tile level dependency b/w ME & EncLoop */
                        u8_temp = (ULWORD64)(
                            &ps_job_queue_next[num_jobs * i4_num_tile_col + col_tile_ctr] -
                            ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]->ps_job_q_enc);

                        u8_temp *= sizeof(job_queue_t);

                        /* store the offset to the array */
                        ps_job_queue_curr->au4_out_ofsts[ctr] = (UWORD32)u8_temp;
                    }

                    ps_job_queue_curr++;
                }
            }  //for ends

            /* set the last pointer to NULL */
            ps_job_queue_curr--;
            ps_job_queue_curr->pv_next = (void *)NULL;
        }  //else ends
    }

    //prepare Enc_loop JOB queue for all bitrate instances
    //for(pass = 0; pass < NUM_ENC_JOBS_QUES; pass++)
    for(i = 0; i < i4_num_bitrate_instances; i++)
    {
        job_queue_t *ps_job_queue_curr;
        job_queue_t *ps_job_queue_next;
        WORD32 ctr;
        WORD32 inp_dep;
        WORD32 out_dep;
        WORD32 num_vert_units;
        HEVCE_ENC_JOB_TYPES_T task_type;

        /* In case of I or IDR pictures ME will not perform any processing */
        //if(ENC_LOOP_JOB == pass)
        {
            if(((IV_I_FRAME == i4_pic_type) || (IV_IDR_FRAME == i4_pic_type)) &&
               !L0ME_IN_OPENLOOP_MODE)
            {
                inp_dep = 0;
            }
            else
            {
                inp_dep = 1;
            }

            task_type = (HEVCE_ENC_JOB_TYPES_T)(ENC_LOOP_JOB + i);
            ps_job_queue_curr =
                (job_queue_t *)ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
                    ->as_job_que_enc_hdls[ENC_LOOP_JOB + i]
                    .pv_next;
            ps_job_queue_next =
                (job_queue_t *)ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]
                    ->as_job_que_enc_hdls[ENC_LOOP_JOB + i + 1]
                    .pv_next;
            out_dep = 1;  //output dependecny is the next bit-rate instance's input dependency
            num_vert_units = num_ctb_vert_rows;

            if(i == i4_num_bitrate_instances - 1)  //for last bit-rate instance
            {
                //clear output dependency
                ps_job_queue_next = NULL;
                out_dep = 0;
            }
        }

        /* loop over all the vertical rows */
        for(num_jobs = 0; num_jobs < num_vert_units; num_jobs++)
        {
            /* loop over all the column tiles */
            for(col_tile_ctr = 0; col_tile_ctr < i4_num_tile_col; col_tile_ctr++)
            {
                ULWORD64 u8_temp;

                {
                    ps_job_queue_curr->s_job_info.s_enc_loop_job_info.i4_ctb_row_no = num_jobs;
                    ps_job_queue_curr->s_job_info.s_enc_loop_job_info.i4_tile_col_idx =
                        col_tile_ctr;
                    ps_job_queue_curr->s_job_info.s_enc_loop_job_info.i4_bitrate_instance_no = i;
                }

                ps_job_queue_curr->pv_next = (void *)(ps_job_queue_curr + 1);

                ps_job_queue_curr->i4_task_type = task_type;

                ps_job_queue_curr->i4_num_input_dep = inp_dep;

                /* set the entire input dep buffer to default value 0 */
                memset(&ps_job_queue_curr->au1_in_dep[0], 0, sizeof(UWORD8) * MAX_IN_DEP);

                /* set the input dep buffer to 1 for num inp dep */
                if(0 != inp_dep)
                {
                    memset(&ps_job_queue_curr->au1_in_dep[0], 1, sizeof(UWORD8) * inp_dep);
                }

                ps_job_queue_curr->i4_num_output_dep = out_dep;

                /* set the entire offset buffer to default value */
                memset(&ps_job_queue_curr->au4_out_ofsts[0], 0xFF, sizeof(UWORD32) * MAX_OUT_DEP);

                for(ctr = 0; ctr < out_dep; ctr++)
                {
                    /* col tile level dependency b/w EncLoops of MBR */
                    u8_temp = (ULWORD64)(
                        &ps_job_queue_next[num_jobs * i4_num_tile_col + col_tile_ctr] -
                        ps_ctxt->s_multi_thrd.aps_cur_out_me_prms[i4_curr_frm_id]->ps_job_q_enc);

                    u8_temp *= sizeof(job_queue_t);

                    /* store the offset to the array */
                    ps_job_queue_curr->au4_out_ofsts[ctr] = (UWORD32)u8_temp;
                }

                ps_job_queue_curr++;
            }
        }

        /* set the last pointer to NULL */
        ps_job_queue_curr--;
        ps_job_queue_curr->pv_next = (void *)NULL;
    }

    return;

} /* End of ihevce_prepare_job_queue */

/**
*******************************************************************************
*
* @brief Function prepares the Job Queues for all the passes of pre enc
*
* @par Description: Based on picture type sets the input and output dependency
*
* @param[inout] pv_enc_ctxt
*  Pointer to encoder context
*
* @param[in] ps_curr_inp
*   Current Input buffer pointer
*
* @returns
*  None
*
* @remarks
*
*******************************************************************************
*/
void ihevce_prepare_pre_enc_job_queue(
    void *pv_enc_ctxt, ihevce_lap_enc_buf_t *ps_curr_inp, WORD32 i4_ping_pong)
{
    /* local variables */
    enc_ctxt_t *ps_ctxt;
    job_queue_t *ps_decomp_job_queue_lyr0;
    job_queue_t *ps_decomp_job_queue_lyr1;
    job_queue_t *ps_decomp_job_queue_lyr2;
    job_queue_t *ps_decomp_job_queue_lyr3;
    job_queue_t *ps_me_job_queue_lyr1;
    job_queue_t *ps_me_job_queue_lyr2;
    job_queue_t *ps_me_job_queue_lyr3;
    job_queue_t *ps_me_job_queue_lyr4;
    job_queue_t *ps_ipe_job_queue;
    job_queue_t *aps_me_job_queues[MAX_NUM_HME_LAYERS];
    multi_thrd_me_job_q_prms_t *ps_me_job_q_prms;
    WORD32 ai4_decomp_num_vert_units_lyr[MAX_NUM_HME_LAYERS];
    WORD32 a14_decomp_lyr_unit_size[MAX_NUM_HME_LAYERS];
    WORD32 layer_no;
    WORD32 decomp_lyr_cnt;
    WORD32 num_jobs;
    WORD32 n_tot_layers;
    WORD32 a_wd[MAX_NUM_HME_LAYERS];
    WORD32 a_ht[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_wd[MAX_NUM_HME_LAYERS];
    WORD32 a_disp_ht[MAX_NUM_HME_LAYERS];
    WORD32 u4_log_ctb_size;
    WORD32 num_ctb_vert_rows;
    WORD32 pass;
    WORD32 me_lyr_cnt;
    WORD32 num_hme_lyrs;
    WORD32 ai4_me_num_vert_units_lyr[MAX_NUM_HME_LAYERS];
    WORD32 me_start_lyr_pass;
    WORD32 ctb_size;
    WORD32 me_coarsest_lyr_inp_dep = -1;

    (void)ps_curr_inp;
    /* derive local varaibles */
    ps_ctxt = (enc_ctxt_t *)pv_enc_ctxt;
    num_ctb_vert_rows = ps_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert;

    /* CHANGE REQUIRED: change the pointer to the job queue buffer */
    /* memset the entire job que buffer to zero */
    memset(
        ps_ctxt->s_multi_thrd.aps_job_q_pre_enc[i4_ping_pong],
        0,
        MAX_NUM_VERT_UNITS_FRM * NUM_PRE_ENC_JOBS_QUES * sizeof(job_queue_t));

    /* Get the number of vertical units in a layer from the resolution of the layer */
    a_wd[0] = ps_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_wd;
    a_ht[0] = ps_ctxt->s_frm_ctb_prms.i4_cu_aligned_pic_ht;
    n_tot_layers = hme_derive_num_layers(1, a_wd, a_ht, a_disp_wd, a_disp_ht);
    GETRANGE(u4_log_ctb_size, ps_ctxt->s_frm_ctb_prms.i4_ctb_size);

    ASSERT(n_tot_layers >= 3);

    /*
    * Always force minimum layers as 4 so that we would have both l1 and l2
    * pre intra analysis
    */
    if(n_tot_layers == 3)
    {
        n_tot_layers = 4;
        a_wd[3] = CEIL16(a_wd[2] >> 1);
        a_ht[3] = CEIL16(a_ht[2] >> 1);
    }

    for(layer_no = 0; layer_no < n_tot_layers; layer_no++)
    {
        ctb_size = 1 << (u4_log_ctb_size - 1 - layer_no);
        ai4_decomp_num_vert_units_lyr[layer_no] = ((a_ht[layer_no] + ctb_size) & ~(ctb_size - 1)) >>
                                                  (u4_log_ctb_size - 1 - layer_no);
        a14_decomp_lyr_unit_size[layer_no] = 1 << (u4_log_ctb_size - 1 - layer_no);
    }

    /* get the start address of  Job queues */
    ps_decomp_job_queue_lyr0 = ps_ctxt->s_multi_thrd.aps_job_q_pre_enc[i4_ping_pong];
    ps_decomp_job_queue_lyr1 = ps_decomp_job_queue_lyr0 + MAX_NUM_VERT_UNITS_FRM;
    ps_decomp_job_queue_lyr2 = ps_decomp_job_queue_lyr1 + MAX_NUM_VERT_UNITS_FRM;
    ps_decomp_job_queue_lyr3 = ps_decomp_job_queue_lyr2 + MAX_NUM_VERT_UNITS_FRM;
    ps_me_job_queue_lyr4 = ps_decomp_job_queue_lyr3 + MAX_NUM_VERT_UNITS_FRM;
    ps_me_job_queue_lyr3 = ps_me_job_queue_lyr4 + MAX_NUM_VERT_UNITS_FRM;
    ps_me_job_queue_lyr2 = ps_me_job_queue_lyr3 + MAX_NUM_VERT_UNITS_FRM;
    ps_me_job_queue_lyr1 = ps_me_job_queue_lyr2 + MAX_NUM_VERT_UNITS_FRM;

    ps_ipe_job_queue = ps_me_job_queue_lyr1 + MAX_NUM_VERT_UNITS_FRM;

    /* store the JOB queue in the Job handle */
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][DECOMP_JOB_LYR0].pv_next =
        (void *)ps_decomp_job_queue_lyr0;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][DECOMP_JOB_LYR1].pv_next =
        (void *)ps_decomp_job_queue_lyr1;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][DECOMP_JOB_LYR2].pv_next =
        (void *)ps_decomp_job_queue_lyr2;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][DECOMP_JOB_LYR3].pv_next =
        (void *)ps_decomp_job_queue_lyr3;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][ME_JOB_LYR4].pv_next =
        (void *)ps_me_job_queue_lyr4;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][ME_JOB_LYR3].pv_next =
        (void *)ps_me_job_queue_lyr3;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][ME_JOB_LYR2].pv_next =
        (void *)ps_me_job_queue_lyr2;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][ME_JOB_LYR1].pv_next =
        (void *)ps_me_job_queue_lyr1;
    ps_ctxt->s_multi_thrd.as_job_que_preenc_hdls[i4_ping_pong][IPE_JOB_LYR0].pv_next =
        (void *)ps_ipe_job_queue;

    /* store the ME Jobs que into array */
    aps_me_job_queues[0] = NULL;
    aps_me_job_queues[1] = ps_me_job_queue_lyr1;
    aps_me_job_queues[2] = ps_me_job_queue_lyr2;
    aps_me_job_queues[3] = ps_me_job_queue_lyr3;
    aps_me_job_queues[4] = ps_me_job_queue_lyr4;
    decomp_lyr_cnt = 0;
    /* Set the me_lyr_cnt to 0  */
    me_lyr_cnt = 0;

    /* call the ME function which returns the layer properties */
    ihevce_coarse_me_get_lyr_prms_job_que(
        ps_ctxt->s_module_ctxt.pv_coarse_me_ctxt,
        ps_curr_inp,
        &num_hme_lyrs,
        &ai4_me_num_vert_units_lyr[0],
        &ps_ctxt->s_multi_thrd.as_me_job_q_prms[0][0]);

    ps_me_job_q_prms = &ps_ctxt->s_multi_thrd.as_me_job_q_prms[0][0];

    /* derive ME coarsest layer tak type */
    me_start_lyr_pass = ME_JOB_LYR4 + (MAX_NUM_HME_LAYERS - num_hme_lyrs);

    ps_ctxt->s_multi_thrd.i4_me_coarsest_lyr_type = me_start_lyr_pass;

    /* coarsest HME layer number of units should be less than or equal to max in dep in Job queue */
    /* this constraint is to take care of Coarsest layer requring entire layer to do FULL search */
    ASSERT(ai4_me_num_vert_units_lyr[0] <= MAX_IN_DEP);
    /* loop over all the passes in the encoder */
    for(pass = 0; pass < NUM_PRE_ENC_JOBS_QUES; pass++)
    {
        job_queue_t *ps_pre_enc_job_queue_curr;
        job_queue_t *ps_pre_enc_job_queue_next;
        WORD32 inp_dep_pass;
        WORD32 out_dep_pass;
        WORD32 num_vert_units;
        HEVCE_PRE_ENC_JOB_TYPES_T pre_enc_task_type;
        HEVCE_ENC_JOB_TYPES_T enc_task_type;
        WORD32 proc_valid_flag = 0;

        // num_vert_units = ai4_decomp_num_vert_units_lyr[decomp_lyr_cnt];
        /* Initializing the job queues for max no of rows among all the layers. And max would be for last layer*/
        num_vert_units = ai4_decomp_num_vert_units_lyr[n_tot_layers - 1];

        if(DECOMP_JOB_LYR0 == pass)
        {
            proc_valid_flag = 1;
            pre_enc_task_type = DECOMP_JOB_LYR0;
            enc_task_type = (HEVCE_ENC_JOB_TYPES_T)-1;
            ps_pre_enc_job_queue_curr = ps_decomp_job_queue_lyr0;

            inp_dep_pass = 0;
            decomp_lyr_cnt++;

            /* If all the decomp layers are done next job queue will be ME job queue */
            if(decomp_lyr_cnt == (n_tot_layers - 1))
            {
                /* Assumption : num_hme_lyrs > 1*/
                ps_pre_enc_job_queue_next = aps_me_job_queues[num_hme_lyrs - 1];

                /* ME coarsest layer is currently made dependent on entire decomp layer */
                out_dep_pass = ai4_me_num_vert_units_lyr[0];
                me_coarsest_lyr_inp_dep = num_vert_units;
            }
            else
            {
                ps_pre_enc_job_queue_next = ps_decomp_job_queue_lyr1;
                out_dep_pass = 3;
            }
        }
        else if((DECOMP_JOB_LYR1 == pass) && (decomp_lyr_cnt != (n_tot_layers - 1)))
        {
            proc_valid_flag = 1;
            pre_enc_task_type = DECOMP_JOB_LYR1;
            enc_task_type = (HEVCE_ENC_JOB_TYPES_T)-1;
            ps_pre_enc_job_queue_curr = ps_decomp_job_queue_lyr1;

            inp_dep_pass = 3;
            decomp_lyr_cnt++;

            /* If all the decomp layers are done next job queue will be ME job queue */
            if(decomp_lyr_cnt == (n_tot_layers - 1))
            {
                /* Assumption : num_hme_lyrs > 1*/
                ps_pre_enc_job_queue_next = aps_me_job_queues[num_hme_lyrs - 1];

                /* ME coarsest layer is currently made dependent on entire decomp layer */
                out_dep_pass = ai4_me_num_vert_units_lyr[0];
                me_coarsest_lyr_inp_dep = num_vert_units;
            }
            else
            {
                ps_pre_enc_job_queue_next = ps_decomp_job_queue_lyr2;
                out_dep_pass = 3;
            }
        }
        else if((DECOMP_JOB_LYR2 == pass) && (decomp_lyr_cnt != (n_tot_layers - 1)))
        {
            proc_valid_flag = 1;
            pre_enc_task_type = DECOMP_JOB_LYR2;
            enc_task_type = (HEVCE_ENC_JOB_TYPES_T)-1;
            ps_pre_enc_job_queue_curr = ps_decomp_job_queue_lyr2;

            inp_dep_pass = 3;
            decomp_lyr_cnt++;

            /* If all the decomp layers are done next job queue will be ME job queue */
            if(decomp_lyr_cnt == (n_tot_layers - 1))
            {
                /* Assumption : num_hme_lyrs > 1*/
                ps_pre_enc_job_queue_next = aps_me_job_queues[num_hme_lyrs - 1];

                /* ME coarsest layer is currently made dependent on entire decomp layer */
                out_dep_pass = ai4_me_num_vert_units_lyr[0];
                me_coarsest_lyr_inp_dep = num_vert_units;
            }
            else
            {
                /* right now MAX 4 layers worth of JOB queues are prepared */
                ASSERT(0);
            }
        }

        else if(IPE_JOB_LYR0 == pass)
        {
            proc_valid_flag = 1;
            pre_enc_task_type = IPE_JOB_LYR0;
            enc_task_type = (HEVCE_ENC_JOB_TYPES_T)-1;
            ps_pre_enc_job_queue_curr = ps_ipe_job_queue;
            ps_pre_enc_job_queue_next = NULL;
            num_vert_units = num_ctb_vert_rows;
        }
        else if(((pass >= ME_JOB_LYR4) && (pass <= ME_JOB_LYR1)) && (pass >= me_start_lyr_pass))
        {
            /* num_ver_units of coarsest layer is stored at 0th index */
            num_vert_units = ai4_me_num_vert_units_lyr[me_lyr_cnt];
            proc_valid_flag = 1;

            pre_enc_task_type =
                (HEVCE_PRE_ENC_JOB_TYPES_T)((WORD32)ME_JOB_LYR1 - (num_hme_lyrs - me_lyr_cnt - 2));

            enc_task_type = (HEVCE_ENC_JOB_TYPES_T)-1;

            /* Assumption : num_hme_lyrs > 1*/
            ps_pre_enc_job_queue_curr = aps_me_job_queues[num_hme_lyrs - me_lyr_cnt - 1];

            if(me_lyr_cnt == (num_hme_lyrs - 2))
            {
                ps_pre_enc_job_queue_next = ps_ipe_job_queue;
            }
            else
            {
                ps_pre_enc_job_queue_next = aps_me_job_queues[num_hme_lyrs - me_lyr_cnt - 2];
            }
            me_lyr_cnt++;
        }

        /* check for valid processing flag */
        if(0 == proc_valid_flag)
        {
            continue;
        }

        /* in the loop ps_me_job_q_prms get incremented for every row */
        /* so at the end of one layer the pointer will be correctly   */
        /* pointing to the start of next layer                        */

        /* loop over all the vertical rows */
        for(num_jobs = 0; num_jobs < num_vert_units; num_jobs++)
        {
            ULWORD64 u8_temp;
            WORD32 inp_dep = 0;
            WORD32 out_dep = 0;
            WORD32 ctr;
            WORD32 job_off_ipe;

            if(IPE_JOB_LYR0 == pass)
            {
                ps_pre_enc_job_queue_curr->s_job_info.s_ipe_job_info.i4_ctb_row_no = num_jobs;
                inp_dep = ps_me_job_q_prms->i4_num_inp_dep;
                out_dep = 0;
            }
            else if((pass >= DECOMP_JOB_LYR0) && (pass <= DECOMP_JOB_LYR3))
            {
                ps_pre_enc_job_queue_curr->s_job_info.s_decomp_job_info.i4_vert_unit_row_no =
                    num_jobs;

                /* Input and output dependencies of 1st row and last row is 1 less than other rows*/
                inp_dep = inp_dep_pass;
                out_dep = out_dep_pass;

                if(pass != DECOMP_JOB_LYR0)
                {
                    if(((num_jobs == 0) || (num_jobs == num_vert_units - 1)))
                    {
                        inp_dep = inp_dep_pass - 1;
                    }
                }

                if(pass != (DECOMP_JOB_LYR0 + n_tot_layers - 2))
                {
                    if(((num_jobs == 0) || (num_jobs == num_vert_units - 1)))
                    {
                        out_dep = out_dep_pass - 1;
                    }
                }
            }
            else /* remaining all are ME JOBS */
            {
                ps_pre_enc_job_queue_curr->s_job_info.s_me_job_info.i4_vert_unit_row_no = num_jobs;

                if(pass == me_start_lyr_pass)
                {
                    ASSERT(me_coarsest_lyr_inp_dep != -1);
                    inp_dep = me_coarsest_lyr_inp_dep;
                }
                else
                {
                    inp_dep = ps_me_job_q_prms->i4_num_inp_dep;
                }
                out_dep = ps_me_job_q_prms->i4_num_output_dep;
            }
            ps_pre_enc_job_queue_curr->pv_next = (void *)(ps_pre_enc_job_queue_curr + 1);

            ps_pre_enc_job_queue_curr->i4_pre_enc_task_type = pre_enc_task_type;
            ps_pre_enc_job_queue_curr->i4_task_type = enc_task_type;

            /* Set the input dependencies */
            ps_pre_enc_job_queue_curr->i4_num_input_dep = inp_dep;

            /* set the entire input dep buffer to default value 0 */
            memset(&ps_pre_enc_job_queue_curr->au1_in_dep[0], 0, sizeof(UWORD8) * MAX_IN_DEP);

            /* set the input dep buffer to 1 for num inp dep */
            if(0 != inp_dep)
            {
                memset(&ps_pre_enc_job_queue_curr->au1_in_dep[0], 1, sizeof(UWORD8) * inp_dep);
            }

            /* If decomposition layer ends at this pass the no of out dependencies
            * will be based on number of vertical units in the coarsets layer of HME
            * This is because the search range in coarsest layer will be almost
            * entire frame (search range of +-128 in vert direction is max supported
            */
            if(pass == (DECOMP_JOB_LYR0 + n_tot_layers - 2))
            {
                job_off_ipe = 0;
            }
            else
            {
                if(num_jobs == 0)
                    job_off_ipe = num_jobs;

                else
                    job_off_ipe = num_jobs - 1;
            }

            /* Set the offsets of output dependencies */
            ps_pre_enc_job_queue_curr->i4_num_output_dep = out_dep;

            /* set the entire offset buffer to default value */
            memset(
                &ps_pre_enc_job_queue_curr->au4_out_ofsts[0], 0xFF, sizeof(UWORD32) * MAX_OUT_DEP);

            for(ctr = 0; ctr < out_dep; ctr++)
            {
                /* if IPE or DECOMP loop the dep is 1 to 1*/
                if(((pass >= DECOMP_JOB_LYR0) && (pass <= DECOMP_JOB_LYR3)) ||
                   (IPE_JOB_LYR0 == pass))
                {
                    u8_temp = (ULWORD64)(
                        &ps_pre_enc_job_queue_next[job_off_ipe] -
                        ps_ctxt->s_multi_thrd.aps_job_q_pre_enc[i4_ping_pong]);

                    u8_temp *= sizeof(job_queue_t);

                    /* add the excat inp dep byte for the next layer JOB */
                    u8_temp += ps_pre_enc_job_queue_next[job_off_ipe].i4_num_input_dep;

                    /* increment the inp dep number for a given job */
                    ps_pre_enc_job_queue_next[job_off_ipe].i4_num_input_dep++;

                    job_off_ipe++;
                }
                else if((pass >= ME_JOB_LYR4) && (pass <= ME_JOB_LYR1))
                {
                    /* ME layer Jobs */
                    WORD32 job_off;

                    job_off = ps_me_job_q_prms->ai4_out_dep_unit_off[ctr];

                    u8_temp = (ULWORD64)(
                        &ps_pre_enc_job_queue_next[job_off] -
                        ps_ctxt->s_multi_thrd.aps_job_q_pre_enc[i4_ping_pong]);

                    u8_temp *= sizeof(job_queue_t);

                    /* add the excat inp dep byte for the next layer JOB */
                    u8_temp += ps_pre_enc_job_queue_next[job_off].i4_num_input_dep;

                    /* increment the inp dep number for a given job */
                    ps_pre_enc_job_queue_next[job_off].i4_num_input_dep++;
                }
                /* store the offset to the array */
                ps_pre_enc_job_queue_curr->au4_out_ofsts[ctr] = (UWORD32)u8_temp;
            }
            /* ME job q params is incremented only for ME jobs */
            if(((pass >= ME_JOB_LYR4) && (pass <= ME_JOB_LYR1)) || (IPE_JOB_LYR0 == pass))
            {
                ps_me_job_q_prms++;
            }
            ps_pre_enc_job_queue_curr++;
        }

        /* set the last pointer to NULL */
        ps_pre_enc_job_queue_curr--;
        ps_pre_enc_job_queue_curr->pv_next = (void *)NULL;
    }

    /* reset the num ctb processed in every row  for IPE sync */
    memset(
        &ps_ctxt->s_multi_thrd.ai4_ctbs_in_row_proc_ipe_pass[0],
        0,
        (MAX_NUM_CTB_ROWS_FRM * sizeof(WORD32)));

} /* End of ihevce_prepare_pre_enc_job_queue */
