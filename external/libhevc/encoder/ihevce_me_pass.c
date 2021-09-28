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
* \file ihevce_me_pass.c
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

#include "ihevc_debug.h"
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
#include "ihevce_inter_pred.h"

#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "ihevce_me_instr_set_router.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_refine.h"
#include "hme_function_selector.h"
#include "ihevce_me_pass.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

/** orig simple five tap scaler */
#define FIVE_TAP_ORIG_SCALER 0

/** simple gaussian filter, blurs the image a bit */
#define SIMPLE_GAUSSIAN_SCALER 0

/** lanczos scaler gives sharper images           */
#define LANCZOS_SCALER 1

// Saturated addition z = x + y
// overflow condition: z<x or z<y
#define SATURATED_ADD(z, x, y)                                                                     \
    {                                                                                              \
        (z) = (x) + (y);                                                                           \
        if(((z) < (x)) || ((z) < (y)))                                                             \
            (z) = MAX_INTRA_COST_IPE;                                                              \
    }

#define SATURATED_SUB(z, x, y)                                                                     \
    {                                                                                              \
        (z) = (x) - (y);                                                                           \
        if((z) < 0) /*if (((z) > (x)) || ((z) > (y))) */                                           \
            (z) = 0;                                                                               \
    }

#if(FIVE_TAP_ORIG_SCALER + SIMPLE_GAUSSIAN_SCALER + LANCZOS_SCALER) > 1
#error "HME ERROR: Only one scaler can be enabled at a time"
#endif

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_me_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for ME module
*    Note : Include TOT MEM. req. for ME + TOT MEM. req. for Dep Mngr for L0 ME
*
* \return
*    Number of memory records
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_me_get_num_mem_recs(WORD32 i4_num_me_frm_pllel)
{
    WORD32 me_mem_recs = hme_enc_num_alloc(i4_num_me_frm_pllel);

    return (me_mem_recs);
}

void ihevce_derive_me_init_prms(
    ihevce_static_cfg_params_t *ps_init_prms,
    hme_init_prms_t *ps_hme_init_prms,
    S32 i4_num_proc_thrds,
    S32 i4_resolution_id)
{
    WORD32 i4_field_pic = ps_init_prms->s_src_prms.i4_field_pic;
    WORD32 min_cu_size;

    /* max number of ref frames. This should be > ref frms sent any frm */
    ps_hme_init_prms->max_num_ref = ((DEFAULT_MAX_REFERENCE_PICS) << i4_field_pic);

    /* get the min cu size from config params */
    min_cu_size = ps_init_prms->s_config_prms.i4_min_log2_cu_size;

    min_cu_size = 1 << min_cu_size;

    /* Width and height for the layer being encoded */
    ps_hme_init_prms->a_wd[0] =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
        SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, min_cu_size);

    ps_hme_init_prms->a_ht[0] =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, min_cu_size);

    /* we store 4 results in coarsest layer per blk. 8x4L, 8x4R, 4x8T, 4x8B */
    ps_hme_init_prms->max_num_results_coarse = 4;

    /* Every refinement layer stores a max of 2 results per partition */
    ps_hme_init_prms->max_num_results = 2;

    /* Assuming abt 4 layers for 1080p, we do explicit search across all ref */
    /* frames in all but final layer In final layer, it could be 1/2 */
    ps_hme_init_prms->num_layers_explicit_search = 3;

    /* Populate the max_tr_depth for Inter */
    ps_hme_init_prms->u1_max_tr_depth = ps_init_prms->s_config_prms.i4_max_tr_tree_depth_nI;

    ps_hme_init_prms->log_ctb_size = ps_init_prms->s_config_prms.i4_max_log2_cu_size;
    ASSERT(ps_hme_init_prms->log_ctb_size == 6);

    /* currently encoding only 1 layer */
    ps_hme_init_prms->num_simulcast_layers = 1;

    /* this feature not yet supported */
    ps_hme_init_prms->segment_higher_layers = 0;

    /* Allow 4x4 in refinement layers. Unconditionally enabled in coarse lyr */
    /* And not enabled in encode layers, this is just for intermediate refine*/
    /* layers, where it could be used for better accuracy of motion.         */

#if !OLD_XTREME_SPEED
    if((IHEVCE_QUALITY_P6 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset) ||
       (IHEVCE_QUALITY_P7 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset) ||
       (IHEVCE_QUALITY_P5 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset) ||
       (IHEVCE_QUALITY_P4 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset))
        ps_hme_init_prms->use_4x4 = 0;
    else
        ps_hme_init_prms->use_4x4 = 1;
#else
    ps_hme_init_prms->use_4x4 = 1;
#endif

    ps_hme_init_prms->num_b_frms =
        (1 << ps_init_prms->s_coding_tools_prms.i4_max_temporal_layers) - 1;

    ps_hme_init_prms->i4_num_proc_thrds = i4_num_proc_thrds;

    if(IHEVCE_QUALITY_P0 ==
       ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_PRISTINE_QUALITY;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 3;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 3;
    }
    else if(
        IHEVCE_QUALITY_P2 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_HIGH_QUALITY;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 3;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 3;
    }
    else if(
        IHEVCE_QUALITY_P3 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_MEDIUM_SPEED;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 2;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 2;
    }
    else if(
        IHEVCE_QUALITY_P4 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_HIGH_SPEED;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 1;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 1;
    }
    else if(
        IHEVCE_QUALITY_P5 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_XTREME_SPEED;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 1;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 1;
    }
    else if(
        IHEVCE_QUALITY_P6 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_XTREME_SPEED_25;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 1;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 1;
    }
    else if(
        IHEVCE_QUALITY_P7 ==
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset)
    {
        ps_hme_init_prms->s_me_coding_tools.e_me_quality_presets = ME_XTREME_SPEED_25;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_hpel_refine = 1;
        ps_hme_init_prms->s_me_coding_tools.i4_num_steps_qpel_refine = 0;
    }

    ps_hme_init_prms->s_me_coding_tools.u1_l0_me_controlled_via_cmd_line = 0;

    /* Register the search range params from static params */
    ps_hme_init_prms->max_horz_search_range = ps_init_prms->s_config_prms.i4_max_search_range_horz;
    ps_hme_init_prms->max_vert_search_range = ps_init_prms->s_config_prms.i4_max_search_range_vert;
    ps_hme_init_prms->e_arch_type = ps_init_prms->e_arch_type;
    ps_hme_init_prms->is_interlaced = (ps_init_prms->s_src_prms.i4_field_pic == IV_INTERLACED);

    ps_hme_init_prms->u1_is_stasino_enabled =
        ((ps_init_prms->s_coding_tools_prms.i4_vqet &
          (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
         (ps_init_prms->s_coding_tools_prms.i4_vqet &
          (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)));
}

/*!
******************************************************************************
* \if Function name : ihevce_me_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for ME.
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
WORD32 ihevce_me_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id,
    WORD32 i4_num_me_frm_pllel)
{
    hme_memtab_t as_memtabs[MAX_HME_ENC_TOT_MEMTABS];
    WORD32 n_tabs, i;

    /* Init prms structure specific to HME */
    hme_init_prms_t s_hme_init_prms;

    /*************************************************************************/
    /* code flow: we call hme alloc function and then remap those memtabs    */
    /* to a different type of memtab structure.                              */
    /*************************************************************************/
    if(i4_num_me_frm_pllel > 1)
    {
        ASSERT(MAX_HME_ENC_TOT_MEMTABS >= hme_enc_num_alloc(i4_num_me_frm_pllel));
    }
    else
    {
        ASSERT(MIN_HME_ENC_TOT_MEMTABS >= hme_enc_num_alloc(i4_num_me_frm_pllel));
    }

    /*************************************************************************/
    /* POPULATE THE HME INIT PRMS                                            */
    /*************************************************************************/
    ihevce_derive_me_init_prms(ps_init_prms, &s_hme_init_prms, i4_num_proc_thrds, i4_resolution_id);

    /*************************************************************************/
    /* CALL THE ME FUNCTION TO GET MEMTABS                                   */
    /*************************************************************************/
    n_tabs = hme_enc_alloc(&as_memtabs[0], &s_hme_init_prms, i4_num_me_frm_pllel);
    ASSERT(n_tabs == hme_enc_num_alloc(i4_num_me_frm_pllel));

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
    /* --- L0 ME sync Dep Mngr Mem requests --                               */
    /*************************************************************************/
    ps_mem_tab += n_tabs;

    return (n_tabs);
}

