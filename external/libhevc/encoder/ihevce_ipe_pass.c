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
* \file ihevce_ipe_pass.c
*
* \brief
*    This file contains interface functions of Intra Prediction Estimation
*    module
* \date
*    18/09/2012
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
#include "ihevc_quant_tables.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
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
#include "ihevce_inter_pred.h"
#include "ihevc_weighted_pred.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_ipe_structs.h"
#include "ihevce_ipe_pass.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"
#include "ihevce_recur_bracketing.h"
#include "ihevce_nbr_avail.h"
#include "ihevce_global_tables.h"
#include "ihevc_resi_trans.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Global Tables                                                             */
/*****************************************************************************/

/**
******************************************************************************
* @brief  Look up table for choosing the appropriate function for
*         Intra prediction
*
* @remarks Same look up table enums are used for luma & chroma but each
*          have seperate functions implemented
******************************************************************************
*/
WORD32 g_i4_ipe_funcs[MAX_NUM_IP_MODES] = {
    IPE_FUNC_MODE_0, /* Mode 0 */
    IPE_FUNC_MODE_1, /* Mode 1 */
    IPE_FUNC_MODE_2, /* Mode 2 */
    IPE_FUNC_MODE_3TO9, /* Mode 3 */
    IPE_FUNC_MODE_3TO9, /* Mode 4 */
    IPE_FUNC_MODE_3TO9, /* Mode 5 */
    IPE_FUNC_MODE_3TO9, /* Mode 6 */
    IPE_FUNC_MODE_3TO9, /* Mode 7 */
    IPE_FUNC_MODE_3TO9, /* Mode 8 */
    IPE_FUNC_MODE_3TO9, /* Mode 9 */
    IPE_FUNC_MODE_10, /* Mode 10 */
    IPE_FUNC_MODE_11TO17, /* Mode 11 */
    IPE_FUNC_MODE_11TO17, /* Mode 12 */
    IPE_FUNC_MODE_11TO17, /* Mode 13 */
    IPE_FUNC_MODE_11TO17, /* Mode 14 */
    IPE_FUNC_MODE_11TO17, /* Mode 15 */
    IPE_FUNC_MODE_11TO17, /* Mode 16 */
    IPE_FUNC_MODE_11TO17, /* Mode 17 */
    IPE_FUNC_MODE_18_34, /* Mode 18 */
    IPE_FUNC_MODE_19TO25, /* Mode 19 */
    IPE_FUNC_MODE_19TO25, /* Mode 20 */
    IPE_FUNC_MODE_19TO25, /* Mode 21 */
    IPE_FUNC_MODE_19TO25, /* Mode 22 */
    IPE_FUNC_MODE_19TO25, /* Mode 23 */
    IPE_FUNC_MODE_19TO25, /* Mode 24 */
    IPE_FUNC_MODE_19TO25, /* Mode 25 */
    IPE_FUNC_MODE_26, /* Mode 26 */
    IPE_FUNC_MODE_27TO33, /* Mode 27 */
    IPE_FUNC_MODE_27TO33, /* Mode 26 */
    IPE_FUNC_MODE_27TO33, /* Mode 29 */
    IPE_FUNC_MODE_27TO33, /* Mode 30 */
    IPE_FUNC_MODE_27TO33, /* Mode 31 */
    IPE_FUNC_MODE_27TO33, /* Mode 32 */
    IPE_FUNC_MODE_27TO33, /* Mode 33 */
    IPE_FUNC_MODE_18_34, /* Mode 34 */
};

/**
******************************************************************************
* @brief  Look up table for deciding whether to use original samples or
*         filtered reference samples for Intra prediction
*
* @remarks This table has the flags for transform size of 8, 16 and 32
*          Input is log2nT - 3 and intra prediction mode
******************************************************************************
*/
UWORD8 gau1_ipe_filter_flag[3][MAX_NUM_IP_MODES] = {
    { 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 },
    { 1, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 }
};

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_ipe_recompute_lambda_from_min_8x8_act_in_ctb \endif
*
* \brief
*    This function recomputes lambda using min 8x8 act in CTB
*
* \author
*    Ittiam
*
* \return
*    Nothing
*
******************************************************************************
*/
void ihevce_ipe_recompute_lambda_from_min_8x8_act_in_ctb(
    ihevce_ipe_ctxt_t *ps_ctxt, ihevce_ed_ctb_l1_t *ps_ed_ctb_l1)
{
    WORD32 i4_cu_qp = 0;
#if MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON
    WORD32 i4_activity;
#endif
    WORD32 i4_qscale;
    WORD32 i4_curr_satd;
    long double ld_avg_satd;

#if LAMDA_BASED_ON_QUANT
    i4_curr_satd = ps_ed_ctb_l1->i4_32x32_satd[0][2];
    i8_avg_satd = ps_ctxt->i8_curr_frame_32x32_avg_act[2];
#else
    i4_curr_satd = ps_ed_ctb_l1->i4_32x32_satd[0][3];
    ld_avg_satd = 2.0 + ps_ctxt->ld_curr_frame_16x16_log_avg[0];
#endif

    if(ps_ctxt->i4_l0ipe_qp_mod)
    {
#if MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON
        i4_cu_qp = ihevce_cu_level_qp_mod(
            ps_ctxt->i4_qscale,
            i4_curr_satd,
            ld_avg_satd,
            ps_ctxt->f_strength,
            &i4_activity,
            &i4_qscale,
            ps_ctxt->ps_rc_quant_ctxt);
#endif
    }
    ihevce_get_ipe_ol_cu_lambda_prms(ps_ctxt, i4_cu_qp);
}
/*!
******************************************************************************
* \if Function name : ihevce_ipe_pass_satd \endif
*
* \brief
*    This function calcuates the SATD for a given size and returns the value
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*
* List of Functions
*
******************************************************************************
*/
UWORD32 ihevce_ipe_pass_satd(WORD16 *pi2_coeff, WORD32 coeff_stride, WORD32 trans_size)
{
    WORD32 i, j, satd;

    satd = 0;

    /* run a loop and find the satd by doing ABS */
    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            satd += abs(*pi2_coeff++);
        }
        /* row level update */
        pi2_coeff += coeff_stride - trans_size;
    }

    {
        WORD32 transform_shift;
        WORD32 log2_trans_size;

        GETRANGE(log2_trans_size, trans_size);
        log2_trans_size -= 1;
        transform_shift = MAX_TR_DYNAMIC_RANGE - BIT_DEPTH - log2_trans_size;
        satd >>= transform_shift;
    }

    return (satd);
}

