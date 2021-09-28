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
* \file ihevce_coarse_me_pass.c
*
* \brief
*    Converts the language of the encoder to language of me. This is an i/f
*    between the encoder style APIs and ME style APIs. This is basically
*    a memoryless glue layer.
*
* \date
*    22/10/2012
*
* \author
*    Ittiam
*
*
* List of Functions
*
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
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"
#include "ihevce_me_pass.h"
#include "ihevce_coarse_me_pass.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for ME module
*    Note : Include total mem. req. for HME + Total mem. req. for Dep Mngr for HME
*
* \return
*    Number of memory records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_coarse_me_get_num_mem_recs()
{
    WORD32 hme_mem_recs = hme_coarse_num_alloc();
    WORD32 hme_dep_mngr_mem_recs = hme_coarse_dep_mngr_num_alloc();

    return ((hme_mem_recs + hme_dep_mngr_mem_recs));
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for coarse ME.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_num_proc_thrds : Number of processing threads for this module
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
* \return
*    Number of records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_coarse_me_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id)
{
    hme_memtab_t as_memtabs[HME_COARSE_TOT_MEMTABS];
    WORD32 n_tabs, i;

    /* Init prms structure specific to HME */
    hme_init_prms_t s_hme_init_prms;

    //return (ihevce_coarse_me_get_num_mem_recs());
    /*************************************************************************/
    /* code flow: we call hme alloc function and then remap those memtabs    */
    /* to a different type of memtab structure.                              */
    /*************************************************************************/
    ASSERT(HME_COARSE_TOT_MEMTABS >= hme_coarse_num_alloc());

    /*************************************************************************/
    /* POPULATE THE HME INIT PRMS                                            */
    /*************************************************************************/
    ihevce_derive_me_init_prms(ps_init_prms, &s_hme_init_prms, i4_num_proc_thrds, i4_resolution_id);

    /*************************************************************************/
    /* CALL THE ME FUNCTION TO GET MEMTABS                                   */
    /*************************************************************************/
    n_tabs = hme_coarse_alloc(&as_memtabs[0], &s_hme_init_prms);
    ASSERT(n_tabs == hme_coarse_num_alloc());

    /*************************************************************************/
    /* REMAP RESULTS TO ENCODER MEMTAB STRUCTURE                             */
    /*************************************************************************/
    for(i = 0; i < n_tabs; i++)
    {
        ps_mem_tab[i].i4_mem_size = as_memtabs[i].size;
        ps_mem_tab[i].i4_mem_alignment = as_memtabs[i].align;
        ps_mem_tab[i].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
        ps_mem_tab[i].i4_size = sizeof(iv_mem_rec_t);
    }

    /*************************************************************************/
    /* --- HME Coarse sync Dep Mngr Mem requests --                          */
    /*************************************************************************/
    {
        WORD32 n_dep_tabs;

        ps_mem_tab += n_tabs;

        n_dep_tabs = hme_coarse_dep_mngr_alloc(
            ps_mem_tab, ps_init_prms, i4_mem_space, i4_num_proc_thrds, i4_resolution_id);

        ASSERT(n_dep_tabs == hme_coarse_dep_mngr_num_alloc());

        /* Update the total no. of mem tabs */
        n_tabs += n_dep_tabs;
    }

    return (n_tabs);
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_init \endif
*
* \brief
*    Intialization for ME context state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] pv_osal_handle : Osal handle
*
* \return
*    Handle to the ME context
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_coarse_me_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available)
{
    /* ME handle to be returned */
    void *pv_me_ctxt;
    WORD32 status;
    coarse_me_master_ctxt_t *ps_ctxt;

    /* Init prms structure specific to HME */
    hme_init_prms_t s_hme_init_prms;

    /* memtabs to be passed to hme */
    hme_memtab_t as_memtabs[HME_COARSE_TOT_MEMTABS];
    WORD32 n_tabs, n_dep_tabs, i;

    /*************************************************************************/
    /* POPULATE THE HME INIT PRMS                                            */
    /*************************************************************************/
    ihevce_derive_me_init_prms(ps_init_prms, &s_hme_init_prms, i4_num_proc_thrds, i4_resolution_id);

    /*************************************************************************/
    /* Ensure local declaration is sufficient                                */
    /*************************************************************************/
    n_tabs = hme_coarse_num_alloc();
    ASSERT(HME_COARSE_TOT_MEMTABS >= n_tabs);

    /*************************************************************************/
    /* MAP RESULTS TO HME MEMTAB STRUCTURE                                   */
    /*************************************************************************/
    for(i = 0; i < n_tabs; i++)
    {
        as_memtabs[i].size = ps_mem_tab[i].i4_mem_size;
        as_memtabs[i].align = ps_mem_tab[i].i4_mem_alignment;
        as_memtabs[i].pu1_mem = (U08 *)ps_mem_tab[i].pv_base;
    }
    /*************************************************************************/
    /* CALL THE ME FUNCTION TO GET MEMTABS                                   */
    /*************************************************************************/
    pv_me_ctxt = (void *)as_memtabs[0].pu1_mem;
    status = hme_coarse_init(pv_me_ctxt, &as_memtabs[0], &s_hme_init_prms);
    ps_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    if(status == -1)
        return NULL;

    /*************************************************************************/
    /* --- HME sync Dep Mngr Mem init --                                     */
    /*************************************************************************/

    ps_mem_tab += n_tabs;

    n_dep_tabs = hme_coarse_dep_mngr_init(
        ps_mem_tab, ps_init_prms, pv_me_ctxt, pv_osal_handle, i4_num_proc_thrds, i4_resolution_id);
    ASSERT(n_dep_tabs <= hme_coarse_dep_mngr_num_alloc());

    n_tabs += n_dep_tabs;

    ihevce_me_instr_set_router(
        (ihevce_me_optimised_function_list_t *)ps_ctxt->pv_me_optimised_function_list,
        ps_init_prms->e_arch_type);

    ihevce_cmn_utils_instr_set_router(
        &ps_ctxt->s_cmn_opt_func, u1_is_popcnt_available, ps_init_prms->e_arch_type);

    return (pv_me_ctxt);
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_reg_thrds_sem \endif
*
* \brief
*    Intialization for ME context state structure with semaphores .
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ppv_sem_hdls : Array of semaphore handles
* \param[in] i4_num_proc_thrds : Number of processing threads
*
* \return
*   none
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_reg_thrds_sem(void *pv_me_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds)
{
    hme_coarse_dep_mngr_reg_sem(pv_me_ctxt, ppv_sem_hdls, i4_num_proc_thrds);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_delete \endif
*
* \brief
*    Destroy Coarse ME module
* Note : Only Destroys the resources allocated in the module like
*   semaphore,etc. Memory free is done Separately using memtabs
*
* \param[in] pv_me_ctxt : pointer to Coarse ME ctxt
* \param[in] ps_init_prms : Create time static parameters
* \param[in] pv_osal_handle : Osal handle
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_delete(
    void *pv_me_ctxt, ihevce_static_cfg_params_t *ps_init_prms, WORD32 i4_resolution_id)
{
    /* --- HME sync Dep Mngr Delete --*/
    hme_coarse_dep_mngr_delete(pv_me_ctxt, ps_init_prms, i4_resolution_id);
}

/**
*******************************************************************************
* \if Function name : ihevce_coarse_me_set_resolution \endif
*
* \brief
*    Sets the resolution for ME state
*
* \par Description:
*    ME requires information of resolution to prime up its layer descriptors
*    and contexts. This API is called whenever a control call from application
*    causes a change of resolution. Has to be called once initially before
*    processing any frame. Again this is just a glue function and calls the
*    actual ME API for the same.
*
* \param[in,out] pv_me_ctxt: Handle to the ME context
* \param[in] n_enc_layers: Number of layers getting encoded
* \param[in] p_wd : Pointer containing widths of each layer getting encoded.
* \param[in] p_ht : Pointer containing heights of each layer getting encoded.
*
* \returns
*  none
*
* \author
*  Ittiam
*
*******************************************************************************
*/
void ihevce_coarse_me_set_resolution(
    void *pv_me_ctxt, WORD32 n_enc_layers, WORD32 *p_wd, WORD32 *p_ht)
{
    /* local variables */
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    WORD32 thrds;

    for(thrds = 0; thrds < ps_master_ctxt->i4_num_proc_thrds; thrds++)
    {
        coarse_me_ctxt_t *ps_me_thrd_ctxt;

        ps_me_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[thrds];

        hme_coarse_set_resolution((void *)ps_me_thrd_ctxt, n_enc_layers, p_wd, p_ht);
    }
}
void ihevce_coarse_me_get_rc_param(
    void *pv_me_ctxt,
    LWORD64 *i8_acc_frame_hme_cost,
    LWORD64 *i8_acc_frame_hme_sad,
    LWORD64 *i8_acc_num_blks_higher_sad,
    LWORD64 *i8_total_blks,
    WORD32 i4_is_prev_pic_same_scene)
{
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    WORD32 thrds;
    coarse_me_ctxt_t *ps_me_thrd_ctxt;

    *i8_acc_frame_hme_cost = 0;
    *i8_acc_frame_hme_sad = 0;

    for(thrds = 0; thrds < ps_master_ctxt->i4_num_proc_thrds; thrds++)
    {
        ps_me_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[thrds];
        *i8_acc_frame_hme_cost += ps_me_thrd_ctxt->i4_L1_hme_best_cost;

        /*Calculate me cost wrt. to ref only for P frame */
        if(ps_me_thrd_ctxt->s_frm_prms.is_i_pic == ps_me_thrd_ctxt->s_frm_prms.bidir_enabled)
        {
            *i8_acc_num_blks_higher_sad += ps_me_thrd_ctxt->i4_num_blks_high_sad;
            *i8_total_blks += ps_me_thrd_ctxt->i4_num_blks;
        }

        *i8_acc_frame_hme_sad += ps_me_thrd_ctxt->i4_L1_hme_sad;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_process \endif
*
* \brief
*    Frame level ME function
*
* \par Description:
*    Processing of all layers starting from coarse and going
*    to the refinement layers, except enocde layer
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] ps_enc_lap_inp  : pointer to input yuv buffer (frame buffer)
* \param[in,out] ps_ctb_out : pointer to CTB analyse output structure (frame buffer)
* \param[out] ps_cu_out : pointer to CU analyse output structure (frame buffer)
* \param[in]  pd_intra_costs : pointerto intra cost buffer
* \param[in]  ps_multi_thrd_ctxt : pointer to multi thread ctxt
* \param[in]  thrd_id : Thread id of the current thrd in which function is executed
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_process(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_ping_pong)

{
    /* local variables */
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_thrd_ctxt;

    /* get the current thread ctxt pointer */
    ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[thrd_id];
    ps_thrd_ctxt->thrd_id = thrd_id;

    /* frame level processing function */
    hme_coarse_process_frm(
        (void *)ps_thrd_ctxt,
        &ps_master_ctxt->s_ref_map,
        &ps_master_ctxt->s_frm_prms,
        ps_multi_thrd_ctxt,
        i4_ping_pong,
        &ps_master_ctxt->apv_dep_mngr_hme_sync[0]);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_frame_end \endif
*
* \brief
*    End of frame update function performs
*       - GMV collation
*       - Dynamic Search Range collation
*
* \param[in] pv_ctxt : pointer to ME module
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_frame_end(void *pv_me_ctxt)
{
    /* local variables */
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    layer_ctxt_t *ps_curr_layer;
    WORD32 num_ref, num_thrds, cur_poc;
    WORD32 coarse_layer_id;
    WORD32 i4_num_ref;
    ME_QUALITY_PRESETS_T e_me_quality_preset;

    /* GMV collation is done for coarse Layer only */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];
    coarse_layer_id = ps_thrd0_ctxt->num_layers - 1;
    ps_curr_layer = ps_thrd0_ctxt->ps_curr_descr->aps_layers[coarse_layer_id];
    i4_num_ref = ps_master_ctxt->s_ref_map.i4_num_ref;
    e_me_quality_preset = ps_thrd0_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets;

    /* No processing is required if current pic is I pic */
    if(1 == ps_master_ctxt->s_frm_prms.is_i_pic)
    {
        return;
    }

    /* use thrd 0 ctxt to collate the GMVs histogram and Dynamic Search Range */
    /* across all threads */
    for(num_ref = 0; num_ref < i4_num_ref; num_ref++)
    {
        WORD32 i4_offset, i4_lobe_size, i4_layer_id;
        mv_hist_t *ps_hist_thrd0;
        dyn_range_prms_t *aps_dyn_range_prms_thrd0[MAX_NUM_LAYERS];

        ps_hist_thrd0 = ps_thrd0_ctxt->aps_mv_hist[num_ref];

        /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
        if(ps_thrd0_ctxt->s_frm_prms.is_i_pic == ps_thrd0_ctxt->s_frm_prms.bidir_enabled)
        {
            for(i4_layer_id = coarse_layer_id; i4_layer_id > 0; i4_layer_id--)
            {
                aps_dyn_range_prms_thrd0[i4_layer_id] =
                    &ps_thrd0_ctxt->s_coarse_dyn_range_prms.as_dyn_range_prms[i4_layer_id][num_ref];
            }
        }

        i4_lobe_size = ps_hist_thrd0->i4_lobe1_size;
        i4_offset = i4_lobe_size >> 1;

        /* run a loop over all the other threads to add up the histogram */
        /* and to update the dynamical search range                      */
        for(num_thrds = 1; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            dyn_range_prms_t *ps_dyn_range_prms;

            if(ME_XTREME_SPEED_25 != e_me_quality_preset)
            {
                mv_hist_t *ps_hist;
                WORD32 i4_y, i4_x;
                /* get current thrd histogram pointer */
                ps_hist = ps_master_ctxt->aps_me_ctxt[num_thrds]->aps_mv_hist[num_ref];

                /* Accumalate the Bin count for all the thread */
                for(i4_y = 0; i4_y < ps_hist_thrd0->i4_num_rows; i4_y++)
                {
                    for(i4_x = 0; i4_x < ps_hist_thrd0->i4_num_cols; i4_x++)
                    {
                        S32 i4_bin_id;

                        i4_bin_id = i4_x + (i4_y * ps_hist_thrd0->i4_num_cols);

                        ps_hist_thrd0->ai4_bin_count[i4_bin_id] +=
                            ps_hist->ai4_bin_count[i4_bin_id];
                    }
                }
            }

            /* Update the dynamical search range for each Layer              */
            /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
            if(ps_thrd0_ctxt->s_frm_prms.is_i_pic == ps_thrd0_ctxt->s_frm_prms.bidir_enabled)
            {
                for(i4_layer_id = coarse_layer_id; i4_layer_id > 0; i4_layer_id--)
                {
                    /* get current thrd, layer dynamical search range param. pointer */
                    ps_dyn_range_prms =
                        &ps_master_ctxt->aps_me_ctxt[num_thrds]
                             ->s_coarse_dyn_range_prms.as_dyn_range_prms[i4_layer_id][num_ref];
                    /* TODO : This calls can be optimized further. No need for min in 1st call and max in 2nd call */
                    hme_update_dynamic_search_params(
                        aps_dyn_range_prms_thrd0[i4_layer_id], ps_dyn_range_prms->i2_dyn_max_y);

                    hme_update_dynamic_search_params(
                        aps_dyn_range_prms_thrd0[i4_layer_id], ps_dyn_range_prms->i2_dyn_min_y);
                }
            }
        }
    }

    /*************************************************************************/
    /* Get the MAX/MIN per POC distance based on the all the ref. pics       */
    /*************************************************************************/
    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
    if(ps_thrd0_ctxt->s_frm_prms.is_i_pic == ps_thrd0_ctxt->s_frm_prms.bidir_enabled)
    {
        WORD32 i4_layer_id;
        cur_poc = ps_thrd0_ctxt->i4_curr_poc;

        for(i4_layer_id = coarse_layer_id; i4_layer_id > 0; i4_layer_id--)
        {
            ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id] = 0;
            ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id] = 0;
        }

        for(num_ref = 0; num_ref < i4_num_ref; num_ref++)
        {
            for(i4_layer_id = coarse_layer_id; i4_layer_id > 0; i4_layer_id--)
            {
                WORD16 i2_mv_per_poc;
                WORD32 ref_poc, poc_diff;
                dyn_range_prms_t *ps_dyn_range_prms_thrd0;

                ps_dyn_range_prms_thrd0 =
                    &ps_thrd0_ctxt->s_coarse_dyn_range_prms.as_dyn_range_prms[i4_layer_id][num_ref];

                ref_poc = ps_dyn_range_prms_thrd0->i4_poc;
                ASSERT(ref_poc < cur_poc);
                poc_diff = (cur_poc - ref_poc);

                /* cur. ref. pic. max y per POC */
                i2_mv_per_poc = (ps_dyn_range_prms_thrd0->i2_dyn_max_y + (poc_diff - 1)) / poc_diff;
                /* update the max y per POC */
                ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id] =
                    MAX(ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id],
                        i2_mv_per_poc);

                /* cur. ref. pic. min y per POC */
                i2_mv_per_poc = (ps_dyn_range_prms_thrd0->i2_dyn_min_y - (poc_diff - 1)) / poc_diff;
                /* update the min y per POC */
                ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id] =
                    MIN(ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id],
                        i2_mv_per_poc);
            }
        }

        /*************************************************************************/
        /* Populate the results to all thread ctxt                               */
        /*************************************************************************/
        for(num_thrds = 1; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            for(i4_layer_id = coarse_layer_id; i4_layer_id > 0; i4_layer_id--)
            {
                ps_master_ctxt->aps_me_ctxt[num_thrds]
                    ->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id] =
                    ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_max_y_per_poc[i4_layer_id];

                ps_master_ctxt->aps_me_ctxt[num_thrds]
                    ->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id] =
                    ps_thrd0_ctxt->s_coarse_dyn_range_prms.i2_dyn_min_y_per_poc[i4_layer_id];
            }
        }
    }

    if(ME_XTREME_SPEED_25 != e_me_quality_preset)
    {
        /* call the function which calcualtes the GMV    */
        /* layer pointer is shared across all threads    */
        /* hence all threads will have access to updated */
        /* GMVs populated using thread 0 ctxt            */
        for(num_ref = 0; num_ref < i4_num_ref; num_ref++)
        {
            hme_calculate_global_mv(
                ps_thrd0_ctxt->aps_mv_hist[num_ref],
                &ps_curr_layer->s_global_mv[num_ref][GMV_THICK_LOBE],
                GMV_THICK_LOBE);
        }
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_frame_dpb_update \endif
*
* \brief
*    Frame level ME initialisation function
*
* \par Description:
*   Updation of ME's internal DPB
*    based on available ref list information
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] num_ref_l0 : Number of reference pics in L0 list
* \param[in] num_ref_l1 : Number of reference pics in L1 list
* \param[in] pps_rec_list_l0 : List of recon pics in L0 list
* \param[in] pps_rec_list_l1 : List of recon pics in L1 list
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_frame_dpb_update(
    void *pv_me_ctxt,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1)
{
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    WORD32 a_pocs_buffered_in_me[MAX_NUM_REF + 1];
    WORD32 a_pocs_to_remove[MAX_NUM_REF + 2];
    WORD32 poc_remove_id = 0;
    WORD32 i, count;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd ctxt */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /*************************************************************************/
    /* Updation of ME's DPB list. This involves the following steps:         */
    /* 1. Obtain list of active POCs maintained within ME.                   */
    /* 2. Search each of them in the ref list. Whatever is not found goes to */
    /*     the list to be removed. Note: a_pocs_buffered_in_me holds the     */
    /*    currently active POC list within ME. a_pocs_to_remove holds the    */
    /*    list of POCs to be removed, terminated by -1.                      */
    /*************************************************************************/
    hme_coarse_get_active_pocs_list((void *)ps_thrd0_ctxt, a_pocs_buffered_in_me);

    count = 0;
    while(a_pocs_buffered_in_me[count] != -1)
    {
        WORD32 poc_to_search = a_pocs_buffered_in_me[count];
        WORD32 match_found_flag = 0;

        /*********************************************************************/
        /* Search in any one list (L0/L1) since both lists contain all the   */
        /* active ref pics.                                                  */
        /*********************************************************************/
        for(i = 0; i < num_ref_l0; i++)
        {
            if(poc_to_search == pps_rec_list_l0[i]->i4_poc)
            {
                match_found_flag = 1;
                break;
            }
        }
        for(i = 0; i < num_ref_l1; i++)
        {
            if(poc_to_search == pps_rec_list_l1[i]->i4_poc)
            {
                match_found_flag = 1;
                break;
            }
        }

        if(0 == match_found_flag)
        {
            /*****************************************************************/
            /* POC buffered inside ME but not part of ref list given by DPB  */
            /* Hence this needs to be flagged to ME for removal.             */
            /*****************************************************************/
            a_pocs_to_remove[poc_remove_id] = poc_to_search;
            poc_remove_id++;
        }
        count++;
    }

    /* List termination */
    a_pocs_to_remove[poc_remove_id] = -1;

    /* Call the ME API to remove "outdated" POCs */
    hme_coarse_discard_frm(ps_thrd0_ctxt, a_pocs_to_remove);
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_frame_init \endif
*
* \brief
*    Coarse Frame level ME initialisation function
*
* \par Description:
*    The following pre-conditions exist for this function: a. We have the input
*    pic ready for encode, b. We have the reference list with POC, L0/L1 IDs
*    and ref ptrs ready for this picture and c. ihevce_me_set_resolution has
*    been called atleast once. Once these are supplied, the following are
*    done here: a. Input pyramid creation, b. Updation of ME's internal DPB
*    based on available ref list information
*
* \param[in] pv_ctxt : pointer to ME module
* \param[in] ps_frm_ctb_prms : CTB characteristics parameters
* \param[in] ps_frm_lamda : Frame level Lambda params
* \param[in] num_ref_l0 : Number of reference pics in L0 list
* \param[in] num_ref_l1 : Number of reference pics in L1 list
* \param[in] num_ref_l0_active : Active reference pics in L0 dir for current frame (shall be <= num_ref_l0)
* \param[in] num_ref_l1_active : Active reference pics in L1 dir for current frame (shall be <= num_ref_l1)
* \param[in] pps_rec_list_l0 : List of recon pics in L0 list
* \param[in] pps_rec_list_l1 : List of recon pics in L1 list
* \param[in] ps_enc_lap_inp  : pointer to input yuv buffer (frame buffer)
* \param[in] i4_frm_qp       : current picture QP
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_frame_init(
    void *pv_me_ctxt,
    ihevce_static_cfg_params_t *ps_stat_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    WORD32 i4_frm_qp,
    ihevce_ed_blk_t *ps_layer1_buf,  //EIID
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1,
    UWORD8 *pu1_me_reverse_map_info,
    WORD32 i4_temporal_layer_id)
{
    /* local variables */
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_ctxt;
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    WORD32 inp_poc, num_ref;
    WORD32 i;

    /* Input POC is derived from input buffer */
    inp_poc = ps_enc_lap_inp->s_lap_out.i4_poc;
    num_ref = num_ref_l0 + num_ref_l1;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd 0 ctxt */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    ps_master_ctxt->s_frm_prms.u1_num_active_ref_l0 = num_ref_l0_active;
    ps_master_ctxt->s_frm_prms.u1_num_active_ref_l1 = num_ref_l1_active;

    /* store the frm ctb ctxt to all the thrd ctxt */
    {
        WORD32 num_thrds;

        /* initialise the parameters for all the threads */
        for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];
            ps_ctxt->pv_ext_frm_prms = (void *)ps_frm_ctb_prms;
            /*EIID: early decision buffer pointer */
            ps_ctxt->ps_ed_blk = ps_layer1_buf;
            ps_ctxt->ps_ed_ctb_l1 = ps_ed_ctb_l1;

            /* weighted pred enable flag */
            ps_ctxt->i4_wt_pred_enable_flag = ps_enc_lap_inp->s_lap_out.i1_weighted_pred_flag |
                                              ps_enc_lap_inp->s_lap_out.i1_weighted_bipred_flag;

            if(1 == ps_ctxt->i4_wt_pred_enable_flag)
            {
                /* log2 weight denom  */
                ps_ctxt->s_wt_pred.wpred_log_wdc =
                    ps_enc_lap_inp->s_lap_out.i4_log2_luma_wght_denom;
            }
            else
            {
                /* default value */
                ps_ctxt->s_wt_pred.wpred_log_wdc = DENOM_DEFAULT;
            }
            ps_ctxt->i4_L1_hme_best_cost = 0;
            ps_ctxt->i4_L1_hme_sad = 0;
            ps_ctxt->i4_num_blks_high_sad = 0;
            ps_ctxt->i4_num_blks = 0;

            ps_ctxt->pv_me_optimised_function_list = ps_master_ctxt->pv_me_optimised_function_list;
            ps_ctxt->ps_cmn_utils_optimised_function_list = &ps_master_ctxt->s_cmn_opt_func;
        }
    }
    /* Create the reference map for ME */
    ihevce_me_create_ref_map(
        pps_rec_list_l0,
        pps_rec_list_l1,
        num_ref_l0_active,
        num_ref_l1_active,
        num_ref,
        &ps_master_ctxt->s_ref_map);
    /*************************************************************************/
    /* Call the ME frame level processing for further actiion.               */
    /* ToDo: Support Row Level API.                                          */
    /*************************************************************************/
    ps_master_ctxt->s_frm_prms.i2_mv_range_x = ps_thrd0_ctxt->s_init_prms.max_horz_search_range;
    ps_master_ctxt->s_frm_prms.i2_mv_range_y = ps_thrd0_ctxt->s_init_prms.max_vert_search_range;

    ps_master_ctxt->s_frm_prms.is_i_pic = 0;
    ps_master_ctxt->s_frm_prms.i4_temporal_layer_id = i4_temporal_layer_id;

    ps_master_ctxt->s_frm_prms.is_pic_second_field =
        (!(ps_enc_lap_inp->s_input_buf.i4_bottom_field ^
           ps_enc_lap_inp->s_input_buf.i4_topfield_first));
    {
        S32 pic_type = ps_enc_lap_inp->s_lap_out.i4_pic_type;

        /*********************************************************************/
        /* For I Pic, we do not call update fn at ctb level, instead we do   */
        /* one shot update for entire picture.                               */
        /*********************************************************************/
        if((pic_type == IV_I_FRAME) || (pic_type == IV_II_FRAME) || (pic_type == IV_IDR_FRAME))
        {
            ps_master_ctxt->s_frm_prms.is_i_pic = 1;
            ps_master_ctxt->s_frm_prms.bidir_enabled = 0;
        }
        else if((pic_type == IV_P_FRAME) || (pic_type == IV_PP_FRAME))
        {
            ps_master_ctxt->s_frm_prms.bidir_enabled = 0;
        }
        else if((pic_type == IV_B_FRAME) || (pic_type == IV_BB_FRAME))
        {
            ps_master_ctxt->s_frm_prms.bidir_enabled = 1;
        }
        else
        {
            /* not sure whether we need to handle mixed frames like IP, */
            /* they should ideally come as single field. */
            /* TODO : resolve thsi ambiguity */
            ASSERT(0);
        }
    }
    /************************************************************************/
    /* Lambda calculations moved outside ME and to one place, so as to have */
    /* consistent lambda across ME, IPE, CL RDOPT etc                       */
    /************************************************************************/

    {
#define CLIP3_F(min, max, val) (((val) < (min)) ? (min) : (((val) > (max)) ? (max) : (val)))
        double q_steps[6] = { 0.625, 0.703, 0.79, 0.889, 1.0, 1.125 };
        double d_b_pic_factor;
        double d_q_factor;
        //double d_lambda;
        UWORD8 u1_temp_hier = ps_enc_lap_inp->s_lap_out.i4_temporal_lyr_id;

        if(u1_temp_hier)
        {
            d_b_pic_factor = CLIP3_F(2.0, 4.0, (i4_frm_qp - 12.0) / 6.0);
        }
        else
            d_b_pic_factor = 1.0;

        d_q_factor = (1 << (i4_frm_qp / 6)) * q_steps[i4_frm_qp % 6];
        ps_master_ctxt->s_frm_prms.qstep = (WORD32)d_q_factor;
        ps_master_ctxt->s_frm_prms.i4_frame_qp = i4_frm_qp;
    }

    /* HME Dependency Manager : Reset the num ctb processed in every row */
    /* for ME sync in every layer                                        */
    {
        WORD32 ctr;
        for(ctr = 1; ctr < ps_thrd0_ctxt->num_layers; ctr++)
        {
            void *pv_dep_mngr_state;
            pv_dep_mngr_state = ps_master_ctxt->apv_dep_mngr_hme_sync[ctr - 1];

            ihevce_dmgr_rst_row_row_sync(pv_dep_mngr_state);
        }
    }

    /* Frame level init of all threads of ME */
    {
        WORD32 num_thrds;

        /* initialise the parameters for all the threads */
        for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            ps_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            hme_coarse_process_frm_init(
                (void *)ps_ctxt, ps_ctxt->ps_hme_ref_map, ps_ctxt->ps_hme_frm_prms);
        }
    }

    ps_master_ctxt->s_frm_prms.i4_cl_sad_lambda_qf = ps_frm_lamda->i4_cl_sad_lambda_qf;
    ps_master_ctxt->s_frm_prms.i4_cl_satd_lambda_qf = ps_frm_lamda->i4_cl_satd_lambda_qf;
    ps_master_ctxt->s_frm_prms.i4_ol_sad_lambda_qf = ps_frm_lamda->i4_ol_sad_lambda_qf;
    ps_master_ctxt->s_frm_prms.i4_ol_satd_lambda_qf = ps_frm_lamda->i4_ol_satd_lambda_qf;
    ps_master_ctxt->s_frm_prms.lambda_q_shift = LAMBDA_Q_SHIFT;

    ps_master_ctxt->s_frm_prms.pf_interp_fxn = NULL;

    /*************************************************************************/
    /* If num ref is 0, that means that it has to be coded as I. Do nothing  */
    /* However mv bank update needs to happen with "intra" mv.               */
    /*************************************************************************/
    if(ps_master_ctxt->s_ref_map.i4_num_ref == 0 || ps_master_ctxt->s_frm_prms.is_i_pic)
    {
        for(i = 1; i < ps_thrd0_ctxt->num_layers; i++)
        {
            layer_ctxt_t *ps_layer_ctxt = ps_thrd0_ctxt->ps_curr_descr->aps_layers[i];
            BLK_SIZE_T e_blk_size;
            S32 use_4x4;

            /* The mv bank is filled with "intra" mv */
            use_4x4 = hme_get_mv_blk_size(
                ps_thrd0_ctxt->s_init_prms.use_4x4,
                i,
                ps_thrd0_ctxt->num_layers,
                ps_thrd0_ctxt->u1_encode[i]);
            e_blk_size = use_4x4 ? BLK_4x4 : BLK_8x8;
            hme_init_mv_bank(ps_layer_ctxt, e_blk_size, 2, 1, ps_ctxt->u1_encode[i]);
            hme_fill_mvbank_intra(ps_layer_ctxt);

            /* Clear out the global mvs */
            memset(
                ps_layer_ctxt->s_global_mv,
                0,
                sizeof(hme_mv_t) * ps_thrd0_ctxt->max_num_ref * NUM_GMV_LOBES);
        }

        return;
    }

    /*************************************************************************/
    /* Coarse & refine Layer frm init (layer mem is common across thrds)     */
    /*************************************************************************/
    {
        coarse_prms_t s_coarse_prms;
        refine_prms_t s_refine_prms;
        S16 i2_max;
        S32 layer_id;

        layer_id = ps_thrd0_ctxt->num_layers - 1;
        i2_max = ps_thrd0_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_x;
        i2_max = MAX(i2_max, ps_thrd0_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_y);
        s_coarse_prms.i4_layer_id = layer_id;

        {
            S32 log_start_step;
            /* Based on Preset, set the starting step size for Refinement */
            if(ME_MEDIUM_SPEED > ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets)
            {
                log_start_step = 0;
            }
            else
            {
                log_start_step = 1;
            }
            s_coarse_prms.i4_max_iters = i2_max >> log_start_step;
            s_coarse_prms.i4_start_step = 1 << log_start_step;
        }
        s_coarse_prms.i4_num_ref = ps_master_ctxt->s_ref_map.i4_num_ref;
        s_coarse_prms.do_full_search = 1;
        s_coarse_prms.num_results = ps_thrd0_ctxt->max_num_results_coarse;

        hme_coarse_frm_init(ps_thrd0_ctxt, &s_coarse_prms);

        layer_id--;

        /*************************************************************************/
        /* This loop will run for all refine layers (non- encode layers)          */
        /*************************************************************************/
        while(layer_id > 0)
        {
            layer_ctxt_t *ps_curr_layer;
            layer_ctxt_t *ps_coarse_layer;

            ps_coarse_layer = ps_thrd0_ctxt->ps_curr_descr->aps_layers[layer_id + 1];

            ps_curr_layer = ps_thrd0_ctxt->ps_curr_descr->aps_layers[layer_id];

            hme_set_refine_prms(
                &s_refine_prms,
                ps_thrd0_ctxt->u1_encode[layer_id],
                ps_master_ctxt->s_ref_map.i4_num_ref,
                layer_id,
                ps_thrd0_ctxt->num_layers,
                ps_thrd0_ctxt->num_layers_explicit_search,
                ps_thrd0_ctxt->s_init_prms.use_4x4,
                &ps_master_ctxt->s_frm_prms,
                NULL,
                &ps_thrd0_ctxt->s_init_prms.s_me_coding_tools);

            hme_refine_frm_init(ps_curr_layer, &s_refine_prms, ps_coarse_layer);

            layer_id--;
        }
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_decomp_pre_intra_frame_init \endif
*
* \brief
*    Frame Intialization for Decomp intra pre analysis.
*
* \param[in] pv_ctxt : pointer to module ctxt
* \param[in] ppu1_decomp_lyr_bufs : pointer to array of layer buffer pointers
* \param[in] pi4_lyr_buf_stride : pointer to array of layer buffer strides
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_coarse_me_get_lyr_buf_desc(
    void *pv_me_ctxt, UWORD8 **ppu1_decomp_lyr_bufs, WORD32 *pi4_lyr_buf_stride)
{
    /* local variables */
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    WORD32 lyr_no;
    layers_descr_t *ps_curr_descr;
    WORD32 i4_free_idx;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd0  ctxt */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* Obtain an empty layer descriptor */
    i4_free_idx = hme_coarse_find_free_descr_idx((void *)ps_thrd0_ctxt);

    ps_curr_descr = &ps_thrd0_ctxt->as_ref_descr[i4_free_idx];

    /* export all the layer buffers except Layer 0 (encode layer) */
    for(lyr_no = 1; lyr_no < ps_thrd0_ctxt->num_layers; lyr_no++)
    {
        pi4_lyr_buf_stride[lyr_no - 1] = ps_curr_descr->aps_layers[lyr_no]->i4_inp_stride;
        ppu1_decomp_lyr_bufs[lyr_no - 1] = ps_curr_descr->aps_layers[lyr_no]->pu1_inp;
    }

    return (i4_free_idx);
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_lyr_prms_job_que \endif
*
* \brief Returns to the caller key attributes related to dependency between layers
*          for multi-thread execution
*
*
* \par Description:
*    This function requires the precondition that the width and ht of encode
*    layer is known, and ME API ihevce_me_set_resolution() API called with
*    this info. Based on this, ME populates useful information for the encoder
*    to execute the multi-thread (concurrent across layers) in this API.
*    The number of layers, number of vertical units in each layer, and for
*    each vertial unit in each layer, its dependency on previous layer's units
*    From ME's perspective, a vertical unit is one which is smallest min size
*    vertically (and spans the entire row horizontally). This is CTB for encode
*    layer, and 8x8 / 4x4 for non encode layers.
*
* \param[in] pv_ctxt : ME handle
* \param[in] ps_curr_inp : Input buffer descriptor
* \param[out] pi4_num_hme_lyrs : Num of HME layers (ME updates)
* \param[out] pi4_num_vert_units_in_lyr : Array of size N (num layers), each
*                     entry has num vertical units in that particular layer
* \param[in] ps_me_job_q_prms : Array of job queue prms, one for each unit in a
*                 layer. Note that this is contiguous in order of processing
*                 All k units of layer N-1 from top to bottom, followed by
*                 all m units of layer N-2 .... ends with X units of layer 0
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_get_lyr_prms_job_que(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    WORD32 *pi4_num_hme_lyrs,
    WORD32 *pi4_num_vert_units_in_lyr,
    multi_thrd_me_job_q_prms_t *ps_me_job_q_prms)
{
    coarse_me_ctxt_t *ps_ctxt;
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;

    /* These arrays and ptrs track input dependencies for units of a layer */
    /* This is a ping poing design, while using one part, we update other part */
    U08 au1_inp_dep[2][MAX_NUM_VERT_UNITS_FRM];
    U08 *pu1_inp_dep_c, *pu1_inp_dep_n;

    /* Height of current and next layers */
    S32 ht_c, ht_n;

    /* Blk ht at a given layer and next layer*/
    S32 unit_ht_c, unit_ht_n, blk_ht_c, blk_ht_n;

    /* Number of vertical units in current and next layer */
    S32 num_vert_c, num_vert_n;

    S32 ctb_size = 64, num_layers, i, j, k;

    /* since same layer desc pointer is stored in all thread ctxt */
    /* a free idx is obtained using 0th thread ctxt pointer */
    ps_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* Set the number of layers */
    num_layers = ps_ctxt->num_layers;
    *pi4_num_hme_lyrs = num_layers;

    pu1_inp_dep_c = &au1_inp_dep[0][0];
    pu1_inp_dep_n = &au1_inp_dep[1][0];

    ASSERT(num_layers >= 2);

    ht_n = ps_ctxt->a_ht[num_layers - 2];
    ht_c = ps_ctxt->a_ht[num_layers - 1];

    /* compute blk ht and unit ht for c and n */
    if(ps_ctxt->u1_encode[num_layers - 1])
    {
        blk_ht_c = 16;
        unit_ht_c = ctb_size;
    }
    else
    {
        blk_ht_c = hme_get_blk_size(ps_ctxt->s_init_prms.use_4x4, num_layers - 1, num_layers, 0);
        unit_ht_c = blk_ht_c;
    }

    num_vert_c = (ht_c + unit_ht_c - 1) / unit_ht_c;

    /* For new design in Coarsest HME layer we need */
    /* one additional row extra at the end of frame */
    /* hence num_vert_c is incremented by 1         */
    num_vert_c++;

    /* Dummy initialization outside loop, not used first time */
    memset(pu1_inp_dep_c, 0, num_vert_c);

    /*************************************************************************/
    /* Run through each layer, set the number of vertical units and job queue*/
    /* attrs for each vert unit in the layer                                 */
    /*************************************************************************/
    for(i = num_layers - 1; i > 0; i--)
    {
        /* 0th entry is actually layer id num_layers - 1 */
        /* and entry num_layers-1 equals the biggest layer (id = 0) */
        pi4_num_vert_units_in_lyr[num_layers - 1 - i] = num_vert_c;
        /* "n" is computed for first time */
        ht_n = ps_ctxt->a_ht[i - 1];
        blk_ht_n = hme_get_blk_size(ps_ctxt->s_init_prms.use_4x4, i - 1, num_layers, 0);
        unit_ht_n = blk_ht_n;
        if(ps_ctxt->u1_encode[i - 1])
            unit_ht_n = ctb_size;

        num_vert_n = (ht_n + unit_ht_n - 1) / unit_ht_n;
        /* Initialize all units' inp dep in next layer to 0 */
        memset(pu1_inp_dep_n, 0, num_vert_n * sizeof(U08));

        /* Evaluate dependencies for this layer */
        for(j = 0; j < num_vert_c; j++)
        {
            S32 v1, v2;

            /* Output dependencies. When one unit in current layer finishes, */
            /* how many in the next layer it affects?. Assuming that the top */
            /* of this vertical unit and bottom of this vertical unit project*/
            /* somewhere in the next layer. The top of this vertical unit    */
            /* becomes the bottom right point for somebody, and the bottom of*/
            /* this vertical unit becomes the colocated pt for somebody, this*/
            /* is the extremum.                                              */

            /* for the initial unit affected by j in "c" layer, take j-1th   */
            /* unit top and project it.                                      */
            v1 = (j - 1) * unit_ht_c * ht_n;
            v1 /= (ht_c * unit_ht_n);
            v1 -= 1;

            /* for the final unit affected by j in "c" layer, take jth unit  */
            /* bottom and project it.                                        */

            v2 = (j + 1) * unit_ht_c * ht_n;
            v2 /= (ht_c * unit_ht_n);
            v2 += 1;

            /* Clip to be within valid limits */
            v1 = HME_CLIP(v1, 0, (num_vert_n - 1));
            v2 = HME_CLIP(v2, 0, (num_vert_n - 1));

            /* In the layer "n", units starting at offset v1, and upto v2 are*/
            /* dependent on unit j of layer "c". So for each of these units  */
            /* increment the dependency by 1 corresponding to "jth" unit in  */
            /* layer "c"                                                     */
            ps_me_job_q_prms->i4_num_output_dep = v2 - v1 + 1;
            ASSERT(ps_me_job_q_prms->i4_num_output_dep <= MAX_OUT_DEP);
            for(k = v1; k <= v2; k++)
                pu1_inp_dep_n[k]++;

            /* Input dependency would have been calculated in prev run */
            ps_me_job_q_prms->i4_num_inp_dep = pu1_inp_dep_c[j];
            ASSERT(ps_me_job_q_prms->i4_num_inp_dep <= MAX_OUT_DEP);

            /* Offsets */
            for(k = v1; k <= v2; k++)
                ps_me_job_q_prms->ai4_out_dep_unit_off[k - v1] = k;

            ps_me_job_q_prms++;
        }

        /* Compute the blk size and vert unit size in each layer             */
        /* "c" denotes curr layer, and "n" denotes the layer to which result */
        /* is projected to                                                   */
        ht_c = ht_n;
        blk_ht_c = blk_ht_n;
        unit_ht_c = unit_ht_n;
        num_vert_c = num_vert_n;

        /* Input dep count for next layer was computed this iteration. */
        /* Swap so that p_inp_dep_n becomes current for next iteration, */
        /* and p_inp_dep_c will become update area during next iteration */
        /* for next to next.                                             */
        {
            U08 *pu1_tmp = pu1_inp_dep_n;
            pu1_inp_dep_n = pu1_inp_dep_c;
            pu1_inp_dep_c = pu1_tmp;
        }
    }

    /* LAYER 0 OR ENCODE LAYER UPDATE : NO OUTPUT DEPS */

    /* set the numebr of vertical units */
    pi4_num_vert_units_in_lyr[num_layers - 1] = num_vert_c;
    for(j = 0; j < num_vert_c; j++)
    {
        /* Here there is no output dependency for ME. However this data is used for encode, */
        /* and there is a 1-1 correspondence between this and the encode     */
        /* Hence we set output dependency of 1 */
        ps_me_job_q_prms->i4_num_output_dep = 1;
        ps_me_job_q_prms->ai4_out_dep_unit_off[0] = j;
        ps_me_job_q_prms->i4_num_inp_dep = pu1_inp_dep_c[j];
        ASSERT(ps_me_job_q_prms->i4_num_inp_dep <= MAX_OUT_DEP);
        ps_me_job_q_prms++;
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_set_lyr1_mv_bank \endif
*
* \brief
*    Frame level ME initialisation of MV bank of penultimate layer
*
* \par Description:
*    Updates the Layer1 context with the given buffers
*
* \param[in] pv_me_ctxt : pointer to ME module
* \param[in] pu1_mv_bank : MV bank buffer pointer
* \param[in] pu1_ref_idx_bank : refrence bank buffer pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_set_lyr1_mv_bank(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    void *pv_mv_bank,
    void *pv_ref_idx_bank,
    WORD32 i4_curr_idx)
{
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    layer_ctxt_t *ps_lyr1_ctxt;

    /* Input descriptor that is updated and passed to ME */
    hme_inp_desc_t s_inp_desc;

    /*************************************************************************/
    /* Add the current input to ME's DPB. This will also create the pyramids */
    /* for the HME layers tha are not "encoded".                             */
    /*************************************************************************/
    s_inp_desc.i4_poc = ps_enc_lap_inp->s_lap_out.i4_poc;
    s_inp_desc.s_layer_desc[0].pu1_y = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_y_buf;
    s_inp_desc.s_layer_desc[0].pu1_u = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_u_buf;
    s_inp_desc.s_layer_desc[0].pu1_v = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_v_buf;

    s_inp_desc.s_layer_desc[0].luma_stride = ps_enc_lap_inp->s_lap_out.s_input_buf.i4_y_strd;
    s_inp_desc.s_layer_desc[0].chroma_stride = ps_enc_lap_inp->s_lap_out.s_input_buf.i4_uv_strd;

    hme_coarse_add_inp(pv_me_ctxt, &s_inp_desc, i4_curr_idx);

    /* All processing done using shared / common memory across */
    /* threads is done using thrd 0 ctxt since layer ctxt is shared accross all threads */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    ps_lyr1_ctxt = ps_thrd0_ctxt->ps_curr_descr->aps_layers[1];

    /* register the mv bank & ref idx bank pointer */
    ps_lyr1_ctxt->ps_layer_mvbank->pi1_ref_idx_base = (S08 *)pv_ref_idx_bank;
    ps_lyr1_ctxt->ps_layer_mvbank->ps_mv_base = (hme_mv_t *)pv_mv_bank;

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_coarse_me_get_lyr1_ctxt \endif
*
* \brief
*    function to get teh Layer 1 properties to be passed on the encode layer
*
* \par Description:
*    Ucopies the enitre layer ctxt emory to the destination
*
* \param[in] pv_me_ctxt : pointer to ME module
* \param[in] pu1_mv_bank : MV bank buffer pointer
* \param[in] pu1_ref_idx_bank : refrence bank buffer pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_coarse_me_get_lyr1_ctxt(
    void *pv_me_ctxt, void *pv_layer_ctxt, void *pv_layer_mv_bank_ctxt)
{
    coarse_me_ctxt_t *ps_thrd0_ctxt;
    coarse_me_master_ctxt_t *ps_master_ctxt = (coarse_me_master_ctxt_t *)pv_me_ctxt;
    layer_ctxt_t *ps_lyr1_ctxt;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd 0 ctxt since layer ctxt is shared accross all threads */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];

    /* get the context of layer 1 */
    ps_lyr1_ctxt = ps_thrd0_ctxt->ps_curr_descr->aps_layers[1];

    /* copy the layer ctxt eve registerd mv bank & ref idx bank also goes in */
    memcpy(pv_layer_ctxt, ps_lyr1_ctxt, sizeof(layer_ctxt_t));

    /* copy the layer mv bank contents */
    memcpy(pv_layer_mv_bank_ctxt, ps_lyr1_ctxt->ps_layer_mvbank, sizeof(layer_mv_t));

    /* register the MV bank pointer in the layer ctxt*/
    ((layer_ctxt_t *)pv_layer_ctxt)->ps_layer_mvbank = (layer_mv_t *)pv_layer_mv_bank_ctxt;

    return;
}