/*!
******************************************************************************
* \if Function name : ihevce_me_init \endif
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
void *ihevce_me_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    rc_quant_t *ps_rc_quant_ctxt,
    void *pv_tile_params_base,
    WORD32 i4_resolution_id,
    WORD32 i4_num_me_frm_pllel,
    UWORD8 u1_is_popcnt_available)
{
    /* ME handle to be returned */
    void *pv_me_ctxt;
    WORD32 status;
    me_master_ctxt_t *ps_me_ctxt;
    IV_ARCH_T e_arch_type;

    /* Init prms structure specific to HME */
    hme_init_prms_t s_hme_init_prms;

    /* memtabs to be passed to hme */
    hme_memtab_t as_memtabs[MAX_HME_ENC_TOT_MEMTABS];
    WORD32 n_tabs, i;

    /*************************************************************************/
    /* POPULATE THE HME INIT PRMS                                            */
    /*************************************************************************/
    ihevce_derive_me_init_prms(ps_init_prms, &s_hme_init_prms, i4_num_proc_thrds, i4_resolution_id);

    /*************************************************************************/
    /* Ensure local declaration is sufficient                                */
    /*************************************************************************/
    n_tabs = hme_enc_num_alloc(i4_num_me_frm_pllel);

    if(i4_num_me_frm_pllel > 1)
    {
        ASSERT(MAX_HME_ENC_TOT_MEMTABS >= n_tabs);
    }
    else
    {
        ASSERT(MIN_HME_ENC_TOT_MEMTABS >= n_tabs);
    }

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
    ps_me_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    /* Store Tile params base into ME context */
    ps_me_ctxt->pv_tile_params_base = pv_tile_params_base;

    status = hme_enc_init(
        pv_me_ctxt, &as_memtabs[0], &s_hme_init_prms, ps_rc_quant_ctxt, i4_num_me_frm_pllel);

    if(status == -1)
        return NULL;

    /*************************************************************************/
    /* --- L0 ME sync Dep Mngr Mem init --                                     */
    /*************************************************************************/
    /* Update numer of ME frames running in parallel in me master context */
    ps_me_ctxt->i4_num_me_frm_pllel = i4_num_me_frm_pllel;

    e_arch_type = ps_init_prms->e_arch_type;

    hme_init_function_ptr(ps_me_ctxt, e_arch_type);

    ihevce_me_instr_set_router(
        (ihevce_me_optimised_function_list_t *)ps_me_ctxt->pv_me_optimised_function_list,
        e_arch_type);

    ihevce_cmn_utils_instr_set_router(
        &ps_me_ctxt->s_cmn_opt_func, u1_is_popcnt_available, e_arch_type);

    ps_mem_tab += n_tabs;

    return (pv_me_ctxt);
}

/**
*******************************************************************************
* \if Function name : ihevce_me_set_resolution \endif
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
void ihevce_me_set_resolution(void *pv_me_ctxt, WORD32 n_enc_layers, WORD32 *p_wd, WORD32 *p_ht)
{
    /* local variables */
    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    WORD32 thrds;
    WORD32 i;

    for(thrds = 0; thrds < ps_master_ctxt->i4_num_proc_thrds; thrds++)
    {
        me_ctxt_t *ps_me_thrd_ctxt;

        ps_me_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[thrds];

        for(i = 0; i < MAX_NUM_ME_PARALLEL; i++)
        {
            hme_set_resolution((void *)ps_me_thrd_ctxt, n_enc_layers, p_wd, p_ht, i);
        }
    }
}