/*!
******************************************************************************
* \if Function name : ihevce_ipe_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for IPE module
*
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_ipe_get_num_mem_recs(void)
{
    return (NUM_IPE_MEM_RECS);
}

/*!
******************************************************************************
* \if Function name : ihevce_ipe_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for IPE.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
* \param[in] i4_num_proc_thrds : Number of processing threads for this module
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32
    ihevce_ipe_get_mem_recs(iv_mem_rec_t *ps_mem_tab, WORD32 i4_num_proc_thrds, WORD32 i4_mem_space)
{
    /* memories should be requested assuming worst case requirememnts */

    /* Module context structure */
    ps_mem_tab[IPE_CTXT].i4_mem_size = sizeof(ihevce_ipe_master_ctxt_t);

    ps_mem_tab[IPE_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[IPE_CTXT].i4_mem_alignment = 8;

    /* Threads ctxt structure */
    ps_mem_tab[IPE_THRDS_CTXT].i4_mem_size = i4_num_proc_thrds * sizeof(ihevce_ipe_ctxt_t);

    ps_mem_tab[IPE_THRDS_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[IPE_THRDS_CTXT].i4_mem_alignment = 32;

    return (NUM_IPE_MEM_RECS);
}

/*!
******************************************************************************
* \if Function name : ihevce_ipe_init \endif
*
* \brief
*    Intialization for IPE context state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_init_prms : Create time static parameters
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_ipe_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_ref_id,
    func_selector_t *ps_func_selector,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_resolution_id,
    UWORD8 u1_is_popcnt_available)
{
    WORD32 i4_thrds;
    UWORD32 u4_width, u4_ctb_in_a_row;
    //  WORD32 i4_ctr;
    ihevce_ipe_master_ctxt_t *ps_master_ctxt;
    ihevce_ipe_ctxt_t *ps_ctxt;

    /* IPE master state structure */
    ps_master_ctxt = (ihevce_ipe_master_ctxt_t *)ps_mem_tab[IPE_CTXT].pv_base;

    ps_master_ctxt->i4_num_proc_thrds = i4_num_proc_thrds;

    ps_ctxt = (ihevce_ipe_ctxt_t *)ps_mem_tab[IPE_THRDS_CTXT].pv_base;

    ps_ctxt->ps_rc_quant_ctxt = ps_rc_quant_ctxt;

    /*width of the input YUV to be encoded. */
    u4_width = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
    /*making the width a multiple of CTB size*/
    u4_width += SET_CTB_ALIGN(
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, MAX_CTB_SIZE);

    u4_ctb_in_a_row = (u4_width / MAX_CTB_SIZE);

    /* perform all one initialisation here */
    for(i4_thrds = 0; i4_thrds < ps_master_ctxt->i4_num_proc_thrds; i4_thrds++)
    {
        ps_master_ctxt->aps_ipe_thrd_ctxt[i4_thrds] = ps_ctxt;

        /* initialise the CU and TU sizes */
        ps_ctxt->u1_ctb_size = (1 << ps_init_prms->s_config_prms.i4_max_log2_cu_size);
        ps_ctxt->u1_min_cu_size = (1 << ps_init_prms->s_config_prms.i4_min_log2_cu_size);
        ps_ctxt->u1_min_tu_size = (1 << ps_init_prms->s_config_prms.i4_min_log2_tu_size);

        /** Register the function selector pointer*/
        ps_ctxt->ps_func_selector = ps_func_selector;

        /* Initiailize the encoder quality preset           */
        /* IPE algorithm is controlled based on this preset */
        ps_ctxt->i4_quality_preset =
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;

        if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P7)
        {
            ps_ctxt->i4_quality_preset = IHEVCE_QUALITY_P6;
        }

        /* initialise all the pointer to start of arrays */
        ps_ctxt->ps_ipe_cu_tree = &ps_ctxt->as_ipe_cu_tree[0];

        /* initialize QP */
        ps_ctxt->i1_QP =
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].ai4_frame_qp[i4_ref_id];
        ps_ctxt->u1_num_b_frames =
            (1 << ps_init_prms->s_coding_tools_prms.i4_max_temporal_layers) - 1;

        ps_ctxt->b_sad_type = IPE_SAD_TYPE;
        ps_ctxt->u1_ipe_step_size = IPE_STEP_SIZE;

        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_0] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_planar_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_1] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_dc_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_2] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode2_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_3TO9] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_3_to_9_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_10] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_horz_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_11TO17] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_11_to_17_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_18_34] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_18_34_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_19TO25] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_19_to_25_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_26] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_ver_fptr;
        ps_ctxt->apf_ipe_lum_ip[IPE_FUNC_MODE_27TO33] =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_mode_27_to_33_fptr;

        /* nbr parameters initialization */
        /* perform all one initialisation here */

        ps_ctxt->i4_nbr_map_strd = MAX_PU_IN_CTB_ROW + 1 + 8;

        ps_ctxt->pu1_ctb_nbr_map = ps_ctxt->au1_nbr_ctb_map[0];

        /* move the pointer to 1,2 location */
        ps_ctxt->pu1_ctb_nbr_map += ps_ctxt->i4_nbr_map_strd;
        ps_ctxt->pu1_ctb_nbr_map++;
        ps_ctxt->i4_l0ipe_qp_mod = ps_init_prms->s_config_prms.i4_cu_level_rc & 1;
        ps_ctxt->i4_pass = ps_init_prms->s_pass_prms.i4_pass;
        if(ps_init_prms->s_coding_tools_prms.i4_use_default_sc_mtx == 0)
        {
            /* initialise the scale & rescale matricies */
            ps_ctxt->api2_scal_mat[0] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[1] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[2] = (WORD16 *)&gi2_flat_scale_mat_8x8[0];
            ps_ctxt->api2_scal_mat[3] = (WORD16 *)&gi2_flat_scale_mat_16x16[0];
            ps_ctxt->api2_scal_mat[4] = (WORD16 *)&gi2_flat_scale_mat_32x32[0];
            /*init for inter matrix*/
            ps_ctxt->api2_scal_mat[5] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[6] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[7] = (WORD16 *)&gi2_flat_scale_mat_8x8[0];
            ps_ctxt->api2_scal_mat[8] = (WORD16 *)&gi2_flat_scale_mat_16x16[0];
            ps_ctxt->api2_scal_mat[9] = (WORD16 *)&gi2_flat_scale_mat_32x32[0];

            /*init for rescale matrix*/
            ps_ctxt->api2_rescal_mat[0] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[1] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[2] = (WORD16 *)&gi2_flat_rescale_mat_8x8[0];
            ps_ctxt->api2_rescal_mat[3] = (WORD16 *)&gi2_flat_rescale_mat_16x16[0];
            ps_ctxt->api2_rescal_mat[4] = (WORD16 *)&gi2_flat_rescale_mat_32x32[0];
            /*init for rescale inter matrix*/
            ps_ctxt->api2_rescal_mat[5] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[6] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[7] = (WORD16 *)&gi2_flat_rescale_mat_8x8[0];
            ps_ctxt->api2_rescal_mat[8] = (WORD16 *)&gi2_flat_rescale_mat_16x16[0];
            ps_ctxt->api2_rescal_mat[9] = (WORD16 *)&gi2_flat_rescale_mat_32x32[0];
        }
        else if(ps_init_prms->s_coding_tools_prms.i4_use_default_sc_mtx == 1)
        {
            /* initialise the scale & rescale matricies */
            ps_ctxt->api2_scal_mat[0] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[1] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[2] = (WORD16 *)&gi2_intra_default_scale_mat_8x8[0];
            ps_ctxt->api2_scal_mat[3] = (WORD16 *)&gi2_intra_default_scale_mat_16x16[0];
            ps_ctxt->api2_scal_mat[4] = (WORD16 *)&gi2_intra_default_scale_mat_32x32[0];
            /*init for inter matrix*/
            ps_ctxt->api2_scal_mat[5] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[6] = (WORD16 *)&gi2_flat_scale_mat_4x4[0];
            ps_ctxt->api2_scal_mat[7] = (WORD16 *)&gi2_inter_default_scale_mat_8x8[0];
            ps_ctxt->api2_scal_mat[8] = (WORD16 *)&gi2_inter_default_scale_mat_16x16[0];
            ps_ctxt->api2_scal_mat[9] = (WORD16 *)&gi2_inter_default_scale_mat_32x32[0];

            /*init for rescale matrix*/
            ps_ctxt->api2_rescal_mat[0] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[1] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[2] = (WORD16 *)&gi2_intra_default_rescale_mat_8x8[0];
            ps_ctxt->api2_rescal_mat[3] = (WORD16 *)&gi2_intra_default_rescale_mat_16x16[0];
            ps_ctxt->api2_rescal_mat[4] = (WORD16 *)&gi2_intra_default_rescale_mat_32x32[0];
            /*init for rescale inter matrix*/
            ps_ctxt->api2_rescal_mat[5] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[6] = (WORD16 *)&gi2_flat_rescale_mat_4x4[0];
            ps_ctxt->api2_rescal_mat[7] = (WORD16 *)&gi2_inter_default_rescale_mat_8x8[0];
            ps_ctxt->api2_rescal_mat[8] = (WORD16 *)&gi2_inter_default_rescale_mat_16x16[0];
            ps_ctxt->api2_rescal_mat[9] = (WORD16 *)&gi2_inter_default_rescale_mat_32x32[0];
        }
        else
        {
            ASSERT(0);
        }

        ps_ctxt->u1_bit_depth = ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth;

        /**
        * Initialize the intra prediction modes map for the CTB to INTRA_DC
        **/
        {
            WORD32 row, col;
            for(row = 0; row < (MAX_TU_ROW_IN_CTB + 1); row++)
                for(col = 0; col < (MAX_TU_COL_IN_CTB + 1); col++)
                    ps_ctxt->au1_ctb_mode_map[row][col] = INTRA_DC;
        }

        ihevce_cmn_utils_instr_set_router(
            &ps_ctxt->s_cmn_opt_func, u1_is_popcnt_available, ps_init_prms->e_arch_type);

        ihevce_ipe_instr_set_router(
            &ps_ctxt->s_ipe_optimised_function_list, ps_init_prms->e_arch_type);

        /* increment the thread ctxt pointer */
        ps_ctxt++;
    }

    /* return the handle to caller */
    return ((void *)ps_master_ctxt);
}
/*!
******************************************************************************
* \if Function name : ihevce_ipe_get_frame_intra_satd_cost \endif
*
* \brief
*    Function to export frame-level accumalated SATD .
*
* \param[in] pv_ctxt : pointer to IPE module
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_ipe_get_frame_intra_satd_cost(
    void *pv_ctxt,
    LWORD64 *pi8_frame_satd_by_qpmod,
    LWORD64 *pi8_frame_acc_mode_bits_cost,
    LWORD64 *pi8_frame_acc_activity_factor,
    LWORD64 *pi8_frame_l0_acc_satd)
{
    WORD32 i4_thrds;

    ihevce_ipe_master_ctxt_t *ps_master_ctxt;
    ihevce_ipe_ctxt_t *ps_ctxt;
    LWORD64 i8_frame_acc_satd_cost = 0;
    LWORD64 i8_frame_acc_satd = 0;
    LWORD64 i8_frame_satd_by_qpmod = 0;
    LWORD64 i8_frame_acc_mode_bits_cost = 0;
    LWORD64 i8_frame_acc_activity_factor = 0;
    /* IPE master state structure */
    ps_master_ctxt = (ihevce_ipe_master_ctxt_t *)pv_ctxt;

    /* perform all one initialisation here */
    for(i4_thrds = 0; i4_thrds < ps_master_ctxt->i4_num_proc_thrds; i4_thrds++)
    {
        ps_ctxt = ps_master_ctxt->aps_ipe_thrd_ctxt[i4_thrds];

        i8_frame_acc_satd_cost += ps_ctxt->i8_frame_acc_satd_cost;
        i8_frame_satd_by_qpmod += (ps_ctxt->i8_frame_acc_satd_by_modqp_q10 >> SATD_BY_ACT_Q_FAC);
        i8_frame_acc_mode_bits_cost += ps_ctxt->i8_frame_acc_mode_bits_cost;

        i8_frame_acc_activity_factor += ps_ctxt->i8_frame_acc_act_factor;

        i8_frame_acc_satd += ps_ctxt->i8_frame_acc_satd;
    }
    *pi8_frame_satd_by_qpmod = i8_frame_satd_by_qpmod;

    *pi8_frame_acc_mode_bits_cost = i8_frame_acc_mode_bits_cost;

    *pi8_frame_acc_activity_factor = i8_frame_acc_activity_factor;

    *pi8_frame_l0_acc_satd = i8_frame_acc_satd;

    return (i8_frame_acc_satd_cost);
}

/**
*******************************************************************************
* \if Function name : ihevce_intra_pred_ref_filtering \endif
*
* \brief
*    Intra prediction interpolation filter for ref_filtering for Encoder
*
* \par Description:
*    Reference DC filtering for neighboring samples dependent  on TU size and
*    mode  Refer to section 8.4.4.2.3 in the standard
*
* \param[in] pu1_src pointer to the source
* \param[out] pu1_dst pointer to the destination
* \param[in] nt integer Transform Block size
*
* \returns
*  none
*
* \author
*  Ittiam
*
*******************************************************************************
*/

#if IHEVCE_INTRA_REF_FILTERING == C
void ihevce_intra_pred_ref_filtering(UWORD8 *pu1_src, WORD32 nt, UWORD8 *pu1_dst)
{
    WORD32 i; /* Generic indexing variable */
    WORD32 four_nt = 4 * nt;

    /* Extremities Untouched*/
    pu1_dst[0] = pu1_src[0];
    pu1_dst[4 * nt] = pu1_src[4 * nt];
    /* Perform bilinear filtering of Reference Samples */
    for(i = 0; i < (four_nt - 1); i++)
    {
        pu1_dst[i + 1] = (pu1_src[i] + 2 * pu1_src[i + 1] + pu1_src[i + 2] + 2) >> 2;
    }
}
#endif