void ihevce_populate_me_ctb_data(
    me_ctxt_t *ps_ctxt,
    me_frm_ctxt_t *ps_frm_ctxt,
    cur_ctb_cu_tree_t *ps_cu_tree,
    me_ctb_data_t *ps_me_ctb_data,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos)
{
    inter_cu_results_t *ps_cu_results;

    switch(ps_cu_tree->u1_cu_size)
    {
    case 64:
    {
        block_data_64x64_t *ps_data = &ps_me_ctb_data->s_64x64_block_data;

        ps_cu_results = &ps_frm_ctxt->s_cu64x64_results;
        ps_data->num_best_results = (ps_cu_tree->is_node_valid) ? ps_cu_results->u1_num_best_results
                                                                : 0;

        break;
    }
    case 32:
    {
        block_data_32x32_t *ps_data = &ps_me_ctb_data->as_32x32_block_data[e_cur_blk_pos];

        ps_cu_results = &ps_frm_ctxt->as_cu32x32_results[e_cur_blk_pos];
        ps_data->num_best_results = (ps_cu_tree->is_node_valid) ? ps_cu_results->u1_num_best_results
                                                                : 0;

        break;
    }
    case 16:
    {
        WORD32 i4_blk_id = e_cur_blk_pos + (e_parent_blk_pos << 2);

        block_data_16x16_t *ps_data = &ps_me_ctb_data->as_block_data[i4_blk_id];

        ps_cu_results = &ps_frm_ctxt->as_cu16x16_results[i4_blk_id];
        ps_data->num_best_results = (ps_cu_tree->is_node_valid) ? ps_cu_results->u1_num_best_results
                                                                : 0;

        break;
    }
    case 8:
    {
        WORD32 i4_blk_id = e_cur_blk_pos + (e_parent_blk_pos << 2) + (e_grandparent_blk_pos << 4);

        block_data_8x8_t *ps_data = &ps_me_ctb_data->as_8x8_block_data[i4_blk_id];

        ps_cu_results = &ps_frm_ctxt->as_cu8x8_results[i4_blk_id];
        ps_data->num_best_results = (ps_cu_tree->is_node_valid) ? ps_cu_results->u1_num_best_results
                                                                : 0;

        break;
    }
    }

    if(ps_cu_tree->is_node_valid)
    {
        if((ps_ctxt->s_init_prms.s_me_coding_tools.e_me_quality_presets == ME_PRISTINE_QUALITY) &&
           (ps_cu_tree->u1_cu_size != 8))
        {
            ihevce_populate_me_ctb_data(
                ps_ctxt,
                ps_frm_ctxt,
                ps_cu_tree->ps_child_node_tl,
                ps_me_ctb_data,
                e_parent_blk_pos,
                e_cur_blk_pos,
                POS_TL);

            ihevce_populate_me_ctb_data(
                ps_ctxt,
                ps_frm_ctxt,
                ps_cu_tree->ps_child_node_tr,
                ps_me_ctb_data,
                e_parent_blk_pos,
                e_cur_blk_pos,
                POS_TR);

            ihevce_populate_me_ctb_data(
                ps_ctxt,
                ps_frm_ctxt,
                ps_cu_tree->ps_child_node_bl,
                ps_me_ctb_data,
                e_parent_blk_pos,
                e_cur_blk_pos,
                POS_BL);

            ihevce_populate_me_ctb_data(
                ps_ctxt,
                ps_frm_ctxt,
                ps_cu_tree->ps_child_node_br,
                ps_me_ctb_data,
                e_parent_blk_pos,
                e_cur_blk_pos,
                POS_BR);
        }
    }
    else if(ps_cu_tree->u1_cu_size != 8)
    {
        ihevce_populate_me_ctb_data(
            ps_ctxt,
            ps_frm_ctxt,
            ps_cu_tree->ps_child_node_tl,
            ps_me_ctb_data,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TL);

        ihevce_populate_me_ctb_data(
            ps_ctxt,
            ps_frm_ctxt,
            ps_cu_tree->ps_child_node_tr,
            ps_me_ctb_data,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TR);

        ihevce_populate_me_ctb_data(
            ps_ctxt,
            ps_frm_ctxt,
            ps_cu_tree->ps_child_node_bl,
            ps_me_ctb_data,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BL);

        ihevce_populate_me_ctb_data(
            ps_ctxt,
            ps_frm_ctxt,
            ps_cu_tree->ps_child_node_br,
            ps_me_ctb_data,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BR);
    }
}

void ihevce_me_update_ctb_results(
    void *pv_me_ctxt, void *pv_me_frm_ctxt, WORD32 i4_ctb_x, WORD32 i4_ctb_y)
{
    ctb_analyse_t *ps_ctb_out;
    cur_ctb_cu_tree_t *ps_cu_tree;
    me_ctb_data_t *ps_me_ctb_data;

    me_ctxt_t *ps_ctxt = (me_ctxt_t *)pv_me_ctxt;
    me_frm_ctxt_t *ps_frm_ctxt = (me_frm_ctxt_t *)pv_me_frm_ctxt;

    ps_ctb_out = ps_frm_ctxt->ps_ctb_analyse_curr_row + i4_ctb_x;

    ps_me_ctb_data = ps_frm_ctxt->ps_me_ctb_data_curr_row + i4_ctb_x;
    ps_cu_tree = ps_frm_ctxt->ps_cu_tree_curr_row + (i4_ctb_x * MAX_NUM_NODES_CU_TREE);

    ps_ctb_out->ps_cu_tree = ps_cu_tree;
    ps_ctb_out->ps_me_ctb_data = ps_me_ctb_data;

    ihevce_populate_me_ctb_data(
        ps_ctxt, ps_frm_ctxt, ps_cu_tree, ps_me_ctb_data, POS_NA, POS_NA, POS_NA);
}

WORD32 ihevce_me_find_poc_in_list(
    recon_pic_buf_t **pps_rec_list, WORD32 poc, WORD32 i4_idr_gop_num, WORD32 num_ref)
{
    WORD32 i;

    for(i = 0; i < num_ref; i++)
    {
        if(pps_rec_list[i]->i4_poc == poc && pps_rec_list[i]->i4_idr_gop_num == i4_idr_gop_num)
            return (i);
    }

    /* should never come here */
    ASSERT(0);
    return (-1);
}
void ihevc_me_update_ref_desc(
    hme_ref_desc_t *ps_ref_desc,
    recon_pic_buf_t *ps_recon_pic,
    WORD32 ref_id_l0,
    WORD32 ref_id_l1,
    WORD32 ref_id_lc,
    WORD32 is_fwd)
{
    hme_ref_buf_info_t *ps_ref_info = &ps_ref_desc->as_ref_info[0];
    iv_enc_yuv_buf_t *ps_yuv_desc = (iv_enc_yuv_buf_t *)&ps_recon_pic->s_yuv_buf_desc;
    iv_enc_yuv_buf_t *ps_src_yuv_desc = (iv_enc_yuv_buf_t *)&ps_recon_pic->s_yuv_buf_desc_src;
    S32 offset;

    /* Padding beyond 64 is not of use to ME */
    ps_ref_info->u1_pad_x = MIN(64, PAD_HORZ);
    ps_ref_info->u1_pad_y = MIN(64, PAD_VERT);

    /* Luma stride and offset. Assuming here that supplied ptr is */
    /* 0, 0 position and hence setting offset to 0. In fact, it is */
    /* not used inside ME as of now.                               */
    ps_ref_info->luma_stride = ps_yuv_desc->i4_y_strd;
    ps_ref_info->luma_offset = 0;

    /* 4 planes, fxfy is the direct recon buf, others are from subpel planes */
    //offset = ps_ref_info->luma_stride * PAD_VERT + PAD_HORZ;
    offset = 0;
    ps_ref_info->pu1_rec_fxfy = (UWORD8 *)ps_yuv_desc->pv_y_buf + offset;
    ps_ref_info->pu1_rec_hxfy = ps_recon_pic->apu1_y_sub_pel_planes[0] + offset;
    ps_ref_info->pu1_rec_fxhy = ps_recon_pic->apu1_y_sub_pel_planes[1] + offset;
    ps_ref_info->pu1_rec_hxhy = ps_recon_pic->apu1_y_sub_pel_planes[2] + offset;
    ps_ref_info->pu1_ref_src = (UWORD8 *)ps_src_yuv_desc->pv_y_buf + offset;

    /* U V ptrs though they are not used */
    ps_ref_info->pu1_rec_u = (U08 *)ps_yuv_desc->pv_u_buf;
    ps_ref_info->pu1_rec_v = (U08 *)ps_yuv_desc->pv_v_buf;

    /* uv offsets and strides, same treatment sa luma */
    ps_ref_info->chroma_offset = 0;
    ps_ref_info->chroma_stride = ps_yuv_desc->i4_uv_strd;

    ps_ref_info->pv_dep_mngr = ps_recon_pic->pv_dep_mngr_recon;

    /* L0, L1 and LC id. */
    ps_ref_desc->i1_ref_id_l0 = ref_id_l0;
    ps_ref_desc->i1_ref_id_l1 = ref_id_l1;
    ps_ref_desc->i1_ref_id_lc = ref_id_lc;

    /* POC of the ref pic */
    ps_ref_desc->i4_poc = ps_recon_pic->i4_poc;

    /* Display num of the ref pic */
    ps_ref_desc->i4_display_num = ps_recon_pic->i4_display_num;

    /* GOP number of the reference pic*/
    ps_ref_desc->i4_GOP_num = ps_recon_pic->i4_idr_gop_num;

    /* Whether this picture is in past (fwd) or future (bck) */
    ps_ref_desc->u1_is_fwd = is_fwd;

    /* store the weight and offsets fo refernce picture */
    ps_ref_desc->i2_weight = ps_recon_pic->s_weight_offset.i2_luma_weight;
    ps_ref_desc->i2_offset = ps_recon_pic->s_weight_offset.i2_luma_offset;
}