/*!
******************************************************************************
* \if Function name : ihevce_ipe_process_ctb \endif
*
* \brief
*    CTB level IPE function
*
* \param[in] pv_ctxt : pointer to IPE module
* \param[in] ps_frm_ctb_prms : CTB characteristics parameters
* \param[in] ps_curr_src  : pointer to input yuv buffer (row buffer)
* \param[out] ps_ctb_out : pointer to CTB analyse output structure (row buffer)
* \param[out] ps_row_cu : pointer to CU analyse output structure (row buffer)
*
* \return
*    None
*
* Note : This function will receive CTB pointers which may point to
* blocks of CTB size or smaller (at the right and bottom edges of the picture)
* This function recursively creates smaller square partitions and passes them
* on for intra processing estimation
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_ipe_process_ctb(
    ihevce_ipe_ctxt_t *ps_ctxt,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    iv_enc_yuv_buf_t *ps_curr_src,
    ihevce_ipe_cu_tree_t *ps_curr_ctb_node,
    ipe_l0_ctb_analyse_for_me_t *ps_l0_ipe_out_ctb,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t      *ps_row_cu,
    ihevce_ed_blk_t *ps_ed_l1_ctb,
    ihevce_ed_blk_t *ps_ed_l2_ctb,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1)
{
    /* reset the map buffer to 0*/
    memset(
        &ps_ctxt->au1_nbr_ctb_map[0][0],
        0,
        (MAX_PU_IN_CTB_ROW + 1 + 8) * (MAX_PU_IN_CTB_ROW + 1 + 8));

    /* set the CTB neighbour availability flags */
    ihevce_set_ctb_nbr(
        &ps_ctxt->s_ctb_nbr_avail_flags,
        ps_ctxt->pu1_ctb_nbr_map,
        ps_ctxt->i4_nbr_map_strd,
        ps_ctxt->u2_ctb_num_in_row,
        ps_ctxt->u2_ctb_row_num,
        ps_frm_ctb_prms);

    /* IPE cu and mode decision */
    ihevce_bracketing_analysis(
        ps_ctxt,
        ps_curr_ctb_node,
        ps_curr_src,
        ps_ctb_out,
        //ps_row_cu,
        ps_ed_l1_ctb,
        ps_ed_l2_ctb,
        ps_ed_ctb_l1,
        ps_l0_ipe_out_ctb);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_ipe_process_row \endif
*
* \brief
*    Row level IPE function
*
* \param[in] pv_ctxt : pointer to IPE module
* \param[in] ps_frm_ctb_prms : CTB characteristics parameters
* \param[in] ps_curr_src  : pointer to input yuv buffer (row buffer)
* \param[out] ps_ctb_out : pointer to CTB analyse output structure (row buffer)
* \param[out] ps_cu_out : pointer to CU analyse output structure (row buffer)
*\param[out]  pi4_num_ctbs_cur_row  : pointer to store the number of ctbs processed in current row
*\param[in]  pi4_num_ctbs_top_row  : pointer to check the number of ctbs processed in top row
*
* \return
*    None
*
* Note : Currently the frame level calculations done assumes that
*        framewidth of the input are excat multiple of ctbsize
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_ipe_process_row(
    ihevce_ipe_ctxt_t *ps_ctxt,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    iv_enc_yuv_buf_t *ps_curr_src,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_ctb_out_row,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t   *ps_row_cu,
    ihevce_ed_blk_t *ps_ed_l1_row,
    ihevce_ed_blk_t *ps_ed_l2_row,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_row,
    WORD32 blk_inc_ctb_l1,
    WORD32 blk_inc_ctb_l2)
{
    /* local variables */
    UWORD16 ctb_ctr;
    iv_enc_yuv_buf_t s_curr_src_bufs;
    ipe_l0_ctb_analyse_for_me_t *ps_l0_ipe_out_ctb;
    UWORD16 u2_pic_wdt;
    UWORD16 u2_pic_hgt;
    ihevce_ed_blk_t *ps_ed_l1_ctb;
    ihevce_ed_blk_t *ps_ed_l2_ctb;
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1;

    UWORD8 u1_ctb_size;

    u2_pic_wdt = ps_frm_ctb_prms->i4_cu_aligned_pic_wd;
    u2_pic_hgt = ps_frm_ctb_prms->i4_cu_aligned_pic_ht;

    u1_ctb_size = ps_ctxt->u1_ctb_size;

    /* ----------------------------------------------------- */
    /* store the stride and dimensions of source             */
    /* buffer pointers will be over written at every CTB row */
    /* ----------------------------------------------------- */
    memcpy(&s_curr_src_bufs, ps_curr_src, sizeof(iv_enc_yuv_buf_t));
    ps_l0_ipe_out_ctb = ps_ipe_ctb_out_row;

    /* --------- Loop over all the CTBs in a row --------------- */
    for(ctb_ctr = 0; ctb_ctr < ps_frm_ctb_prms->i4_num_ctbs_horz; ctb_ctr++)
    {
        //UWORD8            num_cus_in_ctb;

        UWORD8 *pu1_tmp;

        /* Create pointer to ctb node */
        ihevce_ipe_cu_tree_t *ps_ctb_node;

        WORD32 nbr_flags;

        WORD32 row;
        /* luma src */
        pu1_tmp = (UWORD8 *)ps_curr_src->pv_y_buf;
        pu1_tmp += (ctb_ctr * ps_frm_ctb_prms->i4_ctb_size);

        s_curr_src_bufs.pv_y_buf = pu1_tmp;

        /* Cb & CR pixel interleaved src */
        pu1_tmp = (UWORD8 *)ps_curr_src->pv_u_buf;
        pu1_tmp += (ctb_ctr * (ps_frm_ctb_prms->i4_ctb_size >> 1));

        s_curr_src_bufs.pv_u_buf = pu1_tmp;

        /* Store the number of current ctb within row in the context */
        ps_ctxt->u2_ctb_num_in_row = ctb_ctr;

        /* Initialize number of coding units in ctb to 0 */
        ps_ctb_out->u1_num_cus_in_ctb = 0;
        /* Initialize split flag to 0 - No partition  */
        ps_ctb_out->u4_cu_split_flags = 0;
        /* store the cu pointer for current ctb out */
        //ps_ctb_out->ps_coding_units_in_ctb = ps_row_cu;

        /* Initialize the CTB parameters at the root node level */
        ps_ctb_node = ps_ctxt->ps_ipe_cu_tree;
        ps_ctb_node->ps_parent = NULL;
        ps_ctb_node->u1_depth = 0;
        ps_ctb_node->u1_cu_size = u1_ctb_size;
        ps_ctb_node->u2_x0 = 0;
        ps_ctb_node->u2_y0 = 0;

        ps_ctb_node->u2_orig_x = ctb_ctr * ps_ctb_node->u1_cu_size;
        ps_ctb_node->u2_orig_y = ps_ctxt->u2_ctb_row_num * ps_ctb_node->u1_cu_size;

        ps_ctb_node->u1_width = u1_ctb_size;
        ps_ctb_node->u1_height = u1_ctb_size;
#if !(PIC_ALIGN_CTB_SIZE)
        if(ps_ctxt->u2_ctb_num_in_row == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
        {
            ps_ctb_node->u1_width = u2_pic_wdt - (ps_ctxt->u2_ctb_num_in_row) * (u1_ctb_size);
        }
        if(ps_ctxt->u2_ctb_row_num == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
        {
            ps_ctb_node->u1_height = u2_pic_hgt - (ps_ctxt->u2_ctb_row_num) * (u1_ctb_size);
        }
#endif

        switch(ps_ctb_node->u1_cu_size)
        {
        case 64:
            ps_ctb_node->u1_log2_nt = 6;
            ps_ctb_node->u1_part_flag_pos = 0;
            break;
        case 32:
            ps_ctb_node->u1_log2_nt = 5;
            ps_ctb_node->u1_part_flag_pos = 4;
            break;
        case 16:
            ps_ctb_node->u1_log2_nt = 4;
            ps_ctb_node->u1_part_flag_pos = 8;
            break;
        }

        /* Set neighbor flags for the CTB */
        nbr_flags = 0;

        if(ps_ctxt->u2_ctb_num_in_row != 0)
        {
            nbr_flags |= LEFT_FLAG; /* Set Left Flag if not in first column */
            ps_ctb_node->u1_num_left_avail = ((u2_pic_hgt - ps_ctb_node->u2_orig_y) >= u1_ctb_size)
                                                 ? u1_ctb_size
                                                 : u2_pic_hgt - ps_ctb_node->u2_orig_y;
        }
        else
        {
            ps_ctb_node->u1_num_left_avail = 0;
        }

        if((ps_ctxt->u2_ctb_num_in_row != 0) && (ps_ctxt->u2_ctb_row_num != 0))
            nbr_flags |= TOP_LEFT_FLAG; /* Set Top-Left Flag if not in first row or first column */

        if(ps_ctxt->u2_ctb_row_num != 0)
        {
            nbr_flags |= TOP_FLAG; /* Set Top Flag if not in first row */
            ps_ctb_node->u1_num_top_avail = ((u2_pic_wdt - ps_ctb_node->u2_orig_x) >= u1_ctb_size)
                                                ? u1_ctb_size
                                                : u2_pic_wdt - ps_ctb_node->u2_orig_x;
        }
        else
        {
            ps_ctb_node->u1_num_top_avail = 0;
        }

        if(ps_ctxt->u2_ctb_row_num != 0)
        {
            if(ps_ctxt->u2_ctb_num_in_row == (ps_frm_ctb_prms->i4_num_ctbs_horz - 1))
                ps_ctb_node->u1_num_top_right_avail = 0;
            else
            {
                ps_ctb_node->u1_num_top_right_avail =
                    ((u2_pic_wdt - ps_ctb_node->u2_orig_x - u1_ctb_size) >= u1_ctb_size)
                        ? u1_ctb_size
                        : u2_pic_wdt - ps_ctb_node->u2_orig_x - u1_ctb_size;
                nbr_flags |=
                    TOP_RIGHT_FLAG; /* Set Top-Right Flag if not in first row or last column*/
            }
        }
        else
        {
            ps_ctb_node->u1_num_top_right_avail = 0;
        }

        ps_ctb_node->u1_num_bottom_left_avail = 0;

        ps_ctb_node->i4_nbr_flag = nbr_flags;

        /**
        * Update CTB Mode Map
        * In case this is first CTB in a row, set left most column to INTRA_DC (NA)
        * else copy last column to first column
        **/
        if(ctb_ctr == 0)
        {
            for(row = 0; row < (MAX_TU_ROW_IN_CTB + 1); row++)
            {
                ps_ctxt->au1_ctb_mode_map[row][0] = INTRA_DC;
            }
        }
        else
        {
            for(row = 0; row < (MAX_TU_ROW_IN_CTB + 1); row++)
            {
                ps_ctxt->au1_ctb_mode_map[row][0] =
                    ps_ctxt->au1_ctb_mode_map[row][MAX_TU_COL_IN_CTB];
            }
        }

        /* --------- IPE call at CTB level ------------------ */

        /* IPE CTB function is expected to Decide on the CUs sizes  */
        /* and populate the best intra prediction modes and TX flags*/
        /* Interface of this CTb level function is kept open */

        ps_ed_l1_ctb = ps_ed_l1_row + ctb_ctr * blk_inc_ctb_l1;
        ps_ed_l2_ctb = ps_ed_l2_row + ctb_ctr * blk_inc_ctb_l2;
        ps_ed_ctb_l1 = ps_ed_ctb_l1_row + ctb_ctr;

        if(ps_ctxt->u1_use_lambda_derived_from_min_8x8_act_in_ctb)
        {
            ihevce_ipe_recompute_lambda_from_min_8x8_act_in_ctb(ps_ctxt, ps_ed_ctb_l1);
        }

        ihevce_ipe_process_ctb(
            ps_ctxt,
            ps_frm_ctb_prms,
            &s_curr_src_bufs,
            ps_ctb_node,
            ps_l0_ipe_out_ctb,
            ps_ctb_out,
            //ps_row_cu,
            ps_ed_l1_ctb,
            ps_ed_l2_ctb,
            ps_ed_ctb_l1);

        /* -------------- ctb level updates ----------------- */

        ps_l0_ipe_out_ctb++;
        //num_cus_in_ctb = ps_ctb_out->u1_num_cus_in_ctb;

        //ps_row_cu += num_cus_in_ctb;

        ps_ctb_out++;
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_ipe_process \endif
*
* \brief
*    Frame level IPE function
*
* \param[in] pv_ctxt : pointer to IPE module
* \param[in] ps_frm_ctb_prms : CTB characteristics parameters
* \param[in] ps_inp  : pointer to input yuv buffer (frame buffer)
* \param[out] ps_ctb_out : pointer to CTB analyse output structure (frame buffer)
* \param[out] ps_cu_out : pointer to CU analyse output structure (frame buffer)
*
* \return
*    None
*
* Note : Currently the frame level calculations done assumes that
*        framewidth of the input are excat multiple of ctbsize
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_ipe_process(
    void *pv_ctxt,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    pre_enc_L0_ipe_encloop_ctxt_t *ps_L0_IPE_curr_out_pre_enc,
    ctb_analyse_t *ps_ctb_out,
    //cu_analyse_t               *ps_cu_out,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_ctb_out,
    void *pv_multi_thrd_ctxt,
    WORD32 slice_type,
    ihevce_ed_blk_t *ps_ed_pic_l1,
    ihevce_ed_blk_t *ps_ed_pic_l2,
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_pic,
    WORD32 thrd_id,
    WORD32 i4_ping_pong)
{
    /* local variables */
    ihevce_ipe_master_ctxt_t *ps_master_ctxt;
    iv_enc_yuv_buf_t *ps_inp = &ps_curr_inp->s_lap_out.s_input_buf;
    ihevce_ipe_ctxt_t *ps_ctxt;
    iv_enc_yuv_buf_t s_curr_src_bufs;
    WORD32 end_of_frame;

    ihevce_ed_blk_t *ps_ed_l1_row;
    ihevce_ed_blk_t *ps_ed_l2_row;
    ihevce_ed_ctb_l1_t *ps_ed_ctb_l1_row;
    WORD32 blk_inc_ctb_l1 = 0;
    WORD32 blk_inc_ctb_l2 = 0;

    /* Layer 1 pre intra analysis related initilization.
    * Compute no of 8x8 blks in the ctb which which is
    * same as no of 4x4 blks in the ctb in layer 1 */
    blk_inc_ctb_l1 = ps_frm_ctb_prms->i4_ctb_size >> 3;
    blk_inc_ctb_l1 = blk_inc_ctb_l1 * blk_inc_ctb_l1;

    /* Layer 2 pre intra analysis related initilization.
    * Compute no of 16x16 blks in the ctb which which is
    * same as no of 8x8 blks in the ctb in layer 2 */
    blk_inc_ctb_l2 = ps_frm_ctb_prms->i4_ctb_size >> 4;
    blk_inc_ctb_l2 = blk_inc_ctb_l2 * blk_inc_ctb_l2;

    /* ----------------------------------------------------- */
    /* store the stride and dimensions of source             */
    /* buffer pointers will be over written at every CTB row */
    /* ----------------------------------------------------- */
    memcpy(&s_curr_src_bufs, ps_inp, sizeof(iv_enc_yuv_buf_t));

    ps_master_ctxt = (ihevce_ipe_master_ctxt_t *)pv_ctxt;
    ps_ctxt = ps_master_ctxt->aps_ipe_thrd_ctxt[thrd_id];
    end_of_frame = 0;

    if(ISLICE == slice_type)
    {
        ps_ctxt->b_sad_type = IPE_SAD_TYPE;
        ps_ctxt->i4_ol_satd_lambda = ps_frm_lamda->i4_ol_satd_lambda_qf;
        ps_ctxt->i4_ol_sad_lambda = ps_frm_lamda->i4_ol_sad_lambda_qf;
    }
    else
    {
        ps_ctxt->b_sad_type = IPE_SAD_TYPE; /* SAD */
        ps_ctxt->i4_ol_satd_lambda = ps_frm_lamda->i4_ol_satd_lambda_qf;
        ps_ctxt->i4_ol_sad_lambda = ps_frm_lamda->i4_ol_sad_lambda_qf;
    }

    ihevce_populate_ipe_ol_cu_lambda_prms(
        (void *)ps_ctxt,
        ps_frm_lamda,
        slice_type,
        ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
        IPE_LAMBDA_TYPE);

    /* register the slice type in the ctxt */
    ps_ctxt->i4_slice_type = slice_type;

    /** Frame-levelSATD cost accumalator init to 0 */
    ps_ctxt->i8_frame_acc_satd_cost = 0;

    /** Frame-levelSATD accumalator init to 0 */
    ps_ctxt->i8_frame_acc_satd = 0;

    /** Frame-level Activity factor accumalator init to 1 */
    ps_ctxt->i8_frame_acc_act_factor = 1;

    /** Frame-levelMode Bits cost accumalator init to 0 */
    ps_ctxt->i8_frame_acc_mode_bits_cost = 0;

    /** Frame -level SATD/qp acc init to 0*/
    ps_ctxt->i8_frame_acc_satd_by_modqp_q10 = 0;

    /* ------------ Loop over all the CTB rows --------------- */
    while(0 == end_of_frame)
    {
        UWORD8 *pu1_tmp;
        WORD32 vert_ctr;
        //cu_analyse_t *ps_row_cu;
        ctb_analyse_t *ps_ctb_out_row;
        job_queue_t *ps_job;
        ipe_l0_ctb_analyse_for_me_t *ps_ipe_ctb_out_row;

        /* Get the current row from the job queue */
        ps_job = (job_queue_t *)ihevce_pre_enc_grp_get_next_job(
            pv_multi_thrd_ctxt, IPE_JOB_LYR0, 1, i4_ping_pong);

        /* If all rows are done, set the end of process flag to 1, */
        /* and the current row to -1 */
        if(NULL == ps_job)
        {
            vert_ctr = -1;
            end_of_frame = 1;
        }
        else
        {
            ASSERT(IPE_JOB_LYR0 == ps_job->i4_pre_enc_task_type);

            /* Obtain the current row's details from the job */
            vert_ctr = ps_job->s_job_info.s_ipe_job_info.i4_ctb_row_no;
            //DBG_PRINTF("IPE PASS : Thread id %d, Vert Ctr %d\n",thrd_id,vert_ctr);

            /* Update the ipe context with current row number */
            ps_ctxt->u2_ctb_row_num = vert_ctr;

            /* derive the current ctb row pointers */

            /* luma src */
            pu1_tmp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_y_buf;
            pu1_tmp += (vert_ctr * ps_frm_ctb_prms->i4_ctb_size * ps_inp->i4_y_strd);

            s_curr_src_bufs.pv_y_buf = pu1_tmp;

            /* Cb & CR pixel interleaved src */
            pu1_tmp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_u_buf;
            pu1_tmp += (vert_ctr * (ps_frm_ctb_prms->i4_ctb_size >> 1) * ps_inp->i4_uv_strd);

            s_curr_src_bufs.pv_u_buf = pu1_tmp;

            /* row intra analyse cost buffer */
            ps_ipe_ctb_out_row = ps_ipe_ctb_out + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* row ctb out structure */
            ps_ctb_out_row = ps_ctb_out + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* call the row level processing function */
            ps_ed_l1_row =
                ps_ed_pic_l1 + ps_frm_ctb_prms->i4_num_ctbs_horz * blk_inc_ctb_l1 * vert_ctr;
            ps_ed_l2_row =
                ps_ed_pic_l2 + ps_frm_ctb_prms->i4_num_ctbs_horz * blk_inc_ctb_l2 * vert_ctr;
            ps_ed_ctb_l1_row = ps_ed_ctb_l1_pic + ps_frm_ctb_prms->i4_num_ctbs_horz * vert_ctr;
            ihevce_ipe_process_row(
                ps_ctxt,
                ps_frm_ctb_prms,
                &s_curr_src_bufs,
                ps_ipe_ctb_out_row,
                ps_ctb_out_row,
                //ps_row_cu,
                ps_ed_l1_row,
                ps_ed_l2_row,
                ps_ed_ctb_l1_row,
                blk_inc_ctb_l1,
                blk_inc_ctb_l2);

            memset(
                ps_ed_l1_row,
                0,
                ps_frm_ctb_prms->i4_num_ctbs_horz * blk_inc_ctb_l1 * sizeof(ihevce_ed_blk_t));
            memset(
                ps_ed_l2_row,
                0,
                ps_frm_ctb_prms->i4_num_ctbs_horz * blk_inc_ctb_l2 * sizeof(ihevce_ed_blk_t));

            /* set the output dependency */
            ihevce_pre_enc_grp_job_set_out_dep(pv_multi_thrd_ctxt, ps_job, i4_ping_pong);
        }
    }

    /* EIID: Print stat regarding how many 16x16 blocks are skipped in the frame, valid for single thread only */
    //DBG_PRINTF("num_16x16_analyze_skipped: %d\n",ps_ctxt->u4_num_16x16_skips_at_L0_IPE);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_frame_lambda_prms \endif
*
* \brief
*    Function whihc calculates the Lambda params for current picture
*
* \param[in] ps_enc_ctxt : encoder ctxt pointer
* \param[in] ps_cur_pic_ctxt : current pic ctxt
* \param[in] i4_cur_frame_qp : current pic QP
* \param[in] first_field : is first field flag
* \param[in] i4_temporal_lyr_id : Current picture layer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_get_ipe_ol_cu_lambda_prms(void *pv_ctxt, WORD32 i4_cur_cu_qp)
{
    ihevce_ipe_ctxt_t *ps_ctxt = (ihevce_ipe_ctxt_t *)pv_ctxt;
    //WORD32 chroma_qp = gau1_ihevc_chroma_qp_scale[i4_cur_cu_qp];

    /* Store the params for IPE pass */
    ps_ctxt->i4_ol_satd_lambda = ps_ctxt->i4_ol_satd_lambda_qf_array[i4_cur_cu_qp];
    ps_ctxt->i4_ol_sad_lambda = ps_ctxt->i4_ol_sad_lambda_qf_array[i4_cur_cu_qp];
}

/*!
******************************************************************************
* \if Function name : ihevce_get_frame_lambda_prms \endif
*
* \brief
*    Function whihc calculates the Lambda params for current picture
*
* \param[in] ps_enc_ctxt : encoder ctxt pointer
* \param[in] ps_cur_pic_ctxt : current pic ctxt
* \param[in] i4_cur_frame_qp : current pic QP
* \param[in] first_field : is first field flag
* \param[in] i4_temporal_lyr_id : Current picture layer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_populate_ipe_ol_cu_lambda_prms(
    void *pv_ctxt,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 i4_slice_type,
    WORD32 i4_temporal_lyr_id,
    WORD32 i4_lambda_type)
{
    WORD32 i4_curr_cu_qp;
    double lambda_modifier;
    double lambda_uv_modifier;
    double lambda;
    double lambda_uv;

    ihevce_ipe_ctxt_t *ps_ctxt = (ihevce_ipe_ctxt_t *)pv_ctxt;

    WORD32 i4_qp_bd_offset = 6 * (ps_ctxt->u1_bit_depth - 8);

    for(i4_curr_cu_qp =
            ps_ctxt->ps_rc_quant_ctxt->i2_min_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset;
        i4_curr_cu_qp <= ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;
        i4_curr_cu_qp++)
    {
        WORD32 chroma_qp = i4_curr_cu_qp;

        if((BSLICE == i4_slice_type) && (i4_temporal_lyr_id))
        {
            lambda_modifier = ps_frm_lamda->lambda_modifier *
                              CLIP3((((double)(i4_curr_cu_qp - 12)) / 6.0), 2.00, 4.00);
            lambda_uv_modifier = ps_frm_lamda->lambda_uv_modifier *
                                 CLIP3((((double)(chroma_qp - 12)) / 6.0), 2.00, 4.00);
        }
        else
        {
            lambda_modifier = ps_frm_lamda->lambda_modifier;
            lambda_uv_modifier = ps_frm_lamda->lambda_uv_modifier;
        }
        if(ps_ctxt->i4_use_const_lamda_modifier)
        {
            if(ISLICE == i4_slice_type)
            {
                lambda_modifier = ps_ctxt->f_i_pic_lamda_modifier;
                lambda_uv_modifier = ps_ctxt->f_i_pic_lamda_modifier;
            }
            else
            {
                lambda_modifier = CONST_LAMDA_MOD_VAL;
                lambda_uv_modifier = CONST_LAMDA_MOD_VAL;
            }
        }

        switch(i4_lambda_type)
        {
        case 0:
        {
            i4_qp_bd_offset = 0;

            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda) / 2) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda * 1.9) / 2) * (1 << LAMBDA_Q_SHIFT));
            }

            ps_ctxt->i4_ol_sad_type2_lambda_qf_array[i4_curr_cu_qp] =
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp];

            ps_ctxt->i4_ol_satd_type2_lambda_qf_array[i4_curr_cu_qp] =
                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp];

            break;
        }
        case 1:
        {
            ASSERT(0); /* should not enter the path for IPE*/
            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda) / 2) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda * 1.9) / 2) * (1 << LAMBDA_Q_SHIFT));
            }

            ps_ctxt->i4_ol_sad_type2_lambda_qf_array[i4_curr_cu_qp] =
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp];

            ps_ctxt->i4_ol_satd_type2_lambda_qf_array[i4_curr_cu_qp] =
                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp];

            break;
        }
        case 2:
        {
            ASSERT(0); /* should not enter the path for IPE*/
            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_ol_sad_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda) / 2) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda * 1.9) / 2) * (1 << LAMBDA_Q_SHIFT));
            }
            i4_qp_bd_offset = 0;

            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bd_offset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_ol_sad_type2_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_type2_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda)) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_ol_sad_type2_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda) / 2) * (1 << LAMBDA_Q_SHIFT));

                ps_ctxt->i4_ol_satd_type2_lambda_qf_array[i4_curr_cu_qp] =
                    (WORD32)((sqrt(lambda * 1.9) / 2) * (1 << LAMBDA_Q_SHIFT));
            }
            break;
        }
        default:
        {
            /* Intended to be a barren wasteland! */
            ASSERT(0);
        }
        }
    }
}