/* Create the reference map for ME */
void ihevce_me_create_ref_map(
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    WORD32 num_ref,
    hme_ref_map_t *ps_ref_map)
{
    WORD32 min_ref, i, poc, ref_id_l0, ref_id_l1;

    /* tracks running count of ref pics */
    WORD32 ref_count = 0, i4_idr_gop_num;

    /* points to One instance of a ref pic structure */
    recon_pic_buf_t *ps_recon_pic;

    /* points to one instance of ref desc str used by ME */
    hme_ref_desc_t *ps_ref_desc;

    min_ref = MIN(num_ref_l0_active, num_ref_l1_active);

    for(i = 0; i < min_ref; i++)
    {
        /* Create interleaved L0 and L1 entries */
        ps_ref_desc = &ps_ref_map->as_ref_desc[ref_count];
        ps_recon_pic = pps_rec_list_l0[i];
        poc = ps_recon_pic->i4_poc;
        i4_idr_gop_num = ps_recon_pic->i4_idr_gop_num;
        ref_id_l0 = i;
        ref_id_l1 = ihevce_me_find_poc_in_list(pps_rec_list_l1, poc, i4_idr_gop_num, num_ref);
        ihevc_me_update_ref_desc(ps_ref_desc, ps_recon_pic, ref_id_l0, ref_id_l1, 2 * i, 1);

        ref_count++;

        ps_ref_desc = &ps_ref_map->as_ref_desc[ref_count];
        ps_recon_pic = pps_rec_list_l1[i];
        poc = ps_recon_pic->i4_poc;
        i4_idr_gop_num = ps_recon_pic->i4_idr_gop_num;
        ref_id_l1 = i;
        ref_id_l0 = ihevce_me_find_poc_in_list(pps_rec_list_l0, poc, i4_idr_gop_num, num_ref);
        ihevc_me_update_ref_desc(ps_ref_desc, ps_recon_pic, ref_id_l0, ref_id_l1, 2 * i + 1, 0);

        ref_count++;
    }

    if(num_ref_l0_active > min_ref)
    {
        for(i = 0; i < (num_ref_l0_active - min_ref); i++)
        {
            ps_ref_desc = &ps_ref_map->as_ref_desc[ref_count];
            ref_id_l0 = i + min_ref;
            ps_recon_pic = pps_rec_list_l0[ref_id_l0];
            poc = ps_recon_pic->i4_poc;
            i4_idr_gop_num = ps_recon_pic->i4_idr_gop_num;
            ref_id_l1 = ihevce_me_find_poc_in_list(pps_rec_list_l1, poc, i4_idr_gop_num, num_ref);
            ihevc_me_update_ref_desc(
                ps_ref_desc, ps_recon_pic, ref_id_l0, ref_id_l1, 2 * min_ref + i, 1);
            ref_count++;
        }
    }
    else
    {
        for(i = 0; i < (num_ref_l1_active - min_ref); i++)
        {
            ps_ref_desc = &ps_ref_map->as_ref_desc[ref_count];
            ref_id_l1 = i + min_ref;
            ps_recon_pic = pps_rec_list_l1[ref_id_l1];
            poc = ps_recon_pic->i4_poc;
            i4_idr_gop_num = ps_recon_pic->i4_idr_gop_num;
            ref_id_l0 = ihevce_me_find_poc_in_list(pps_rec_list_l0, poc, i4_idr_gop_num, num_ref);
            ihevc_me_update_ref_desc(
                ps_ref_desc, ps_recon_pic, ref_id_l0, ref_id_l1, 2 * min_ref + i, 0);
            ref_count++;
        }
    }

    ps_ref_map->i4_num_ref = ref_count;
    ASSERT(ref_count == (num_ref_l0_active + num_ref_l1_active));

    /* TODO : Fill better values in lambda depending on ref dist */
    for(i = 0; i < ps_ref_map->i4_num_ref; i++)
        ps_ref_map->as_ref_desc[i].lambda = 20;
}