#define ME_COST_THRSHOLD 7
/*!
******************************************************************************
* \if Function name : ihevce_get_frame_lambda_prms \endif
*
* \brief
*    Function whihc calculates the Lambda params for current picture
*
* \param[in] ps_enc_ctxt : encoder ctxt pointer
* \param[in] ps_cur_pic_ctxt : current pic ctxt
* \param[in] i4_cur_frame_qp : current pic QP
* \param[in] first_field : is first field flag
* \param[in] i4_temporal_lyr_id : Current picture layer id
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_populate_ipe_frame_init(
    void *pv_ctxt,
    ihevce_static_cfg_params_t *ps_stat_prms,
    WORD32 i4_curr_frm_qp,
    WORD32 i4_slice_type,
    WORD32 i4_thrd_id,
    pre_enc_me_ctxt_t *ps_curr_out,
    WORD8 i1_cu_qp_delta_enabled_flag,
    rc_quant_t *ps_rc_quant_ctxt,
    WORD32 i4_quality_preset,
    WORD32 i4_temporal_lyr_id,
    ihevce_lap_output_params_t *ps_lap_out)
{
    ihevce_ipe_master_ctxt_t *ps_master_ctxt = (ihevce_ipe_master_ctxt_t *)pv_ctxt;
    WORD32 i4_i;
    WORD32 ai4_mod_factor_num[2];

    ihevce_ipe_ctxt_t *ps_ctxt = ps_master_ctxt->aps_ipe_thrd_ctxt[i4_thrd_id];
    ps_ctxt->i4_hevc_qp = i4_curr_frm_qp;
    ps_ctxt->i4_quality_preset = i4_quality_preset;
    ps_ctxt->i4_temporal_lyr_id = i4_temporal_lyr_id;
    ps_ctxt->ps_rc_quant_ctxt = ps_rc_quant_ctxt;
    ps_ctxt->i4_qscale =
        ps_ctxt->ps_rc_quant_ctxt
            ->pi4_qp_to_qscale[i4_curr_frm_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];

    ps_ctxt->i4_frm_qp = i4_curr_frm_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset;
    ps_ctxt->i4_slice_type = i4_slice_type;  //EIID
    ps_ctxt->i4_temporal_layer = ps_lap_out->i4_temporal_lyr_id;
    ps_ctxt->i4_is_ref_pic = ps_lap_out->i4_is_ref_pic;
    ps_ctxt->u4_num_16x16_skips_at_L0_IPE = 0;
    ps_ctxt->i4_use_const_lamda_modifier = USE_CONSTANT_LAMBDA_MODIFIER;
    ps_ctxt->i4_use_const_lamda_modifier =
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
        ps_ctxt->f_i_pic_lamda_modifier = ps_lap_out->f_i_pic_lamda_modifier;
    }
#if POW_OPT
    for(i4_i = 0; i4_i < 2; i4_i++)
    {
        ps_ctxt->ld_curr_frame_8x8_log_avg[i4_i] = ps_curr_out->ld_curr_frame_8x8_log_avg[i4_i];
        ps_ctxt->ld_curr_frame_16x16_log_avg[i4_i] = ps_curr_out->ld_curr_frame_16x16_log_avg[i4_i];
        ps_ctxt->ld_curr_frame_32x32_log_avg[i4_i] = ps_curr_out->ld_curr_frame_32x32_log_avg[i4_i];
    }

    ps_ctxt->ld_curr_frame_16x16_log_avg[2] = ps_curr_out->ld_curr_frame_16x16_log_avg[2];
    ps_ctxt->ld_curr_frame_32x32_log_avg[2] = ps_curr_out->ld_curr_frame_32x32_log_avg[2];
    ps_ctxt->i8_curr_frame_avg_mean_act = ps_curr_out->i8_curr_frame_avg_mean_act;
#else
    for(i4_i = 0; i4_i < 2; i4_i++)
    {
        ps_ctxt->i8_curr_frame_8x8_avg_act[i4_i] = ps_curr_out->i8_curr_frame_8x8_avg_act[i4_i];
        ps_ctxt->i8_curr_frame_16x16_avg_act[i4_i] = ps_curr_out->i8_curr_frame_16x16_avg_act[i4_i];
        ps_ctxt->i8_curr_frame_32x32_avg_act[i4_i] = ps_curr_out->i8_curr_frame_32x32_avg_act[i4_i];
    }

    ps_ctxt->i8_curr_frame_16x16_avg_act[2] = ps_curr_out->i8_curr_frame_16x16_avg_act[2];
    ps_ctxt->i8_curr_frame_32x32_avg_act[2] = ps_curr_out->i8_curr_frame_32x32_avg_act[2];
#endif

    ps_ctxt->pi2_trans_out =
        (WORD16 *)&ps_ctxt->au1_pred_samples[0];  //overlaying trans coeff memory with pred_samples
    ps_ctxt->pi2_trans_tmp = (WORD16 *)&ps_ctxt->au1_pred_samples[2048];

    /*Mod factor NUM */
    ps_ctxt->ai4_mod_factor_derived_by_variance[0] =
        ps_curr_out->ai4_mod_factor_derived_by_variance[0];
    ps_ctxt->ai4_mod_factor_derived_by_variance[1] =
        ps_curr_out->ai4_mod_factor_derived_by_variance[1];

    ps_ctxt->f_strength = ps_curr_out->f_strength;

    if(ps_stat_prms->s_coding_tools_prms.i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER))
    {
        if(ps_stat_prms->s_coding_tools_prms.i4_vqet &
           (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION))
        {
            ps_ctxt->i4_enable_noise_detection = 1;
        }
        else
        {
            ps_ctxt->i4_enable_noise_detection = 0;
        }
    }
    else
    {
        ps_ctxt->i4_enable_noise_detection = 0;
    }

    {
        if(ISLICE == ps_ctxt->i4_slice_type)
        {
            ai4_mod_factor_num[0] = INTRA_QP_MOD_FACTOR_NUM;  //16;
            ai4_mod_factor_num[1] = INTRA_QP_MOD_FACTOR_NUM;  //16;
        }
        else
        {
            ai4_mod_factor_num[0] = INTER_QP_MOD_FACTOR_NUM;  //4;
            ai4_mod_factor_num[1] = INTER_QP_MOD_FACTOR_NUM;  //4;
        }

#if ENABLE_QP_MOD_BASED_ON_SPATIAL_VARIANCE
        for(i4_i = 0; i4_i < 2; i4_i++)
        {
            WORD32 mod_factor_num_val =
                ps_ctxt->ai4_mod_factor_derived_by_variance[i4_i] * QP_MOD_FACTOR_DEN;

            ai4_mod_factor_num[i4_i] = CLIP3(mod_factor_num_val, 1, ai4_mod_factor_num[i4_i]);
            ps_ctxt->ai4_mod_factor_derived_by_variance[i4_i] = ai4_mod_factor_num[i4_i];
        }
#else
        for(i4_i = 0; i4_i < 2; i4_i++)
        {
            ps_ctxt->ai4_mod_factor_derived_by_variance[i4_i] = ai4_mod_factor_num[i4_i];
        }
#endif
    }

    ps_ctxt->u1_use_lambda_derived_from_min_8x8_act_in_ctb = MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON &&
                                                             i1_cu_qp_delta_enabled_flag;

    ps_ctxt->u1_use_satd = 1;
    ps_ctxt->u1_level_1_refine_on = 1;
    ps_ctxt->u1_disable_child_cu_decide = 0;

#if !OLD_XTREME_SPEED
    if(((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P5) ||
        (ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6)) &&
       (ps_ctxt->i4_slice_type != ISLICE))
    {
        ps_ctxt->u1_use_satd = 0;
        ps_ctxt->u1_level_1_refine_on = 1;
        ps_ctxt->u1_disable_child_cu_decide = 0;
    }

#endif

    if((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P4) && (ps_ctxt->i4_slice_type != ISLICE))
        ps_ctxt->u1_use_satd = 0;
    if(ps_ctxt->i4_quality_preset > IHEVCE_QUALITY_P3)
        ps_ctxt->u1_use_satd = 0;
}