/*!
******************************************************************************
* \if Function name : ihevce_me_process \endif
*
* \brief
*    Frame level ME function
*
* \par Description:
*    Processing of all layers starting from coarse and going
*    to the refinement layers, all layers
*    that are encoded go CTB by CTB. Outputs of this function are populated
*    ctb_analyse_t structures, one per CTB.
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
void ihevce_me_process(
    void *pv_me_ctxt,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    ctb_analyse_t *ps_ctb_out,
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms,
    double *pd_intra_costs,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse_ctb,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_l0_ipe_input,
    void *pv_coarse_layer,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 i4_frame_parallelism_level,
    WORD32 thrd_id,
    WORD32 i4_me_frm_id)
{
    me_ctxt_t *ps_thrd_ctxt;
    me_frm_ctxt_t *ps_ctxt;

    PF_EXT_UPDATE_FXN_T pf_ext_update_fxn;

    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    cur_ctb_cu_tree_t *ps_cu_tree_out = ps_cur_out_me_prms->ps_cur_ctb_cu_tree;
    me_ctb_data_t *ps_me_ctb_data_out = ps_cur_out_me_prms->ps_cur_ctb_me_data;
    layer_ctxt_t *ps_coarse_layer = (layer_ctxt_t *)pv_coarse_layer;

    pf_ext_update_fxn = (PF_EXT_UPDATE_FXN_T)ihevce_me_update_ctb_results;

    /* get the current thread ctxt pointer */
    ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[thrd_id];
    ps_ctxt = ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];
    ps_ctxt->thrd_id = thrd_id;

    /* store the ctb out and cu out base pointers */
    ps_ctxt->ps_ctb_analyse_base = ps_ctb_out;

    ps_ctxt->ps_cu_tree_base = ps_cu_tree_out;
    ps_ctxt->ps_ipe_l0_ctb_frm_base = ps_ipe_analyse_ctb;
    ps_ctxt->ps_me_ctb_data_base = ps_me_ctb_data_out;
    ps_ctxt->ps_func_selector = &ps_master_ctxt->s_func_selector;

    /** currently in master context. Copying that to me context **/
    /* frame level processing function */
    hme_process_frm(
        (void *)ps_thrd_ctxt,
        ps_l0_ipe_input,
        &ps_master_ctxt->as_ref_map[i4_me_frm_id],
        &pd_intra_costs,
        &ps_master_ctxt->as_frm_prms[i4_me_frm_id],
        pf_ext_update_fxn,
        ps_coarse_layer,
        ps_multi_thrd_ctxt,
        i4_frame_parallelism_level,
        thrd_id,
        i4_me_frm_id);
}
/*!
******************************************************************************
* \if Function name : ihevce_me_frame_dpb_update \endif
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
void ihevce_me_frame_dpb_update(
    void *pv_me_ctxt,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    WORD32 i4_thrd_id)
{
    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    me_ctxt_t *ps_thrd0_ctxt;
    WORD32 a_pocs_to_remove[MAX_NUM_REF + 2];
    WORD32 i, i4_is_buffer_full;
    WORD32 i4_least_POC = 0x7FFFFFFF;
    WORD32 i4_least_GOP_num = 0x7FFFFFFF;
    me_ctxt_t *ps_ctxt;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd ctxt */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

    ps_ctxt = (me_ctxt_t *)ps_thrd0_ctxt;
    a_pocs_to_remove[0] = INVALID_POC;
    /*************************************************************************/
    /* Updation of ME's DPB list. This involves the following steps:         */
    /* 1. Obtain list of active POCs maintained within ME.                   */
    /* 2. Search each of them in the ref list. Whatever is not found goes to */
    /*     the list to be removed. Note: a_pocs_buffered_in_me holds the     */
    /*    currently active POC list within ME. a_pocs_to_remove holds the    */
    /*    list of POCs to be removed, terminated by -1.                      */
    /*************************************************************************/
    i4_is_buffer_full =
        hme_get_active_pocs_list((void *)ps_thrd0_ctxt, ps_master_ctxt->i4_num_me_frm_pllel);

    if(i4_is_buffer_full)
    {
        /* remove if any non-reference pictures are present */
        for(i = 0;
            i <
            (ps_ctxt->aps_me_frm_prms[0]->max_num_ref * ps_master_ctxt->i4_num_me_frm_pllel) + 1;
            i++)
        {
            if(ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_is_reference == 0 &&
               ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_non_ref_free == 1)
            {
                i4_least_POC = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_poc;
                i4_least_GOP_num = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_idr_gop_num;
            }
        }
        /* if all non reference pictures are removed, then find the least poc
        in the least gop number*/
        if(i4_least_POC == 0x7FFFFFFF)
        {
            ASSERT(i4_least_GOP_num == 0x7FFFFFFF);
            for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref *
                            ps_master_ctxt->i4_num_me_frm_pllel) +
                               1;
                i++)
            {
                if(i4_least_GOP_num > ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_idr_gop_num)
                {
                    i4_least_GOP_num = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_idr_gop_num;
                }
            }
            for(i = 0; i < (ps_ctxt->aps_me_frm_prms[0]->max_num_ref *
                            ps_master_ctxt->i4_num_me_frm_pllel) +
                               1;
                i++)
            {
                if(i4_least_POC > ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_poc &&
                   ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_idr_gop_num == i4_least_GOP_num)
                {
                    i4_least_POC = ps_ctxt->as_ref_descr[i].aps_layers[0]->i4_poc;
                }
            }
        }
        ASSERT(i4_least_POC != 0x7FFFFFFF);
        a_pocs_to_remove[0] = i4_least_POC;
        a_pocs_to_remove[1] = INVALID_POC;
    }

    /* Call the ME API to remove "outdated" POCs */
    hme_discard_frm(
        ps_thrd0_ctxt, a_pocs_to_remove, i4_least_GOP_num, ps_master_ctxt->i4_num_me_frm_pllel);
}
/*!
******************************************************************************
* \if Function name : ihevce_me_frame_init \endif
*
* \brief
*    Frame level ME initialisation function
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
void ihevce_me_frame_init(
    void *pv_me_ctxt,
    me_enc_rdopt_ctxt_t *ps_cur_out_me_prms,
    ihevce_static_cfg_params_t *ps_stat_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 num_ref_l0,
    WORD32 num_ref_l1,
    WORD32 num_ref_l0_active,
    WORD32 num_ref_l1_active,
    recon_pic_buf_t **pps_rec_list_l0,
    recon_pic_buf_t **pps_rec_list_l1,
    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2],
    func_selector_t *ps_func_selector,
    ihevce_lap_enc_buf_t *ps_enc_lap_inp,
    void *pv_coarse_layer,
    WORD32 i4_me_frm_id,
    WORD32 i4_thrd_id,
    WORD32 i4_frm_qp,
    WORD32 i4_temporal_layer_id,
    WORD8 i1_cu_qp_delta_enabled_flag,
    void *pv_dep_mngr_encloop_dep_me)
{
    me_ctxt_t *ps_thrd_ctxt;
    me_ctxt_t *ps_thrd0_ctxt;
    me_frm_ctxt_t *ps_ctxt;
    hme_inp_desc_t s_inp_desc;

    WORD32 inp_poc, num_ref;
    WORD32 i;

    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    layer_ctxt_t *ps_coarse_layer = (layer_ctxt_t *)pv_coarse_layer;

    /* Input POC is derived from input buffer */
    inp_poc = ps_enc_lap_inp->s_lap_out.i4_poc;
    num_ref = num_ref_l0 + num_ref_l1;

    /* All processing done using shared / common memory across */
    /* threads is done using thrd ctxt */
    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[i4_thrd_id];

    ps_ctxt = ps_thrd0_ctxt->aps_me_frm_prms[i4_me_frm_id];

    /* Update the paarameters "num_ref_l0_active" and "num_ref_l1_active" in hme_frm_prms */
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].u1_num_active_ref_l0 = num_ref_l0_active;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].u1_num_active_ref_l1 = num_ref_l1_active;

    /*************************************************************************/
    /* Add the current input to ME's DPB. This will also create the pyramids */
    /* for the HME layers tha are not "encoded".                             */
    /*************************************************************************/
    s_inp_desc.i4_poc = inp_poc;
    s_inp_desc.i4_idr_gop_num = ps_enc_lap_inp->s_lap_out.i4_idr_gop_num;
    s_inp_desc.i4_is_reference = ps_enc_lap_inp->s_lap_out.i4_is_ref_pic;
    s_inp_desc.s_layer_desc[0].pu1_y = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_y_buf;
    s_inp_desc.s_layer_desc[0].pu1_u = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_u_buf;
    s_inp_desc.s_layer_desc[0].pu1_v = (UWORD8 *)ps_enc_lap_inp->s_lap_out.s_input_buf.pv_v_buf;

    s_inp_desc.s_layer_desc[0].luma_stride = ps_enc_lap_inp->s_lap_out.s_input_buf.i4_y_strd;
    s_inp_desc.s_layer_desc[0].chroma_stride = ps_enc_lap_inp->s_lap_out.s_input_buf.i4_uv_strd;

    hme_add_inp(pv_me_ctxt, &s_inp_desc, i4_me_frm_id, i4_thrd_id);

    /* store the frm ctb ctxt to all the thrd ctxt */
    {
        WORD32 num_thrds;

        /* initialise the parameters for all the threads */
        for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_me_tmp_frm_ctxt;

            ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            ps_me_tmp_frm_ctxt = ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];

            ps_thrd_ctxt->pv_ext_frm_prms = (void *)ps_frm_ctb_prms;
            ps_me_tmp_frm_ctxt->i4_l0me_qp_mod = ps_stat_prms->s_config_prms.i4_cu_level_rc & 1;

            /* intialize the inter pred (MC) context at frame level */
            ps_me_tmp_frm_ctxt->s_mc_ctxt.ps_ref_list = aps_ref_list;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.i1_weighted_pred_flag =
                ps_enc_lap_inp->s_lap_out.i1_weighted_pred_flag;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.i1_weighted_bipred_flag =
                ps_enc_lap_inp->s_lap_out.i1_weighted_bipred_flag;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.i4_log2_luma_wght_denom =
                ps_enc_lap_inp->s_lap_out.i4_log2_luma_wght_denom;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.i4_log2_chroma_wght_denom =
                ps_enc_lap_inp->s_lap_out.i4_log2_chroma_wght_denom;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.i4_bit_depth = 8;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.u1_chroma_array_type = 1;
            ps_me_tmp_frm_ctxt->s_mc_ctxt.ps_func_selector = ps_func_selector;
            /* Initiallization for non-distributed mode */
            memset(
                ps_me_tmp_frm_ctxt->s_mc_ctxt.ai4_tile_xtra_pel,
                0,
                sizeof(ps_me_tmp_frm_ctxt->s_mc_ctxt.ai4_tile_xtra_pel));

            ps_me_tmp_frm_ctxt->i4_pic_type = ps_enc_lap_inp->s_lap_out.i4_pic_type;

            ps_me_tmp_frm_ctxt->i4_rc_pass = ps_stat_prms->s_pass_prms.i4_pass;
            ps_me_tmp_frm_ctxt->i4_temporal_layer = ps_enc_lap_inp->s_lap_out.i4_temporal_lyr_id;
            ps_me_tmp_frm_ctxt->i4_use_const_lamda_modifier = USE_CONSTANT_LAMBDA_MODIFIER;
            ps_me_tmp_frm_ctxt->i4_use_const_lamda_modifier =
                ps_ctxt->i4_use_const_lamda_modifier ||
                ((ps_stat_prms->s_coding_tools_prms.i4_vqet &
                  (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
                 ((ps_stat_prms->s_coding_tools_prms.i4_vqet &
                   (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)) ||
                  (ps_stat_prms->s_coding_tools_prms.i4_vqet &
                   (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1)) ||
                  (ps_stat_prms->s_coding_tools_prms.i4_vqet &
                   (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_2)) ||
                  (ps_stat_prms->s_coding_tools_prms.i4_vqet &
                   (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_3))));
            {
                ps_me_tmp_frm_ctxt->f_i_pic_lamda_modifier =
                    ps_enc_lap_inp->s_lap_out.f_i_pic_lamda_modifier;
            }
            /* weighted pred enable flag */
            ps_me_tmp_frm_ctxt->i4_wt_pred_enable_flag =
                ps_enc_lap_inp->s_lap_out.i1_weighted_pred_flag |
                ps_enc_lap_inp->s_lap_out.i1_weighted_bipred_flag;

            if(1 == ps_me_tmp_frm_ctxt->i4_wt_pred_enable_flag)
            {
                /* log2 weight denom  */
                ps_me_tmp_frm_ctxt->s_wt_pred.wpred_log_wdc =
                    ps_enc_lap_inp->s_lap_out.i4_log2_luma_wght_denom;
            }
            else
            {
                /* default value */
                ps_me_tmp_frm_ctxt->s_wt_pred.wpred_log_wdc = DENOM_DEFAULT;
            }

            ps_me_tmp_frm_ctxt->u1_is_curFrame_a_refFrame = ps_enc_lap_inp->s_lap_out.i4_is_ref_pic;

            ps_thrd_ctxt->pv_me_optimised_function_list =
                ps_master_ctxt->pv_me_optimised_function_list;
            ps_thrd_ctxt->ps_cmn_utils_optimised_function_list = &ps_master_ctxt->s_cmn_opt_func;
        }
    }

    /* Create the reference map for ME */
    ihevce_me_create_ref_map(
        pps_rec_list_l0,
        pps_rec_list_l1,
        num_ref_l0_active,
        num_ref_l1_active,
        num_ref,
        &ps_master_ctxt->as_ref_map[i4_me_frm_id]);

    /** Remember the pointers to recon list parmas for L0 and L1 lists in the context */
    ps_ctxt->ps_hme_ref_map->pps_rec_list_l0 = pps_rec_list_l0;
    ps_ctxt->ps_hme_ref_map->pps_rec_list_l1 = pps_rec_list_l1;

    /*************************************************************************/
    /* Call the ME frame level processing for further actiion.               */
    /* ToDo: Support Row Level API.                                          */
    /*************************************************************************/
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i2_mv_range_x =
        ps_thrd0_ctxt->s_init_prms.max_horz_search_range;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i2_mv_range_y =
        ps_thrd0_ctxt->s_init_prms.max_vert_search_range;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].is_i_pic = 0;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].is_pic_second_field =
        (!(ps_enc_lap_inp->s_input_buf.i4_bottom_field ^
           ps_enc_lap_inp->s_input_buf.i4_topfield_first));
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_temporal_layer_id = i4_temporal_layer_id;
    {
        S32 pic_type = ps_enc_lap_inp->s_lap_out.i4_pic_type;

        /*********************************************************************/
        /* For I Pic, we do not call update fn at ctb level, instead we do   */
        /* one shot update for entire picture.                               */
        /*********************************************************************/
        if((pic_type == IV_I_FRAME) || (pic_type == IV_II_FRAME) || (pic_type == IV_IDR_FRAME))
        {
            ps_master_ctxt->as_frm_prms[i4_me_frm_id].is_i_pic = 1;
            ps_master_ctxt->as_frm_prms[i4_me_frm_id].bidir_enabled = 0;
        }

        else if((pic_type == IV_P_FRAME) || (pic_type == IV_PP_FRAME))
        {
            ps_master_ctxt->as_frm_prms[i4_me_frm_id].bidir_enabled = 0;
        }
        else if((pic_type == IV_B_FRAME) || (pic_type == IV_BB_FRAME))
        {
            ps_master_ctxt->as_frm_prms[i4_me_frm_id].bidir_enabled = 1;
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
        double d_q_factor;

        d_q_factor = pow(2.0, (i4_frm_qp / 6.)) * 5.0 / 8.0;
        ps_master_ctxt->as_frm_prms[i4_me_frm_id].qstep = (WORD32)(d_q_factor + .5);
        ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_frame_qp = i4_frm_qp;

        /* Qstep multiplied by 256, to work at higher precision:
        5/6 is the rounding factor. Multiplied by 2 for the Had vs DCT
        cost variation */
        ps_master_ctxt->as_frm_prms[i4_me_frm_id].qstep_ls8 =
            (WORD32)((((d_q_factor * 256) * 5) / 3) + .5);
    }

    /* Frame level init of all threads of ME */
    {
        WORD32 num_thrds;

        /* initialise the parameters for all the threads */
        for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_tmp_frm_ctxt;

            ps_thrd_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds];

            ps_tmp_frm_ctxt = ps_thrd_ctxt->aps_me_frm_prms[i4_me_frm_id];

            hme_process_frm_init(
                (void *)ps_thrd_ctxt,
                ps_tmp_frm_ctxt->ps_hme_ref_map,
                ps_tmp_frm_ctxt->ps_hme_frm_prms,
                i4_me_frm_id,
                ps_master_ctxt->i4_num_me_frm_pllel);

            ps_tmp_frm_ctxt->s_frm_lambda_ctxt = *ps_frm_lamda;
            ps_tmp_frm_ctxt->pv_dep_mngr_encloop_dep_me = pv_dep_mngr_encloop_dep_me;
        }
    }

    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_cl_sad_lambda_qf =
        ps_frm_lamda->i4_cl_sad_lambda_qf;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_cl_satd_lambda_qf =
        ps_frm_lamda->i4_cl_satd_lambda_qf;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_ol_sad_lambda_qf =
        ps_frm_lamda->i4_ol_sad_lambda_qf;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].i4_ol_satd_lambda_qf =
        ps_frm_lamda->i4_ol_satd_lambda_qf;
    ps_master_ctxt->as_frm_prms[i4_me_frm_id].lambda_q_shift = LAMBDA_Q_SHIFT;

    ps_master_ctxt->as_frm_prms[i4_me_frm_id].u1_is_cu_qp_delta_enabled =
        i1_cu_qp_delta_enabled_flag;

    /*************************************************************************/
    /* If num ref is 0, that means that it has to be coded as I. Do nothing  */
    /* However mv bank update needs to happen with "intra" mv.               */
    /*************************************************************************/
    if(ps_master_ctxt->as_ref_map[i4_me_frm_id].i4_num_ref == 0 ||
       ps_master_ctxt->as_frm_prms[i4_me_frm_id].is_i_pic)
    {
        for(i = 0; i < 1; i++)
        {
            layer_ctxt_t *ps_layer_ctxt = ps_ctxt->ps_curr_descr->aps_layers[i];
            BLK_SIZE_T e_blk_size;
            S32 use_4x4;

            /* The mv bank is filled with "intra" mv */
            use_4x4 = hme_get_mv_blk_size(
                ps_thrd0_ctxt->s_init_prms.use_4x4, i, ps_ctxt->num_layers, ps_ctxt->u1_encode[i]);
            e_blk_size = use_4x4 ? BLK_4x4 : BLK_8x8;
            hme_init_mv_bank(ps_layer_ctxt, e_blk_size, 2, 1, ps_ctxt->u1_encode[i]);
            hme_fill_mvbank_intra(ps_layer_ctxt);

            /* Clear out the global mvs */
            memset(
                ps_layer_ctxt->s_global_mv,
                0,
                sizeof(hme_mv_t) * ps_ctxt->max_num_ref * NUM_GMV_LOBES);
        }

        return;
    }

    /*************************************************************************/
    /* Encode layer frame init                                               */
    /*************************************************************************/
    {
        refine_prms_t s_refine_prms;
        layer_ctxt_t *ps_curr_layer;
        S16 i2_max;
        S32 layer_id;

        layer_id = 0;
        i2_max = ps_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_x;
        i2_max = MAX(i2_max, ps_ctxt->ps_curr_descr->aps_layers[layer_id]->i2_max_mv_y);

        ps_curr_layer = ps_ctxt->ps_curr_descr->aps_layers[layer_id];

        {
            hme_set_refine_prms(
                &s_refine_prms,
                ps_ctxt->u1_encode[layer_id],
                ps_master_ctxt->as_ref_map[i4_me_frm_id].i4_num_ref,
                layer_id,
                ps_ctxt->num_layers,
                ps_ctxt->num_layers_explicit_search,
                ps_thrd0_ctxt->s_init_prms.use_4x4,
                &ps_master_ctxt->as_frm_prms[i4_me_frm_id],
                NULL,
                &ps_thrd0_ctxt->s_init_prms
                     .s_me_coding_tools); /* during frm init Intra cost Pointer is not required */

            hme_refine_frm_init(ps_curr_layer, &s_refine_prms, ps_coarse_layer);
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_l0_me_frame_end \endif
*
* \brief
*    End of frame update function performs
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

void ihevce_l0_me_frame_end(
    void *pv_me_ctxt, WORD32 i4_idx_dvsr_p, WORD32 i4_display_num, WORD32 me_frm_id)
{
    WORD32 i4_num_ref = 0, num_ref, num_thrds, cur_poc, frm_num;

    me_master_ctxt_t *ps_master_ctxt = (me_master_ctxt_t *)pv_me_ctxt;
    me_ctxt_t *ps_thrd0_ctxt;
    me_frm_ctxt_t *ps_frm_ctxt;
    WORD32 prev_me_frm_id;

    ps_thrd0_ctxt = ps_master_ctxt->aps_me_ctxt[0];
    ps_frm_ctxt = ps_thrd0_ctxt->aps_me_frm_prms[me_frm_id];

    /* Deriving the previous poc from previous frames context */
    if(me_frm_id == 0)
        prev_me_frm_id = (MAX_NUM_ME_PARALLEL - 1);
    else
        prev_me_frm_id = me_frm_id - 1;

    /* Getting the max num references value */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        i4_num_ref =
            MAX(i4_num_ref,
                ps_master_ctxt->aps_me_ctxt[num_thrds]
                    ->aps_me_frm_prms[me_frm_id]
                    ->as_l0_dyn_range_prms[i4_idx_dvsr_p]
                    .i4_num_act_ref_in_l0);
    }

    /* No processing is required if current pic is I pic */
    if(1 == ps_master_ctxt->as_frm_prms[me_frm_id].is_i_pic)
    {
        return;
    }

    /* If a B/b pic, then the previous frame ctxts dyn search prms should be copied ito the latest ctxt */
    if(1 == ps_frm_ctxt->s_frm_prms.bidir_enabled)
    {
        return;
    }

    /* Only for P pic. For P, both are 0, I&B has them mut. exclusive */
    ASSERT(ps_frm_ctxt->s_frm_prms.is_i_pic == ps_frm_ctxt->s_frm_prms.bidir_enabled);

    /* use thrd 0 ctxt to collate the Dynamic Search Range across all threads */
    for(num_ref = 0; num_ref < i4_num_ref; num_ref++)
    {
        dyn_range_prms_t *ps_dyn_range_prms_thrd0;

        ps_dyn_range_prms_thrd0 =
            &ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].as_dyn_range_prms[num_ref];

        /* run a loop over all the other threads to update the dynamical search range */
        for(num_thrds = 1; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
        {
            me_frm_ctxt_t *ps_me_tmp_frm_ctxt;

            dyn_range_prms_t *ps_dyn_range_prms;

            ps_me_tmp_frm_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds]->aps_me_frm_prms[me_frm_id];

            /* get current thrd dynamical search range param. pointer */
            ps_dyn_range_prms =
                &ps_me_tmp_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].as_dyn_range_prms[num_ref];

            /* TODO : This calls can be optimized further. No need for min in 1st call and max in 2nd call */
            hme_update_dynamic_search_params(
                ps_dyn_range_prms_thrd0, ps_dyn_range_prms->i2_dyn_max_y);

            hme_update_dynamic_search_params(
                ps_dyn_range_prms_thrd0, ps_dyn_range_prms->i2_dyn_min_y);
        }
    }

    /*************************************************************************/
    /* Get the MAX/MIN per POC distance based on the all the ref. pics       */
    /*************************************************************************/
    cur_poc = ps_frm_ctxt->i4_curr_poc;
    ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_max_y_per_poc = 0;
    ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_min_y_per_poc = 0;
    /*populate display num*/
    ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i4_display_num = i4_display_num;

    for(num_ref = 0; num_ref < i4_num_ref; num_ref++)
    {
        WORD16 i2_mv_per_poc;
        WORD32 ref_poc, poc_diff;
        dyn_range_prms_t *ps_dyn_range_prms_thrd0;
        ps_dyn_range_prms_thrd0 =
            &ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].as_dyn_range_prms[num_ref];

        ref_poc = ps_dyn_range_prms_thrd0->i4_poc;
        /* Should be cleaned up for ME llsm */
        poc_diff = (cur_poc - ref_poc);
        poc_diff = MAX(1, poc_diff);

        /* cur. ref. pic. max y per POC */
        i2_mv_per_poc = (ps_dyn_range_prms_thrd0->i2_dyn_max_y + (poc_diff - 1)) / poc_diff;
        /* update the max y per POC */
        ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_max_y_per_poc = MAX(
            ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_max_y_per_poc, i2_mv_per_poc);

        /* cur. ref. pic. min y per POC */
        i2_mv_per_poc = (ps_dyn_range_prms_thrd0->i2_dyn_min_y - (poc_diff - 1)) / poc_diff;
        /* update the min y per POC */
        ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_min_y_per_poc = MIN(
            ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_min_y_per_poc, i2_mv_per_poc);
    }

    /*************************************************************************/
    /* Populate the results to all thread ctxt                               */
    /*************************************************************************/
    for(num_thrds = 1; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        me_frm_ctxt_t *ps_me_tmp_frm_ctxt;

        ps_me_tmp_frm_ctxt = ps_master_ctxt->aps_me_ctxt[num_thrds]->aps_me_frm_prms[me_frm_id];

        ps_me_tmp_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_max_y_per_poc =
            ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_max_y_per_poc;

        ps_me_tmp_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_min_y_per_poc =
            ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i2_dyn_min_y_per_poc;

        ps_me_tmp_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i4_display_num =
            ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i4_display_num;
    }

    /* Copy the dynamic search paramteres into the other Frame cotexts in parallel */
    for(num_thrds = 0; num_thrds < ps_master_ctxt->i4_num_proc_thrds; num_thrds++)
    {
        l0_dyn_range_prms_t *ps_dyn_range_prms_thrd0;

        ps_frm_ctxt = ps_thrd0_ctxt->aps_me_frm_prms[me_frm_id];

        i4_num_ref = ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p].i4_num_act_ref_in_l0;

        ps_dyn_range_prms_thrd0 = &ps_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p];

        for(frm_num = 0; frm_num < MAX_NUM_ME_PARALLEL; frm_num++)
        {
            if(me_frm_id != frm_num)
            {
                me_frm_ctxt_t *ps_me_tmp_frm_ctxt;

                l0_dyn_range_prms_t *ps_dyn_range_prms;

                ps_me_tmp_frm_ctxt =
                    ps_master_ctxt->aps_me_ctxt[num_thrds]->aps_me_frm_prms[frm_num];

                /* get current thrd dynamical search range param. pointer */
                ps_dyn_range_prms = &ps_me_tmp_frm_ctxt->as_l0_dyn_range_prms[i4_idx_dvsr_p];

                memcpy(ps_dyn_range_prms, ps_dyn_range_prms_thrd0, sizeof(l0_dyn_range_prms_t));
            }
        }
    }
}
