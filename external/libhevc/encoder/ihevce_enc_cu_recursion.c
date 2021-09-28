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
******************************************************************************
* \file ihevce_enc_cu_recursion.c
*
* \brief
*    This file contains Encoder normative loop pass related functions
*
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
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_decomp_pre_intra_structs.h"
#include "ihevce_decomp_pre_intra_pass.h"
#include "ihevce_enc_loop_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_nbr_avail.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_cabac_rdo.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_enc_cu_recursion.h"
#include "ihevce_stasino_helpers.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/
#define NUM_CTB_QUANT_ROUNDING 6

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*********************************************************************************
* Function name : ihevce_store_cu_final
*
* \brief
*    This function store cu info to the enc loop cu context
*
* \param[in] ps_ctxt : pointer to enc loop context structure
* \param[in] ps_cu_final  : pointer to enc loop output CU structure
* \param[in] pu1_ecd_data : ecd data pointer
* \param[in] ps_enc_out_ctxt : pointer to CU information structure
* \param[in] ps_cu_prms : pointer to  cu level parameters for SATD / RDOPT
*
* \return
*    None
*
**********************************************************************************/
void ihevce_store_cu_final(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_enc_loop_out_t *ps_cu_final,
    UWORD8 *pu1_ecd_data,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms)
{
    enc_loop_cu_final_prms_t *ps_enc_loop_bestprms;
    WORD32 i4_8x8_blks_in_cu;
    WORD32 i4_br_id, i4_enc_frm_id;

    WORD32 u4_tex_bits, u4_hdr_bits;
    WORD32 i4_qscale, i4_qscale_ctb;
    ps_enc_loop_bestprms = ps_enc_out_ctxt->ps_cu_prms;
    i4_qscale = ((ps_ctxt->ps_rc_quant_ctxt->pi4_qp_to_qscale
                      [ps_enc_out_ctxt->i1_cu_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]));
    i4_qscale_ctb = ((
        ps_ctxt->ps_rc_quant_ctxt
            ->pi4_qp_to_qscale[ps_ctxt->i4_frame_mod_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]));

    /* All texture bits accumulated */
    u4_tex_bits = ps_enc_loop_bestprms->u4_cu_luma_res_bits +
                  ps_enc_loop_bestprms->u4_cu_chroma_res_bits +
                  ps_enc_loop_bestprms->u4_cu_cbf_bits;

    u4_hdr_bits = ps_enc_loop_bestprms->u4_cu_hdr_bits;

    i4_br_id = ps_ctxt->i4_bitrate_instance_num;
    i4_enc_frm_id = ps_ctxt->i4_enc_frm_id;

    i4_8x8_blks_in_cu = ((ps_enc_out_ctxt->u1_cu_size >> 3) * (ps_enc_out_ctxt->u1_cu_size >> 3));

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_frame_open_loop_ssd +=
        ps_enc_loop_bestprms
            ->i8_cu_ssd;  // + (((float)(ps_ctxt->i8_cl_ssd_lambda_qf/ (1<< LAMBDA_Q_SHIFT))) * ps_enc_loop_bestprms->u4_cu_hdr_bits);

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_open_loop_intra_sad +=
        (UWORD32)(
            ps_enc_loop_bestprms->u4_cu_open_intra_sad +
            (((float)(ps_ctxt->i4_sad_lamda) / (1 << LAMBDA_Q_SHIFT)) *
             ps_enc_loop_bestprms->u4_cu_hdr_bits));

    if(1 == ps_enc_loop_bestprms->u1_intra_flag)
    {
        ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_intra_sad_acc +=
            ps_enc_loop_bestprms->u4_cu_sad;
        ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_frame_intra_cost_acc +=
            ps_enc_loop_bestprms->i8_best_rdopt_cost;
    }
    else
    {
        ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_inter_sad_acc +=
            ps_enc_loop_bestprms->u4_cu_sad;
        ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_frame_inter_cost_acc +=
            ps_enc_loop_bestprms->i8_best_rdopt_cost;
    }
    /*accumulating the frame level stats across frame*/
    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_sad_acc +=
        ps_enc_loop_bestprms->u4_cu_sad;

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_frame_cost_acc +=
        ps_enc_loop_bestprms->i8_best_rdopt_cost;

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_rdopt_bits +=
        (u4_tex_bits + u4_hdr_bits);

    /*Total bits and header bits accumalted here for CTB*/
    ps_ctxt->u4_total_cu_bits += (u4_tex_bits + u4_hdr_bits);
    ps_ctxt->u4_total_cu_bits_mul_qs +=
        ((ULWORD64)((u4_tex_bits + u4_hdr_bits) * (i4_qscale_ctb)) + (1 << (QSCALE_Q_FAC_3 - 1))) >>
        QSCALE_Q_FAC_3;
    ps_ctxt->u4_total_cu_hdr_bits += u4_hdr_bits;
    ps_ctxt->u4_cu_tot_bits_into_qscale +=
        ((ULWORD64)((u4_tex_bits + u4_hdr_bits) * (i4_qscale)) + (1 << (QSCALE_Q_FAC_3 - 1))) >>
        QSCALE_Q_FAC_3;
    ps_ctxt->u4_cu_tot_bits += (u4_tex_bits + u4_hdr_bits);

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_rdopt_header_bits +=
        u4_hdr_bits;

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]
        ->i8_sad_by_qscale[ps_enc_loop_bestprms->u1_intra_flag] +=
        ((((LWORD64)ps_enc_loop_bestprms->u4_cu_sad) << SAD_BY_QSCALE_Q) / i4_qscale);

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]
        ->i4_qp_normalized_8x8_cu_sum[ps_enc_loop_bestprms->u1_intra_flag] +=
        (i4_8x8_blks_in_cu * i4_qscale);

    ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]
        ->i4_8x8_cu_sum[ps_enc_loop_bestprms->u1_intra_flag] += i4_8x8_blks_in_cu;

    /* PCM not supported */
    ps_cu_final->b1_pcm_flag = 0;
    ps_cu_final->b1_pred_mode_flag = ps_enc_loop_bestprms->u1_intra_flag;

    ps_cu_final->b1_skip_flag = ps_enc_loop_bestprms->u1_skip_flag;
    ps_cu_final->b1_tq_bypass_flag = 0;
    ps_cu_final->b3_part_mode = ps_enc_loop_bestprms->u1_part_mode;

    ps_cu_final->pv_coeff = pu1_ecd_data;

    ps_cu_final->i1_cu_qp = ps_enc_out_ctxt->i1_cu_qp;
    if(ps_enc_loop_bestprms->u1_is_cu_coded)
    {
        ps_ctxt->i4_last_cu_qp_from_prev_ctb = ps_enc_out_ctxt->i1_cu_qp;
    }
    else
    {
        ps_ctxt->i4_last_cu_qp_from_prev_ctb = ps_ctxt->i4_pred_qp;
    }
    ps_cu_final->b1_first_cu_in_qg = ps_enc_out_ctxt->b1_first_cu_in_qg;

    /* Update the no residue flag. Needed for inter cu. */
    /* Needed for deblocking inter/intra both           */
    //if(ps_cu_final->b1_pred_mode_flag == PRED_MODE_INTER)
    {
        ps_cu_final->b1_no_residual_syntax_flag = !ps_enc_loop_bestprms->u1_is_cu_coded;
    }

    /* store the number of TUs */
    ps_cu_final->u2_num_tus_in_cu = ps_enc_loop_bestprms->u2_num_tus_in_cu;

    /* ---- copy the TUs to final structure ----- */
    memcpy(
        ps_cu_final->ps_enc_tu,
        &ps_enc_loop_bestprms->as_tu_enc_loop[0],
        ps_enc_loop_bestprms->u2_num_tus_in_cu * sizeof(tu_enc_loop_out_t));

    /* ---- copy the PUs to final structure ----- */
    memcpy(
        ps_cu_final->ps_pu,
        &ps_enc_loop_bestprms->as_pu_enc_loop[0],
        ps_enc_loop_bestprms->u2_num_pus_in_cu * sizeof(pu_t));

    /* --- copy reminder and prev_flags ----- */
    /* only required for intra */
    if(PRED_MODE_INTRA == ps_cu_final->b1_pred_mode_flag)
    {
        memcpy(
            &ps_cu_final->as_prev_rem[0],
            &ps_enc_loop_bestprms->as_intra_prev_rem[0],
            ps_enc_loop_bestprms->u2_num_tus_in_cu * sizeof(intra_prev_rem_flags_t));

        ps_cu_final->b3_chroma_intra_pred_mode = ps_enc_loop_bestprms->u1_chroma_intra_pred_mode;
    }

    /* --------------------------------------------------- */
    /* ---- Boundary Strength Calculation at CU level ---- */
    /* --------------------------------------------------- */
    if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
    {
        WORD32 num_4x4_in_ctb;
        nbr_4x4_t *ps_left_nbr_4x4;
        nbr_4x4_t *ps_top_nbr_4x4;
        nbr_4x4_t *ps_curr_nbr_4x4;
        WORD32 nbr_4x4_left_strd;

        num_4x4_in_ctb = (ps_cu_prms->i4_ctb_size >> 2);

        ps_curr_nbr_4x4 = &ps_ctxt->as_ctb_nbr_arr[0];
        ps_curr_nbr_4x4 += (ps_enc_out_ctxt->b3_cu_pos_x << 1);
        ps_curr_nbr_4x4 += ((ps_enc_out_ctxt->b3_cu_pos_y << 1) * num_4x4_in_ctb);

        /* CU left */
        if(0 == ps_enc_out_ctxt->b3_cu_pos_x)
        {
            ps_left_nbr_4x4 = &ps_ctxt->as_left_col_nbr[0];
            ps_left_nbr_4x4 += ps_enc_out_ctxt->b3_cu_pos_y << 1;
            nbr_4x4_left_strd = 1;
        }
        else
        {
            /* inside CTB */
            ps_left_nbr_4x4 = ps_curr_nbr_4x4 - 1;
            nbr_4x4_left_strd = num_4x4_in_ctb;
        }

        /* CU top */
        if(0 == ps_enc_out_ctxt->b3_cu_pos_y)
        {
            /* CTB boundary */
            ps_top_nbr_4x4 = ps_ctxt->ps_top_row_nbr;
            ps_top_nbr_4x4 += (ps_cu_prms->i4_ctb_pos * (ps_cu_prms->i4_ctb_size >> 2));
            ps_top_nbr_4x4 += (ps_enc_out_ctxt->b3_cu_pos_x << 1);
        }
        else
        {
            /* inside CTB */
            ps_top_nbr_4x4 = ps_curr_nbr_4x4 - num_4x4_in_ctb;
        }

        ihevce_bs_compute_cu(
            ps_cu_final,
            ps_top_nbr_4x4,
            ps_left_nbr_4x4,
            ps_curr_nbr_4x4,
            nbr_4x4_left_strd,
            num_4x4_in_ctb,
            &ps_ctxt->s_deblk_bs_prms);
    }
}

/**
*********************************************************************************
* Function name : ihevce_store_cu_results
*
* \brief
*    This function store cu result to cu info context
*
* \param[in] ps_ctxt : pointer to enc loop context structure
* \param[out] ps_cu_prms : pointer to  cu level parameters for SATD / RDOPT
*
* \return
*    None
*
**********************************************************************************/
void ihevce_store_cu_results(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    final_mode_state_t *ps_final_state)
{
    ihevce_enc_cu_node_ctxt_t *ps_enc_tmp_out_ctxt;
    nbr_4x4_t *ps_nbr_4x4, *ps_tmp_nbr_4x4, *ps_curr_nbr_4x4;

    UWORD8 *pu1_recon, *pu1_final_recon;
    WORD32 num_4x4_in_ctb, ctr;
    WORD32 num_4x4_in_cu;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);
    WORD32 cu_depth, log2_ctb_size, log2_cu_size;

    ps_enc_tmp_out_ctxt = ps_ctxt->ps_enc_out_ctxt;
    (void)ps_final_state;
#if PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    {
        /* ---- copy the child luma recon back to curr. recon -------- */
        pu1_recon = (UWORD8 *)ps_ctxt->pv_cu_luma_recon;

        /* based on CU position derive the luma pointers */
        pu1_final_recon = ps_cu_prms->pu1_luma_recon + (ps_enc_tmp_out_ctxt->b3_cu_pos_x << 3);

        pu1_final_recon +=
            ((ps_enc_tmp_out_ctxt->b3_cu_pos_y << 3) * ps_cu_prms->i4_luma_recon_stride);

        ps_ctxt->s_cmn_opt_func.pf_copy_2d(
            pu1_final_recon,
            ps_cu_prms->i4_luma_recon_stride,
            pu1_recon,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size);

        /* ---- copy the child chroma recon back to curr. recon -------- */
        pu1_recon = (UWORD8 *)ps_ctxt->pv_cu_chrma_recon;

        /* based on CU position derive the chroma pointers */
        pu1_final_recon = ps_cu_prms->pu1_chrm_recon + (ps_enc_tmp_out_ctxt->b3_cu_pos_x << 3);

        pu1_final_recon +=
            ((ps_enc_tmp_out_ctxt->b3_cu_pos_y << (u1_is_422 + 2)) *
             ps_cu_prms->i4_chrm_recon_stride);

        /* Cb and Cr pixel interleaved */
        ps_ctxt->s_cmn_opt_func.pf_copy_2d(
            pu1_final_recon,
            ps_cu_prms->i4_chrm_recon_stride,
            pu1_recon,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            (ps_enc_tmp_out_ctxt->u1_cu_size >> (0 == u1_is_422)));
    }
#else
    if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
    {
        /* ---- copy the child luma recon back to curr. recon -------- */
        pu1_recon = (UWORD8 *)ps_ctxt->pv_cu_luma_recon;

        /* based on CU position derive the luma pointers */
        pu1_final_recon = ps_cu_prms->pu1_luma_recon + (ps_enc_tmp_out_ctxt->b3_cu_pos_x << 3);

        pu1_final_recon +=
            ((ps_enc_tmp_out_ctxt->b3_cu_pos_y << 3) * ps_cu_prms->i4_luma_recon_stride);

        ps_ctxt->s_cmn_opt_func.pf_copy_2d(
            pu1_final_recon,
            ps_cu_prms->i4_luma_recon_stride,
            pu1_recon,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size);

        /* ---- copy the child chroma recon back to curr. recon -------- */
        pu1_recon = (UWORD8 *)ps_ctxt->pv_cu_chrma_recon;

        /* based on CU position derive the chroma pointers */
        pu1_final_recon = ps_cu_prms->pu1_chrm_recon + (ps_enc_tmp_out_ctxt->b3_cu_pos_x << 3);

        pu1_final_recon +=
            ((ps_enc_tmp_out_ctxt->b3_cu_pos_y << (u1_is_422 + 2)) *
             ps_cu_prms->i4_chrm_recon_stride);

        ps_ctxt->s_cmn_opt_func.pf_copy_2d(
            pu1_final_recon,
            ps_cu_prms->i4_chrm_recon_stride,
            pu1_recon,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            ps_enc_tmp_out_ctxt->u1_cu_size,
            (ps_enc_tmp_out_ctxt->u1_cu_size >> (0 == u1_is_422)));
    }
#endif
    /*copy qp for qg*/
    {
        WORD32 i4_num_8x8, i4_x, i4_y;
        WORD32 i4_cu_pos_x, i4_cu_pox_y;
        i4_num_8x8 = ps_enc_tmp_out_ctxt->u1_cu_size >> 3;
        i4_cu_pos_x = ps_enc_tmp_out_ctxt->b3_cu_pos_x;
        i4_cu_pox_y = ps_enc_tmp_out_ctxt->b3_cu_pos_y;
        for(i4_y = 0; i4_y < i4_num_8x8; i4_y++)
        {
            for(i4_x = 0; i4_x < i4_num_8x8; i4_x++)
            {
                if(ps_enc_tmp_out_ctxt->ps_cu_prms->u1_is_cu_coded)
                {
                    ps_ctxt->ai4_qp_qg[((i4_cu_pox_y + i4_y) * 8) + (i4_cu_pos_x + i4_x)] =
                        ps_ctxt->i4_cu_qp;
                }
                else
                {
                    ps_ctxt->ai4_qp_qg[((i4_cu_pox_y + i4_y) * 8) + (i4_cu_pos_x + i4_x)] =
                        ps_ctxt->i4_pred_qp;
                }
            }
        }
    }

    /* ------ copy the nbr 4x4 to final output ------ */
    num_4x4_in_cu = ps_enc_tmp_out_ctxt->u1_cu_size >> 2;
    num_4x4_in_ctb = (ps_cu_prms->i4_ctb_size >> 2);

    ps_curr_nbr_4x4 = &ps_ctxt->as_ctb_nbr_arr[0];
    ps_curr_nbr_4x4 += (ps_enc_tmp_out_ctxt->b3_cu_pos_x << 1);
    ps_curr_nbr_4x4 += ((ps_enc_tmp_out_ctxt->b3_cu_pos_y << 1) * num_4x4_in_ctb);
    ps_tmp_nbr_4x4 = ps_curr_nbr_4x4;

    ps_nbr_4x4 = ps_ctxt->ps_cu_recur_nbr;

    GETRANGE(log2_ctb_size, ps_cu_prms->i4_ctb_size);
    GETRANGE(log2_cu_size, ps_enc_tmp_out_ctxt->u1_cu_size);
    cu_depth = log2_ctb_size - log2_cu_size;

    ASSERT(cu_depth <= 3);
    ASSERT(cu_depth >= 0);

    /*assign qp for all 4x4 nbr blocks*/
    for(ctr = 0; ctr < num_4x4_in_cu * num_4x4_in_cu; ctr++, ps_nbr_4x4++)
    {
        ps_nbr_4x4->b1_skip_flag = ps_enc_tmp_out_ctxt->s_cu_prms.u1_skip_flag;
        ps_nbr_4x4->b2_cu_depth = cu_depth;
        ps_nbr_4x4->b8_qp = ps_ctxt->i4_cu_qp;
    }

    ps_nbr_4x4 = ps_ctxt->ps_cu_recur_nbr;

    for(ctr = 0; ctr < num_4x4_in_cu; ctr++)
    {
        memcpy(ps_tmp_nbr_4x4, ps_nbr_4x4, num_4x4_in_cu * sizeof(nbr_4x4_t));

        ps_tmp_nbr_4x4 += num_4x4_in_ctb;
        ps_nbr_4x4 += num_4x4_in_cu;
    }
}

/**
*********************************************************************************
* Function name : ihevce_populate_cu_struct
*
* \brief
*    This function populate cu struct
*
* \param[in] ps_ctxt : pointer to enc loop context structure
* \param[in] ps_cur_ipe_ctb : pointer to  IPE L0 analyze structure
* \param[in] ps_cu_tree_analyse : pointer to  Structure for CU recursion
* \param[in] ps_best_results : pointer to  strcuture  contain result for partition type of CU
* \param[in] ps_cu_out : pointer to  structre contain  mode analysis info
* \param[in] i4_32x32_id : noise estimation id
* \param[in] u1_num_best_results : num best result value
*
* \return
*    None
*
**********************************************************************************/
void ihevce_populate_cu_struct(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse,
    part_type_results_t *ps_best_results,
    cu_analyse_t *ps_cu_out,
    WORD32 i4_32x32_id,
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_num_best_results)
{
    cu_inter_cand_t *ps_cu_candt;

    WORD32 j;
    /* open loop intra cost by IPE */
    WORD32 intra_cost_ol;
    /* closed loop intra cost based on empirical coding noise estimate */
    WORD32 intra_cost_cl_est = 0;
    /* closed loop intra coding noise estimate */
    WORD32 intra_noise_cl_est;
    WORD32 num_results_to_copy = 0;

    WORD32 found_intra = 0;
    WORD32 quality_preset = ps_ctxt->i4_quality_preset;
    WORD32 frm_qp = ps_ctxt->i4_frame_qp;
    WORD32 frm_qstep_multiplier = gau4_frame_qstep_multiplier[frm_qp - 1];
    WORD32 frm_qstep = ps_ctxt->i4_frame_qstep;
    UWORD8 u1_cu_size = ps_cu_tree_analyse->u1_cu_size;
    UWORD8 u1_x_off = ps_cu_tree_analyse->b3_cu_pos_x << 3;
    UWORD8 u1_y_off = ps_cu_tree_analyse->b3_cu_pos_y << 3;
    UWORD8 u1_threshold_multi;
    switch(quality_preset)
    {
    case IHEVCE_QUALITY_P0:
    case IHEVCE_QUALITY_P2:
    {
        num_results_to_copy =
            MIN(MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_PQ_AND_HQ, u1_num_best_results);
        break;
    }
    case IHEVCE_QUALITY_P3:
    {
        num_results_to_copy = MIN(MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_MS, u1_num_best_results);
        break;
    }
    case IHEVCE_QUALITY_P4:
    case IHEVCE_QUALITY_P5:
    case IHEVCE_QUALITY_P6:
    {
        num_results_to_copy =
            MIN(MAX_NUMBER_OF_INTER_RDOPT_CANDS_IN_HS_AND_XS, u1_num_best_results);
        break;
    }
    }

    ps_cu_out->u1_num_inter_cands = 0;

    /***************************************************************/
    /* Depending CU size that has won in ME,                       */
    /*     Estimate the closed loop intra cost for enabling intra  */
    /*     evaluation in rdopt stage based on preset               */
    /***************************************************************/
    switch(u1_cu_size)
    {
    case 64:
    {
        /* coding noise estimate for intra closed loop cost */
        intra_cost_ol = ps_cur_ipe_ctb->i4_best64x64_intra_cost - frm_qstep * 256;

        intra_noise_cl_est = (frm_qstep * frm_qstep_multiplier) + (intra_cost_ol >> 4);

        intra_noise_cl_est = MIN(intra_noise_cl_est, (frm_qstep * 16)) * 16;

        intra_cost_cl_est = intra_cost_ol + intra_noise_cl_est;
        break;
    }
    case 32:
    {
        /* coding noise estimate for intra closed loop cost */
        intra_cost_ol = ps_cur_ipe_ctb->ai4_best32x32_intra_cost[i4_32x32_id] - frm_qstep * 64;

        intra_noise_cl_est = (frm_qstep * frm_qstep_multiplier) + (intra_cost_ol >> 4);

        intra_noise_cl_est = MIN(intra_noise_cl_est, (frm_qstep * 16)) * 4;

        intra_cost_cl_est = intra_cost_ol + intra_noise_cl_est;
        break;
    }
    case 16:
    {
        /* coding noise estimate for intra closed loop cost */
        intra_cost_ol =
            ps_cur_ipe_ctb->ai4_best16x16_intra_cost[(u1_x_off >> 4) + ((u1_y_off >> 4) << 2)] -
            frm_qstep * 16;

        intra_noise_cl_est = (frm_qstep * frm_qstep_multiplier) + (intra_cost_ol >> 4);

        intra_noise_cl_est = MIN(intra_noise_cl_est, (frm_qstep * 16));

        intra_cost_cl_est = intra_cost_ol + intra_noise_cl_est;
        break;
    }
    case 8:
    {
        /* coding noise estimate for intra closed loop cost */
        intra_cost_ol =
            ps_cur_ipe_ctb->ai4_best8x8_intra_cost[(u1_x_off >> 3) + u1_y_off] - frm_qstep * 4;

        intra_noise_cl_est = (frm_qstep * frm_qstep_multiplier) + (intra_cost_ol >> 4);

        intra_noise_cl_est = MIN(intra_noise_cl_est, (frm_qstep * 16)) >> 2;

        intra_cost_cl_est = intra_cost_ol + intra_noise_cl_est;
        break;
    }
    }
#if DISABLE_INTER_CANDIDATES
    return;
#endif

    u1_threshold_multi = 1;
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
    if(u1_is_cu_noisy)
    {
        intra_cost_cl_est = INT_MAX;
    }
#endif

    ps_cu_candt = ps_cu_out->as_cu_inter_cand;

    /* Check if the first best candidate is inter or intra */
    if(ps_best_results[0].as_pu_results[0].pu.b1_intra_flag)
    {
        ps_cu_out->u1_best_is_intra = 1;
    }
    else
    {
        ps_cu_out->u1_best_is_intra = 0;
    }

    for(j = 0; j < u1_num_best_results; j++)
    {
        part_type_results_t *ps_best = &ps_best_results[j];

        if(ps_best->as_pu_results[0].pu.b1_intra_flag)
        {
            found_intra = 1;
        }
        else
        {
            /* populate the TU split flags, 4 flags copied as max cu can be 64 */
            memcpy(ps_cu_candt->ai4_tu_split_flag, ps_best->ai4_tu_split_flag, 4 * sizeof(WORD32));

            /* populate the TU early CBF flags, 4 flags copied as max cu can be 64 */
            memcpy(ps_cu_candt->ai4_tu_early_cbf, ps_best->ai4_tu_early_cbf, 4 * sizeof(WORD32));

            /* Note: the enums of part size and me part types shall match */
            ps_cu_candt->b3_part_size = ps_best->u1_part_type;

            /* ME will always set the skip flag to 0            */
            /* in closed loop skip will be added as a candidate */
            ps_cu_candt->b1_skip_flag = 0;

            /* copy the inter pus : Note: assuming NxN part type is not supported */
            ps_cu_candt->as_inter_pu[0] = ps_best->as_pu_results[0].pu;

            ps_cu_candt->as_inter_pu[0].b1_merge_flag = 0;

            /* Copy the total cost of the CU candt */
            ps_cu_candt->i4_total_cost = ps_best->i4_tot_cost;

            ps_cu_out->ai4_mv_cost[ps_cu_out->u1_num_inter_cands][0] =
                ps_best->as_pu_results[0].i4_mv_cost;

#if REUSE_ME_COMPUTED_ERROR_FOR_INTER_CAND_SIFTING
            ps_cu_out->ai4_err_metric[ps_cu_out->u1_num_inter_cands][0] =
                ps_best->as_pu_results[0].i4_tot_cost - ps_best->as_pu_results[0].i4_mv_cost;
#endif

            if(ps_best->u1_part_type)
            {
                ps_cu_candt->as_inter_pu[1] = ps_best->as_pu_results[1].pu;
                ps_cu_out->ai4_mv_cost[ps_cu_out->u1_num_inter_cands][1] =
                    ps_best->as_pu_results[1].i4_mv_cost;
#if REUSE_ME_COMPUTED_ERROR_FOR_INTER_CAND_SIFTING
                ps_cu_out->ai4_err_metric[ps_cu_out->u1_num_inter_cands][1] =
                    ps_best->as_pu_results[1].i4_tot_cost - ps_best->as_pu_results[1].i4_mv_cost;
#endif

                ps_cu_candt->as_inter_pu[1].b1_merge_flag = 0;
            }

            ps_cu_candt++;
            ps_cu_out->u1_num_inter_cands++;
            if(intra_cost_cl_est < ((ps_best->i4_tot_cost * u1_threshold_multi) >> 0))
            {
                /* The rationale - */
                /* Artefacts were being observed in some sequences, */
                /* Brooklyn_1080p in particular - where it was readily */
                /* apparent. The cause was coding of CU's as inter CU's */
                /* when they actually needed to be coded as intra CU's. */
                /* This was observed during either fade-outs aor flashes. */
                /* After tinkering with the magnitude of the coding noise */
                /* factor that was added to the intra cost to see when the */
                /* artefacts in Brooklyn vanished, it was observed that the */
                /* factor multiplied with the frame_qstep followed a pattern. */
                /* When the pattern was subjected to a regression analysis, the */
                /* formula seen below emerged. Also note the fact that the coding */
                /* noise factor is the product of the frame_qstep and a constant */
                /* multiplier */

                /*UWORD32 frm_qstep_multiplier =
                -3.346 * log((float)frm_qstep) + 15.925;*/
                found_intra = 1;
            }

            if(ps_cu_out->u1_num_inter_cands >= num_results_to_copy)
            {
                break;
            }
        }
    }

    if(quality_preset < IHEVCE_QUALITY_P4)
    {
        found_intra = 1;
    }

    if(!found_intra)
    {
        /* rdopt evaluation of intra disabled as inter is clear winner */
        ps_cu_out->u1_num_intra_rdopt_cands = 0;

        /* all the modes invalidated */
        ps_cu_out->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
        ps_cu_out->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
        ps_cu_out->s_cu_intra_cand.au1_intra_luma_modes_nxn[0][0] = 255;
        ps_cu_out->u1_chroma_intra_pred_mode = 255;

        /* no intra candt to verify */
        ps_cu_out->s_cu_intra_cand.b6_num_intra_cands = 0;
    }
}

/**
*********************************************************************************
* Function name : ihevce_create_child_nodes_cu_tree
*
* \brief
*    This function create child node from cu tree
*
* \param[in] ps_cu_tree_root : pointer to Structure for CU recursion
* \param[out] ps_cu_tree_cur_node : pointer to  Structure for CU recursion
* \param[in] ai4_child_node_enable : child node enable flag
* \param[in] nodes_already_created : already created node value
* \return
*    None
*
**********************************************************************************/
WORD32 ihevce_create_child_nodes_cu_tree(
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    cur_ctb_cu_tree_t *ps_cu_tree_cur_node,
    WORD32 *ai4_child_node_enable,
    WORD32 nodes_already_created)
{
    cur_ctb_cu_tree_t *ps_tl;
    cur_ctb_cu_tree_t *ps_tr;
    cur_ctb_cu_tree_t *ps_bl;
    cur_ctb_cu_tree_t *ps_br;

    ps_tl = ps_cu_tree_root + nodes_already_created;
    ps_tr = ps_tl + 1;
    ps_bl = ps_tr + 1;
    ps_br = ps_bl + 1;

    if(1 == ps_cu_tree_cur_node->is_node_valid)
    {
        ps_tl = (ai4_child_node_enable[0]) ? ps_tl : NULL;
        ps_tr = (ai4_child_node_enable[1]) ? ps_tr : NULL;
        ps_bl = (ai4_child_node_enable[2]) ? ps_bl : NULL;
        ps_br = (ai4_child_node_enable[3]) ? ps_br : NULL;

        /* In incomplete CTB, if any of the child nodes are assigned to NULL */
        /* then parent node ceases to be valid */
        if((ps_tl == NULL) || (ps_tr == NULL) || (ps_br == NULL) || (ps_bl == NULL))
        {
            ps_cu_tree_cur_node->is_node_valid = 0;
        }
    }
    ps_cu_tree_cur_node->ps_child_node_tl = ps_tl;
    ps_cu_tree_cur_node->ps_child_node_tr = ps_tr;
    ps_cu_tree_cur_node->ps_child_node_bl = ps_bl;
    ps_cu_tree_cur_node->ps_child_node_br = ps_br;

    return 4;
}

/**
*********************************************************************************
* Function name : ihevce_populate_cu_tree
*
* \brief
*    This function create child node from cu tree
*
* \param[in] ps_cur_ipe_ctb : pointer to Structure for CU recursion
* \param[out] ps_cu_tree : pointer to  Structure for CU recursion
* \param[in] tree_depth : child node enable flag
* \param[in] e_quality_preset : already created node value
* \param[in] e_grandparent_blk_pos : already created node value
* \param[in] e_parent_blk_pos : already created node value
* \param[in] e_cur_blk_pos : already created node value
*
* \return
*    None
*
**********************************************************************************/
void ihevce_populate_cu_tree(
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    cur_ctb_cu_tree_t *ps_cu_tree,
    WORD32 tree_depth,
    IHEVCE_QUALITY_CONFIG_T e_quality_preset,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos)
{
    WORD32 ai4_child_enable[4];
    WORD32 children_nodes_required = 0;
    WORD32 cu_pos_x = 0;
    WORD32 cu_pos_y = 0;
    WORD32 cu_size = 0;
    WORD32 i;
    WORD32 node_validity = 0;

    if(NULL == ps_cu_tree)
    {
        return;
    }

    switch(tree_depth)
    {
    case 0:
    {
        /* 64x64 block */
        intra32_analyse_t *ps_intra32_analyse = ps_cur_ipe_ctb->as_intra32_analyse;

        children_nodes_required = 1;
        cu_size = 64;
        cu_pos_x = 0;
        cu_pos_y = 0;

        node_validity = !ps_cur_ipe_ctb->u1_split_flag;

        if(e_quality_preset >= IHEVCE_QUALITY_P2)
        {
            if(node_validity == 1)
            {
                children_nodes_required = 0;
            }
        }

        for(i = 0; i < 4; i++)
        {
            ai4_child_enable[i] = ps_intra32_analyse[i].b1_valid_cu;
        }

        break;
    }
    case 1:
    {
        /* 32x32 block */
        WORD32 valid_flag_32 = (ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].b1_valid_cu);

        intra16_analyse_t *ps_intra16_analyse =
            ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].as_intra16_analyse;

        cu_size = 32;

        /* Explanation for logic below - */
        /* * pos_x and pos_y are in units of 8x8 CU's */
        /* * pos_x = 0 for TL and BL children */
        /* * pos_x = 4 for TR and BR children */
        /* * pos_y = 0 for TL and TR children */
        /* * pos_y = 4 for BL and BR children */
        cu_pos_x = (e_cur_blk_pos & 1) << 2;
        cu_pos_y = (e_cur_blk_pos & 2) << 1;

        {
            node_validity = (ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].b1_merge_flag);

            if(e_quality_preset >= IHEVCE_QUALITY_P2)
            {
                node_validity = (!ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].b1_split_flag);
            }

            node_validity = node_validity && valid_flag_32;
            children_nodes_required = !node_validity || ps_cur_ipe_ctb->u1_split_flag;
        }

        if(e_quality_preset >= IHEVCE_QUALITY_P2)
        {
            if(node_validity == 1)
            {
                children_nodes_required = 0;
            }
            else
            {
                children_nodes_required =
                    (ps_cur_ipe_ctb->as_intra32_analyse[e_cur_blk_pos].b1_split_flag);
            }
        }

        for(i = 0; i < 4; i++)
        {
            ai4_child_enable[i] = ps_intra16_analyse[i].b1_valid_cu;
        }

        break;
    }
    case 2:
    {
        /* 16x16 block */
        WORD32 cu_pos_x_parent;
        WORD32 cu_pos_y_parent;
        WORD32 merge_flag_16;
        WORD32 merge_flag_32;

        intra8_analyse_t *ps_intra8_analyse = ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos]
                                                  .as_intra16_analyse[e_cur_blk_pos]
                                                  .as_intra8_analyse;

        WORD32 valid_flag_16 = (ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos]
                                    .as_intra16_analyse[e_cur_blk_pos]
                                    .b1_valid_cu);

        cu_size = 16;

        /* Explanation for logic below - */
        /* See similar explanation above */
        cu_pos_x_parent = (e_parent_blk_pos & 1) << 2;
        cu_pos_y_parent = (e_parent_blk_pos & 2) << 1;
        cu_pos_x = cu_pos_x_parent + ((e_cur_blk_pos & 1) << 1);
        cu_pos_y = cu_pos_y_parent + (e_cur_blk_pos & 2);

        merge_flag_16 = (ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos]
                             .as_intra16_analyse[e_cur_blk_pos]
                             .b1_merge_flag);
        merge_flag_32 = (ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos].b1_merge_flag);

#if !ENABLE_UNIFORM_CU_SIZE_8x8
        node_validity = (merge_flag_16) || ((ps_cur_ipe_ctb->u1_split_flag) && (!merge_flag_32));
#else
        node_validity = 0;
#endif

        node_validity = (merge_flag_16) || ((ps_cur_ipe_ctb->u1_split_flag) && (!merge_flag_32));

        if(e_quality_preset >= IHEVCE_QUALITY_P2)
        {
            node_validity = (!ps_cur_ipe_ctb->as_intra32_analyse[e_parent_blk_pos]
                                  .as_intra16_analyse[e_cur_blk_pos]
                                  .b1_split_flag);
        }

        node_validity = node_validity && valid_flag_16;

        children_nodes_required = ((ps_cur_ipe_ctb->u1_split_flag) && (!merge_flag_32)) ||
                                  !merge_flag_16;

        if(e_quality_preset >= IHEVCE_QUALITY_P2)
        {
            children_nodes_required = !node_validity;
        }

        for(i = 0; i < 4; i++)
        {
            ai4_child_enable[i] = ps_intra8_analyse[i].b1_valid_cu;
        }
        break;
    }
    case 3:
    {
        /* 8x8 block */
        WORD32 cu_pos_x_grandparent;
        WORD32 cu_pos_y_grandparent;

        WORD32 cu_pos_x_parent;
        WORD32 cu_pos_y_parent;

        WORD32 valid_flag_8 = (ps_cur_ipe_ctb->as_intra32_analyse[e_grandparent_blk_pos]
                                   .as_intra16_analyse[e_parent_blk_pos]
                                   .as_intra8_analyse[e_cur_blk_pos]
                                   .b1_valid_cu);

        cu_size = 8;

        cu_pos_x_grandparent = (e_grandparent_blk_pos & 1) << 2;
        cu_pos_y_grandparent = (e_grandparent_blk_pos & 2) << 1;
        cu_pos_x_parent = cu_pos_x_grandparent + ((e_parent_blk_pos & 1) << 1);
        cu_pos_y_parent = cu_pos_y_grandparent + (e_parent_blk_pos & 2);
        cu_pos_x = cu_pos_x_parent + (e_cur_blk_pos & 1);
        cu_pos_y = cu_pos_y_parent + ((e_cur_blk_pos & 2) >> 1);

        node_validity = 1 && valid_flag_8;

        children_nodes_required = 0;

        break;
    }
    }

    /* Fill the current cu_tree node */
    ps_cu_tree->is_node_valid = node_validity;
    ps_cu_tree->u1_cu_size = cu_size;
    ps_cu_tree->b3_cu_pos_x = cu_pos_x;
    ps_cu_tree->b3_cu_pos_y = cu_pos_y;

    if(children_nodes_required)
    {
        tree_depth++;

        ps_cur_ipe_ctb->nodes_created_in_cu_tree += ihevce_create_child_nodes_cu_tree(
            ps_cur_ipe_ctb->ps_cu_tree_root,
            ps_cu_tree,
            ai4_child_enable,
            ps_cur_ipe_ctb->nodes_created_in_cu_tree);

        ihevce_populate_cu_tree(
            ps_cur_ipe_ctb,
            ps_cu_tree->ps_child_node_tl,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TL);

        ihevce_populate_cu_tree(
            ps_cur_ipe_ctb,
            ps_cu_tree->ps_child_node_tr,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TR);

        ihevce_populate_cu_tree(
            ps_cur_ipe_ctb,
            ps_cu_tree->ps_child_node_bl,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BL);

        ihevce_populate_cu_tree(
            ps_cur_ipe_ctb,
            ps_cu_tree->ps_child_node_br,
            tree_depth,
            e_quality_preset,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BR);
    }
    else
    {
        ps_cu_tree->ps_child_node_tl = NULL;
        ps_cu_tree->ps_child_node_tr = NULL;
        ps_cu_tree->ps_child_node_bl = NULL;
        ps_cu_tree->ps_child_node_br = NULL;
    }
}

/**
*********************************************************************************
* Function name : ihevce_intra_mode_populator
*
* \brief
*    This function populate intra mode info to strcut
*
* \param[in] ps_cu_intra_cand : pointer to Structure contain cu intra candidate info
* \param[out] ps_ipe_data : pointer to  IPE L0 analyze structure
* \param[in] ps_cu_tree_data : poniter to cu recursive struct
* \param[in] i1_slice_type : contain slice type value
* \param[in] i4_quality_preset : contain quality preset value
*
* \return
*    None
*
**********************************************************************************/
static void ihevce_intra_mode_populator(
    cu_intra_cand_t *ps_cu_intra_cand,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_data,
    cur_ctb_cu_tree_t *ps_cu_tree_data,
    WORD8 i1_slice_type,
    WORD32 i4_quality_preset)
{
    WORD32 i4_32x32_id, i4_16x16_id, i4_8x8_id;

    UWORD8 u1_cu_pos_x = ps_cu_tree_data->b3_cu_pos_x;
    UWORD8 u1_cu_pos_y = ps_cu_tree_data->b3_cu_pos_y;

    i4_32x32_id = ((u1_cu_pos_x & 4) >> 2) + ((u1_cu_pos_y & 4) >> 1);

    i4_16x16_id = ((u1_cu_pos_x & 2) >> 1) + ((u1_cu_pos_y & 2));

    i4_8x8_id = (u1_cu_pos_x & 1) + ((u1_cu_pos_y & 1) << 1);

    if(i4_quality_preset < IHEVCE_QUALITY_P3)
    {
        switch(ps_cu_tree_data->u1_cu_size)
        {
        case 64:
        {
            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_ipe_data->au1_best_modes_32x32_tu,
                MAX_INTRA_CU_CANDIDATES + 1);

            break;
        }
        case 32:
        {
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_32x32_ipe_analyze->au1_best_modes_32x32_tu,
                MAX_INTRA_CU_CANDIDATES + 1);

            if((i1_slice_type != ISLICE) && (i4_quality_preset == IHEVCE_QUALITY_P0))
            {
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
            }
            else if((i1_slice_type == ISLICE) && (i4_quality_preset == IHEVCE_QUALITY_P0))
            {
                if((ps_cu_tree_data->ps_child_node_bl != NULL) &&
                   (ps_cu_tree_data->ps_child_node_bl->is_node_valid))
                {
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
                }
                else
                {
                    memcpy(
                        ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                        ps_32x32_ipe_analyze->au1_best_modes_16x16_tu,
                        MAX_INTRA_CU_CANDIDATES + 1);
                }
            }
            else
            {
                memcpy(
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                    ps_32x32_ipe_analyze->au1_best_modes_16x16_tu,
                    MAX_INTRA_CU_CANDIDATES + 1);
            }

            break;
        }
        case 16:
        {
            /* Copy best 16x16 CU modes */
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_16x16_ipe_analyze->au1_best_modes_16x16_tu,
                MAX_INTRA_CU_CANDIDATES + 1);

            if((i1_slice_type != ISLICE) && (i4_quality_preset == IHEVCE_QUALITY_P0))
            {
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
            }
            else if((i1_slice_type == ISLICE) && (i4_quality_preset == IHEVCE_QUALITY_P0))
            {
                if((ps_cu_tree_data->ps_child_node_bl != NULL) &&
                   (ps_cu_tree_data->ps_child_node_bl->is_node_valid))
                {
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
                }
                else
                {
                    memcpy(
                        ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                        ps_16x16_ipe_analyze->au1_best_modes_8x8_tu,
                        MAX_INTRA_CU_CANDIDATES + 1);
                }
            }
            else
            {
                memcpy(
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                    ps_16x16_ipe_analyze->au1_best_modes_8x8_tu,
                    MAX_INTRA_CU_CANDIDATES + 1);
            }

            break;
        }
        case 8:
        {
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            intra8_analyse_t *ps_8x8_ipe_analyze =
                &ps_16x16_ipe_analyze->as_intra8_analyse[i4_8x8_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_8x8_ipe_analyze->au1_best_modes_8x8_tu,
                MAX_INTRA_CU_CANDIDATES + 1);

            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;

            /* Initialise the hash */
            {
                WORD32 i, j;

                for(i = 0; i < NUM_PU_PARTS; i++)
                {
                    ps_cu_intra_cand->au1_num_modes_added[i] = 0;

                    for(j = 0; j < MAX_INTRA_CANDIDATES; j++)
                    {
                        ps_cu_intra_cand->au1_intra_luma_mode_nxn_hash[i][j] = 0;
                    }
                }

                for(i = 0; i < NUM_PU_PARTS; i++)
                {
                    for(j = 0; j < MAX_INTRA_CU_CANDIDATES; j++)
                    {
                        if(ps_8x8_ipe_analyze->au1_4x4_best_modes[i][j] == 255)
                        {
                            ps_cu_intra_cand->au1_intra_luma_modes_nxn[i][j] = 255;
                            break;
                        }

                        ps_cu_intra_cand->au1_intra_luma_modes_nxn[i][j] =
                            ps_8x8_ipe_analyze->au1_4x4_best_modes[i][j];

                        ps_cu_intra_cand->au1_intra_luma_mode_nxn_hash
                            [i][ps_8x8_ipe_analyze->au1_4x4_best_modes[i][j]] = 1;

                        ps_cu_intra_cand->au1_num_modes_added[i]++;
                    }

                    if(ps_cu_intra_cand->au1_num_modes_added[i] == MAX_INTRA_CU_CANDIDATES)
                    {
                        if(i1_slice_type != BSLICE)
                        {
                            ps_cu_intra_cand->au1_num_modes_added[i] =
                                ihevce_intra_mode_nxn_hash_updater(
                                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[i],
                                    ps_cu_intra_cand->au1_intra_luma_mode_nxn_hash[i],
                                    ps_cu_intra_cand->au1_num_modes_added[i]);
                        }
                    }
                }
            }

            break;
        }
        }
    }
    else if(i4_quality_preset == IHEVCE_QUALITY_P6)
    {
        switch(ps_cu_tree_data->u1_cu_size)
        {
        case 64:
        {
            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_ipe_data->au1_best_modes_32x32_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            ps_cu_intra_cand->b1_eval_tx_cusize = 0;
            ps_cu_intra_cand->b1_eval_tx_cusize_by2 = 1;
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;

#if ENABLE_INTRA_MODE_FILTERING_IN_XS25
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
#endif

            break;
        }
        case 32:
        {
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_32x32_ipe_analyze->au1_best_modes_32x32_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_32x32_ipe_analyze->au1_best_modes_16x16_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

#if ENABLE_INTRA_MODE_FILTERING_IN_XS25
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
#endif

            break;
        }
        case 16:
        {
            /* Copy best 16x16 CU modes */
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_16x16_ipe_analyze->au1_best_modes_16x16_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_16x16_ipe_analyze->au1_best_modes_8x8_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

#if ENABLE_INTRA_MODE_FILTERING_IN_XS25
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
#endif

            break;
        }
        case 8:
        {
            WORD32 i;

            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            intra8_analyse_t *ps_8x8_ipe_analyze =
                &ps_16x16_ipe_analyze->as_intra8_analyse[i4_8x8_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_8x8_ipe_analyze->au1_best_modes_8x8_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

#if !ENABLE_INTRA_MODE_FILTERING_IN_XS25
            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_8x8_ipe_analyze->au1_best_modes_4x4_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            for(i = 0; i < 4; i++)
            {
                memcpy(
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[i],
                    ps_8x8_ipe_analyze->au1_4x4_best_modes[i],
                    (NUM_BEST_MODES + 1) * sizeof(UWORD8));

                ps_cu_intra_cand->au1_intra_luma_modes_nxn[i][MAX_INTRA_CU_CANDIDATES] = 255;
            }
#else
            if(255 == ps_8x8_ipe_analyze->au1_4x4_best_modes[0][0])
            {
                memcpy(
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                    ps_8x8_ipe_analyze->au1_best_modes_4x4_tu,
                    (NUM_BEST_MODES + 1) * sizeof(UWORD8));

                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2
                    [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
            }
            else
            {
                for(i = 0; i < 4; i++)
                {
                    memcpy(
                        ps_cu_intra_cand->au1_intra_luma_modes_nxn[i],
                        ps_8x8_ipe_analyze->au1_4x4_best_modes[i],
                        (NUM_BEST_MODES + 1) * sizeof(UWORD8));

                    ps_cu_intra_cand->au1_intra_luma_modes_nxn
                        [i][MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
                }
            }

            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu
                [MAX_NUM_INTRA_MODES_PER_TU_DISTRIBUTION_IN_XS25] = 255;
#endif

#if FORCE_NXN_MODE_BASED_ON_OL_IPE
            if((i4_quality_preset == IHEVCE_QUALITY_P6) && (i1_slice_type != ISLICE))
            {
                /*Evaluate nxn mode for 8x8 if ol ipe wins for nxn over cu=tu and cu=4tu.*/
                /*Disbale CU=TU and CU=4TU modes */
                if(ps_8x8_ipe_analyze->b1_enable_nxn == 1)
                {
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
                    ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[0][1] = 255;
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[1][1] = 255;
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[2][1] = 255;
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[3][1] = 255;
                }
            }
#endif

            break;
        }
        }
    }
    else
    {
        switch(ps_cu_tree_data->u1_cu_size)
        {
        case 64:
        {
            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_ipe_data->au1_best_modes_32x32_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            ps_cu_intra_cand->b1_eval_tx_cusize = 0;
            ps_cu_intra_cand->b1_eval_tx_cusize_by2 = 1;
            ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;

            break;
        }
        case 32:
        {
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_32x32_ipe_analyze->au1_best_modes_32x32_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_32x32_ipe_analyze->au1_best_modes_16x16_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            break;
        }
        case 16:
        {
            /* Copy best 16x16 CU modes */
            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_16x16_ipe_analyze->au1_best_modes_16x16_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_16x16_ipe_analyze->au1_best_modes_8x8_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            break;
        }
        case 8:
        {
            WORD32 i;

            intra32_analyse_t *ps_32x32_ipe_analyze = &ps_ipe_data->as_intra32_analyse[i4_32x32_id];

            intra16_analyse_t *ps_16x16_ipe_analyze =
                &ps_32x32_ipe_analyze->as_intra16_analyse[i4_16x16_id];

            intra8_analyse_t *ps_8x8_ipe_analyze =
                &ps_16x16_ipe_analyze->as_intra8_analyse[i4_8x8_id];

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu,
                ps_8x8_ipe_analyze->au1_best_modes_8x8_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            memcpy(
                ps_cu_intra_cand->au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2,
                ps_8x8_ipe_analyze->au1_best_modes_4x4_tu,
                (NUM_BEST_MODES + 1) * sizeof(UWORD8));

            for(i = 0; i < 4; i++)
            {
                memcpy(
                    ps_cu_intra_cand->au1_intra_luma_modes_nxn[i],
                    ps_8x8_ipe_analyze->au1_4x4_best_modes[i],
                    (NUM_BEST_MODES + 1) * sizeof(UWORD8));

                ps_cu_intra_cand->au1_intra_luma_modes_nxn[i][MAX_INTRA_CU_CANDIDATES] = 255;
            }

            break;
        }
        }
    }
}
/**
******************************************************************************
* \if Function name : ihevce_compute_rdo \endif
*
* \brief
*    Coding Unit mode decide function. Performs RD opt and decides the best mode
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_cu_prms  : pointer to coding unit params (position, buffer pointers)
* \param[in] ps_cu_analyse : pointer to cu analyse
* \param[out] ps_cu_final : pointer to cu final
* \param[out] pu1_ecd_data :pointer to store coeff data for ECD
* \param[out]ps_row_col_pu; colocated pu buffer pointer
* \param[out]pu1_row_pu_map; colocated pu map buffer pointer
* \param[in]col_start_pu_idx : pu index start value
*
* \return
*    None
*
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_compute_rdo(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    me_ctb_data_t *ps_cu_me_data,
    pu_col_mv_t *ps_col_pu,
    final_mode_state_t *ps_final_mode_state,
    UWORD8 *pu1_col_pu_map,
    UWORD8 *pu1_ecd_data,
    WORD32 col_start_pu_idx,
    WORD32 i4_ctb_x_off,
    WORD32 i4_ctb_y_off)
{
    /* Populate the rdo candiates to the structure */
    cu_analyse_t s_cu_analyse;
    LWORD64 rdopt_best_cost;
    /* Populate candidates of child nodes to CU analyse struct for further evaluation */
    cu_analyse_t *ps_cu_analyse;
    WORD32 curr_cu_pos_in_row;
    WORD32 cu_top_right_offset, cu_top_right_dep_pos;
    WORD32 is_first_cu_in_ctb, is_ctb_level_quant_rounding, is_nctb_level_quant_rounding;

    WORD32 cu_pos_x = ps_cu_tree_analyse->b3_cu_pos_x;
    WORD32 cu_pos_y = ps_cu_tree_analyse->b3_cu_pos_y;

    /*Derive the indices of 32*32, 16*16 and 8*8 blocks*/
    WORD32 i4_32x32_id = ((cu_pos_x & 4) >> 2) + ((cu_pos_y & 4) >> 1);

    WORD32 i4_16x16_id = ((cu_pos_x & 2) >> 1) + ((cu_pos_y & 2));

    WORD32 i4_8x8_id = (cu_pos_x & 1) + ((cu_pos_y & 1) << 1);
    if(i4_ctb_y_off == 0)
    {
        /* No wait for 1st row */
        cu_top_right_offset = -(MAX_CTB_SIZE);
        {
            ihevce_tile_params_t *ps_col_tile_params =
                ((ihevce_tile_params_t *)ps_ctxt->pv_tile_params_base + ps_ctxt->i4_tile_col_idx);

            cu_top_right_offset = -(ps_col_tile_params->i4_first_sample_x + (MAX_CTB_SIZE));
        }

        cu_top_right_dep_pos = 0;
    }
    else
    {
        cu_top_right_offset = ps_cu_tree_analyse->u1_cu_size << 1;
        cu_top_right_dep_pos = (i4_ctb_y_off >> 6) - 1;
    }
    ps_cu_analyse = &s_cu_analyse;

    ps_cu_analyse->b3_cu_pos_x = cu_pos_x;
    ps_cu_analyse->b3_cu_pos_y = cu_pos_y;
    ps_cu_analyse->u1_cu_size = ps_cu_tree_analyse->u1_cu_size;

    /* Default initializations */
    ps_cu_analyse->u1_num_intra_rdopt_cands = MAX_INTRA_CU_CANDIDATES;
    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_nxn[0][0] = 255;
    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;

    ps_cu_analyse->s_cu_intra_cand.b1_eval_tx_cusize = 1;
    ps_cu_analyse->s_cu_intra_cand.b1_eval_tx_cusize_by2 = 1;

    switch(ps_cu_tree_analyse->u1_cu_size)
    {
    case 64:
    {
        memcpy(
            ps_cu_analyse[0].i4_act_factor,
            ps_cur_ipe_ctb->i4_64x64_act_factor,
            4 * 2 * sizeof(WORD32));

        ps_cu_analyse[0].s_cu_intra_cand.b1_eval_tx_cusize = 0;
        ps_cu_analyse[0].s_cu_intra_cand.b1_eval_tx_cusize_by2 = 1;
        ps_cu_analyse[0].s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;

        break;
    }
    case 32:
    {
        memcpy(
            ps_cu_analyse[0].i4_act_factor,
            ps_cur_ipe_ctb->i4_32x32_act_factor[i4_32x32_id],
            3 * 2 * sizeof(WORD32));

        break;
    }
    case 16:
    {
        memcpy(
            ps_cu_analyse[0].i4_act_factor,
            ps_cur_ipe_ctb->i4_16x16_act_factor[(i4_32x32_id << 2) + i4_16x16_id],
            2 * 2 * sizeof(WORD32));

        break;
    }
    case 8:
    {
        memcpy(
            ps_cu_analyse[0].i4_act_factor,
            ps_cur_ipe_ctb->i4_16x16_act_factor[(i4_32x32_id << 2) + i4_16x16_id],
            2 * 2 * sizeof(WORD32));

        break;
    }
    }

    /* Populate the me data in cu_analyse struct */
    /* For CU size 32 and 64, add me data to array of cu analyse struct */
    if(ISLICE != ps_ctxt->i1_slice_type)
    {
        if((ps_cu_tree_analyse->u1_cu_size >= 32) && (ps_cu_tree_analyse->u1_inter_eval_enable))
        {
            if(32 == ps_cu_tree_analyse->u1_cu_size)
            {
                ihevce_populate_cu_struct(
                    ps_ctxt,
                    ps_cur_ipe_ctb,
                    ps_cu_tree_analyse,
                    ps_cu_me_data->as_32x32_block_data[i4_32x32_id].as_best_results,
                    ps_cu_analyse,
                    i4_32x32_id,
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
                    ps_cu_prms->u1_is_cu_noisy,
#endif
                    ps_cu_me_data->as_32x32_block_data[i4_32x32_id].num_best_results);
            }
            else
            {
                ihevce_populate_cu_struct(
                    ps_ctxt,
                    ps_cur_ipe_ctb,
                    ps_cu_tree_analyse,
                    ps_cu_me_data->s_64x64_block_data.as_best_results,
                    ps_cu_analyse,
                    i4_32x32_id,
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
                    ps_cu_prms->u1_is_cu_noisy,
#endif
                    ps_cu_me_data->s_64x64_block_data.num_best_results);
            }
        }
        else if(ps_cu_tree_analyse->u1_cu_size < 32)
        {
            i4_8x8_id += (i4_32x32_id << 4) + (i4_16x16_id << 2);
            i4_16x16_id += (i4_32x32_id << 2);

            if(16 == ps_cu_tree_analyse->u1_cu_size)
            {
                block_data_16x16_t *ps_data = &ps_cu_me_data->as_block_data[i4_16x16_id];

                if(ps_cu_tree_analyse->u1_inter_eval_enable)
                {
                    ihevce_populate_cu_struct(
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        ps_cu_tree_analyse,
                        ps_data->as_best_results,
                        ps_cu_analyse,
                        i4_32x32_id,
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
                        ps_cu_prms->u1_is_cu_noisy,
#endif
                        ps_data->num_best_results);
                }
                else
                {
                    ps_cu_analyse->u1_num_inter_cands = 0;
                    ps_cu_analyse->u1_best_is_intra = 1;
                }
            }
            else /* If CU size is 8 */
            {
                block_data_8x8_t *ps_data = &ps_cu_me_data->as_8x8_block_data[i4_8x8_id];

                if(ps_cu_tree_analyse->u1_inter_eval_enable)
                {
                    ihevce_populate_cu_struct(
                        ps_ctxt,
                        ps_cur_ipe_ctb,
                        ps_cu_tree_analyse,
                        ps_data->as_best_results,
                        ps_cu_analyse,
                        i4_32x32_id,
#if DISABLE_INTRA_WHEN_NOISY && USE_NOISE_TERM_IN_ENC_LOOP
                        ps_cu_prms->u1_is_cu_noisy,
#endif
                        ps_data->num_best_results);
                }
                else
                {
                    ps_cu_analyse->u1_num_inter_cands = 0;
                    ps_cu_analyse->u1_best_is_intra = 1;
                }
            }
        }
        else
        {
            ps_cu_analyse->u1_num_inter_cands = 0;
            ps_cu_analyse->u1_best_is_intra = 1;
        }
    }
    else
    {
        ps_cu_analyse->u1_num_inter_cands = 0;
        ps_cu_analyse->u1_best_is_intra = 1;
    }

    if(!ps_ctxt->i1_cu_qp_delta_enable)
    {
        ps_cu_analyse->i1_cu_qp = ps_ctxt->i4_frame_qp;

        /*cu qp must be populated in cu_analyse_t struct*/
        ps_ctxt->i4_cu_qp = ps_cu_analyse->i1_cu_qp;
    }
    else
    {
        ASSERT(ps_cu_analyse->i4_act_factor[0] > 0);
        ASSERT(
            ((ps_cu_analyse->i4_act_factor[1] > 0) && (ps_cu_analyse->u1_cu_size != 8)) ||
            ((ps_cu_analyse->u1_cu_size == 8)));
        ASSERT(
            ((ps_cu_analyse->i4_act_factor[2] > 0) && (ps_cu_analyse->u1_cu_size == 32)) ||
            ((ps_cu_analyse->u1_cu_size != 32)));
    }

    if(ps_ctxt->u1_disable_intra_eval)
    {
        /* rdopt evaluation of intra disabled as inter is clear winner */
        ps_cu_analyse->u1_num_intra_rdopt_cands = 0;

        /* all the modes invalidated */
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_nxn[0][0] = 255;
        ps_cu_analyse->u1_chroma_intra_pred_mode = 255;

        /* no intra candt to verify */
        ps_cu_analyse->s_cu_intra_cand.b6_num_intra_cands = 0;
    }

#if DISABLE_L2_IPE_IN_PB_L1_IN_B
    if((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6) && (ps_cu_analyse->u1_cu_size == 32) &&
       (ps_ctxt->i1_slice_type != ISLICE))
    {
        /* rdopt evaluation of intra disabled as inter is clear winner */
        ps_cu_analyse->u1_num_intra_rdopt_cands = 0;

        /* all the modes invalidated */
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] = 255;
        ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_nxn[0][0] = 255;
        ps_cu_analyse->u1_chroma_intra_pred_mode = 255;

        /* no intra candt to verify */
        ps_cu_analyse->s_cu_intra_cand.b6_num_intra_cands = 0;
    }
#endif

    if(DISABLE_INTRA_WHEN_NOISY && ps_cu_prms->u1_is_cu_noisy)
    {
        ps_cu_analyse->u1_num_intra_rdopt_cands = 0;
    }

    if(ps_cu_analyse->u1_num_intra_rdopt_cands || ps_cu_tree_analyse->u1_intra_eval_enable)
    {
        ihevce_intra_mode_populator(
            &ps_cu_analyse->s_cu_intra_cand,
            ps_cur_ipe_ctb,
            ps_cu_tree_analyse,
            ps_ctxt->i1_slice_type,
            ps_ctxt->i4_quality_preset);

        ps_cu_analyse->u1_num_intra_rdopt_cands = 1;
    }

    ASSERT(!!ps_cu_analyse->u1_num_intra_rdopt_cands || ps_cu_analyse->u1_num_inter_cands);

    if(ps_ctxt->u1_use_top_at_ctb_boundary)
    {
        /* Wait till top data is ready          */
        /* Currently checking till top right CU */
        curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

        if(0 == ps_cu_analyse->b3_cu_pos_y)
        {
            ihevce_dmgr_chk_row_row_sync(
                ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
                curr_cu_pos_in_row,
                cu_top_right_offset,
                cu_top_right_dep_pos,
                ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                ps_ctxt->thrd_id);
        }
    }

#if !DISABLE_TOP_SYNC
    {
        if(0 == ps_cu_analyse->b3_cu_pos_y)
        {
            if((0 == i4_ctb_x_off) && (i4_ctb_y_off != 0))
            {
                if(ps_cu_analyse->b3_cu_pos_x == 0)
                {
                    if(!ps_ctxt->u1_use_top_at_ctb_boundary)
                    {
                        /* Wait till top data is ready          */
                        /* Currently checking till top right CU */
                        curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

                        if(0 == ps_cu_analyse->b3_cu_pos_y)
                        {
                            ihevce_dmgr_chk_row_row_sync(
                                ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
                                curr_cu_pos_in_row,
                                cu_top_right_offset,
                                cu_top_right_dep_pos,
                                ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                                ps_ctxt->thrd_id);
                        }
                    }

                    ihevce_entropy_rdo_copy_states(
                        &ps_ctxt->s_rdopt_entropy_ctxt,
                        ps_ctxt->pu1_top_rt_cabac_state,
                        UPDATE_ENT_SYNC_RDO_STATE);
                }
            }
        }
    }
#else
    {
        if((0 == ps_cu_analyse->b3_cu_pos_y) && (IHEVCE_QUALITY_P6 != ps_ctxt->i4_quality_preset))
        {
            if((0 == i4_ctb_x_off) && (i4_ctb_y_off != 0))
            {
                if(ps_cu_analyse->b3_cu_pos_x == 0)
                {
                    if(!ps_ctxt->u1_use_top_at_ctb_boundary)
                    {
                        /* Wait till top data is ready          */
                        /* Currently checking till top right CU */
                        curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

                        if(0 == ps_cu_analyse->b3_cu_pos_y)
                        {
                            ihevce_dmgr_chk_row_row_sync(
                                ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
                                curr_cu_pos_in_row,
                                cu_top_right_offset,
                                cu_top_right_dep_pos,
                                ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                                ps_ctxt->thrd_id);
                        }
                    }

                    ihevce_entropy_rdo_copy_states(
                        &ps_ctxt->s_rdopt_entropy_ctxt,
                        ps_ctxt->pu1_top_rt_cabac_state,
                        UPDATE_ENT_SYNC_RDO_STATE);
                }
            }
        }
        else if((0 == ps_cu_analyse->b3_cu_pos_y) && (IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset))
        {
            UWORD8 u1_cabac_init_idc;
            WORD8 i1_cabac_init_flag =
                ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt->ps_slice_hdr->i1_cabac_init_flag;

            if(ps_ctxt->i1_slice_type == ISLICE)
            {
                u1_cabac_init_idc = 0;
            }
            else if(ps_ctxt->i1_slice_type == PSLICE)
            {
                u1_cabac_init_idc = i1_cabac_init_flag ? 2 : 1;
            }
            else
            {
                u1_cabac_init_idc = i1_cabac_init_flag ? 1 : 2;
            }

            ihevce_entropy_rdo_copy_states(
                &ps_ctxt->s_rdopt_entropy_ctxt,
                (UWORD8 *)gau1_ihevc_cab_ctxts[u1_cabac_init_idc][ps_ctxt->i4_frame_qp],
                UPDATE_ENT_SYNC_RDO_STATE);
        }
    }
#endif

    /*2 Multi- dimensinal array based on trans size  of rounding factor to be added here */
    /* arrays are for rounding factor corr. to 0-1 decision and 1-2 decision */
    /* Currently the complete array will contain only single value*/
    /*The rounding factor is calculated with the formula
    Deadzone val = (((R1 - R0) * (2^(-8/3)) * lamMod) + 1)/2
    rounding factor = (1 - DeadZone Val)

    Assumption: Cabac states of All the sub-blocks in the TU are considered independent
    */

    /*As long as coef level rdoq is enabled perform this operation */
    is_first_cu_in_ctb = ((0 == ps_cu_analyse->b3_cu_pos_x) && (0 == ps_cu_analyse->b3_cu_pos_y));
    is_ctb_level_quant_rounding =
        ((ps_ctxt->i4_quant_rounding_level == CTB_LEVEL_QUANT_ROUNDING) &&
         (1 == is_first_cu_in_ctb));
    is_nctb_level_quant_rounding =
        ((ps_ctxt->i4_quant_rounding_level == NCTB_LEVEL_QUANT_ROUNDING) &&
         (1 == is_first_cu_in_ctb) && (((i4_ctb_x_off >> 6) % NUM_CTB_QUANT_ROUNDING) == 0));

    if((ps_ctxt->i4_quant_rounding_level == CU_LEVEL_QUANT_ROUNDING) ||
       (ps_ctxt->i4_quant_rounding_level == TU_LEVEL_QUANT_ROUNDING) ||
       (1 == is_ctb_level_quant_rounding) || (1 == is_nctb_level_quant_rounding))
    {
        double i4_lamda_modifier, i4_lamda_modifier_uv;
        WORD32 trans_size, trans_size_cr;
        trans_size = ps_cu_analyse->u1_cu_size;

        if((1 == is_ctb_level_quant_rounding) || (1 == is_nctb_level_quant_rounding))
        {
            trans_size = MAX_TU_SIZE;
        }
        else
        {
            if(ps_cu_analyse->u1_cu_size == 64)
            {
                trans_size >>= 1;
            }
        }

        /*Chroma trans size = half of luma trans size */
        trans_size_cr = trans_size >> 1;

        if((BSLICE == ps_ctxt->i1_slice_type) && (ps_ctxt->i4_temporal_layer_id))
        {
            i4_lamda_modifier = ps_ctxt->i4_lamda_modifier *
                                CLIP3((((double)(ps_ctxt->i4_cu_qp - 12)) / 6.0), 2.00, 4.00);
            i4_lamda_modifier_uv =
                ps_ctxt->i4_uv_lamda_modifier *
                CLIP3((((double)(ps_ctxt->i4_chrm_cu_qp - 12)) / 6.0), 2.00, 4.00);
        }
        else
        {
            i4_lamda_modifier = ps_ctxt->i4_lamda_modifier;
            i4_lamda_modifier_uv = ps_ctxt->i4_uv_lamda_modifier;
        }
        if(ps_ctxt->i4_use_const_lamda_modifier)
        {
            if(ISLICE == ps_ctxt->i1_slice_type)
            {
                i4_lamda_modifier = ps_ctxt->f_i_pic_lamda_modifier;
                i4_lamda_modifier_uv = ps_ctxt->f_i_pic_lamda_modifier;
            }
            else
            {
                i4_lamda_modifier = CONST_LAMDA_MOD_VAL;
                i4_lamda_modifier_uv = CONST_LAMDA_MOD_VAL;
            }
        }

        do
        {
            memset(
                ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[trans_size >> 3],
                0,
                trans_size * trans_size * sizeof(WORD32));
            memset(
                ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[trans_size >> 3],
                0,
                trans_size * trans_size * sizeof(WORD32));

            /*ps_ctxt->i4_quant_rnd_factor[intra_flag], is currently not used */
            ihevce_quant_rounding_factor_gen(
                trans_size,
                1,  //is_luma = 1
                &ps_ctxt->s_rdopt_entropy_ctxt,
                ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[trans_size >> 3],
                ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[trans_size >> 3],
                i4_lamda_modifier,
                0);  //is_tu_level_quant rounding = 0

            trans_size = trans_size >> 1;

        } while(trans_size >= 4);

        /*CHROMA Quant Rounding is to be enabled with CU/TU/CTB/NCTB Luma rounding */
        /*Please note chroma is calcualted only for 1st TU at TU level Rounding */
        if(ps_ctxt->i4_chroma_quant_rounding_level == CHROMA_QUANT_ROUNDING)
        {
            do
            {
                memset(
                    ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[trans_size_cr >> 3],
                    0,
                    trans_size_cr * trans_size_cr * sizeof(WORD32));
                memset(
                    ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[trans_size_cr >> 3],
                    0,
                    trans_size_cr * trans_size_cr * sizeof(WORD32));

                ihevce_quant_rounding_factor_gen(
                    trans_size_cr,
                    0,  //is_luma = 0
                    &ps_ctxt->s_rdopt_entropy_ctxt,
                    ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[trans_size_cr >> 3],
                    ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[trans_size_cr >> 3],
                    i4_lamda_modifier_uv,
                    0);  //is_tu_level_quant rounding = 0

                trans_size_cr = trans_size_cr >> 1;

            } while(trans_size_cr >= 4);
        }
    }

#if DISABLE_INTRAS_IN_BPIC
    if((ps_ctxt->i1_slice_type == BSLICE) && (ps_cu_analyse->u1_num_inter_cands))
    {
        ps_cu_analyse->u1_num_intra_rdopt_cands = 0;
    }
#endif

    rdopt_best_cost = ihevce_cu_mode_decide(
        ps_ctxt,
        ps_cu_prms,
        ps_cu_analyse,
        ps_final_mode_state,
        pu1_ecd_data,
        ps_col_pu,
        pu1_col_pu_map,
        col_start_pu_idx);

    return rdopt_best_cost;
}

/**
******************************************************************************
* \if Function name : ihevce_enc_loop_cu_bot_copy \endif
*
* \brief
*    This function copy the bottom data at CU level to row buffers
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
*
******************************************************************************
*/
void ihevce_enc_loop_cu_bot_copy(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    WORD32 curr_cu_pos_in_row,
    WORD32 curr_cu_pos_in_ctb)
{
    /* ---------------------------------------------- */
    /* copy the bottom row  data to the row buffers   */
    /* ---------------------------------------------- */
    nbr_4x4_t *ps_top_nbr;
    UWORD8 *pu1_buff;
    UWORD8 *pu1_luma_top, *pu1_chrm_top;
    WORD32 nbr_strd;

    WORD32 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);

    /* derive the appropraite pointers */
    pu1_luma_top = (UWORD8 *)ps_ctxt->pv_bot_row_luma + curr_cu_pos_in_row;
    pu1_chrm_top = (UWORD8 *)ps_ctxt->pv_bot_row_chroma + curr_cu_pos_in_row;
    ps_top_nbr = ps_ctxt->ps_bot_row_nbr + (curr_cu_pos_in_row >> 2);
    nbr_strd = ps_cu_prms->i4_ctb_size >> 2;

    /* copy bottom luma data */
    pu1_buff = ps_cu_prms->pu1_luma_recon +
               (ps_cu_prms->i4_luma_recon_stride * (ps_cu_prms->i4_ctb_size - 1));

    pu1_buff += curr_cu_pos_in_ctb;

    memcpy(pu1_luma_top, pu1_buff, ps_enc_out_ctxt->u1_cu_size);

    /* copy bottom chroma data cb and cr pixel interleaved */
    pu1_buff = ps_cu_prms->pu1_chrm_recon + (ps_cu_prms->i4_chrm_recon_stride *
                                             ((ps_cu_prms->i4_ctb_size >> (0 == u1_is_422)) - 1));

    pu1_buff += curr_cu_pos_in_ctb;

    memcpy(pu1_chrm_top, pu1_buff, ps_enc_out_ctxt->u1_cu_size);

    /* store the nbr 4x4 data at cu level */
    {
        nbr_4x4_t *ps_nbr;

        /* copy bottom nbr data */
        ps_nbr = &ps_ctxt->as_ctb_nbr_arr[0];
        ps_nbr += ((ps_cu_prms->i4_ctb_size >> 2) - 1) * nbr_strd;

        ps_nbr += (curr_cu_pos_in_ctb >> 2);

        memcpy(ps_top_nbr, ps_nbr, (ps_enc_out_ctxt->u1_cu_size >> 2) * sizeof(nbr_4x4_t));
    }
    return;
}

/**
******************************************************************************
* \if Function name : ihevce_update_final_cu_results \endif
*
* \brief
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_update_final_cu_results(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    pu_col_mv_t **pps_row_col_pu,
    WORD32 *pi4_col_pu_map_idx,
    cu_final_update_prms *ps_cu_update_prms,
    WORD32 ctb_ctr,
    WORD32 vert_ctb_ctr)
{
    WORD32 curr_cu_pos_in_row;

    cu_enc_loop_out_t *ps_cu_final = *ps_cu_update_prms->pps_cu_final;
    pu_t **pps_row_pu = ps_cu_update_prms->pps_row_pu;
    tu_enc_loop_out_t **pps_row_tu = ps_cu_update_prms->pps_row_tu;
    UWORD8 **ppu1_row_ecd_data = ps_cu_update_prms->ppu1_row_ecd_data;
    WORD32 *pi4_num_pus_in_ctb = ps_cu_update_prms->pi4_num_pus_in_ctb;
    UWORD32 u4_cu_size = ps_enc_out_ctxt->u1_cu_size;
    ps_cu_final->b3_cu_pos_x = ps_enc_out_ctxt->b3_cu_pos_x;
    ps_cu_final->b3_cu_pos_y = ps_enc_out_ctxt->b3_cu_pos_y;

    ps_cu_final->b4_cu_size = ps_enc_out_ctxt->u1_cu_size >> 3;

    /* store the current pu and tu pointes */
    ps_cu_final->ps_pu = *pps_row_pu;
    ps_cu_final->ps_enc_tu = *pps_row_tu;
    curr_cu_pos_in_row = ctb_ctr * ps_cu_prms->i4_ctb_size + (ps_cu_final->b3_cu_pos_x << 3);

    ihevce_store_cu_final(ps_ctxt, ps_cu_final, *ppu1_row_ecd_data, ps_enc_out_ctxt, ps_cu_prms);

    if(NULL != pps_row_col_pu)
    {
        (*pps_row_col_pu) += ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
    }
    if(NULL != pi4_col_pu_map_idx)
    {
        (*pi4_col_pu_map_idx) += ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
    }
    (*pi4_num_pus_in_ctb) += ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
    (*pps_row_tu) += ps_cu_final->u2_num_tus_in_cu;
    (*pps_row_pu) += ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
    (*ppu1_row_ecd_data) += ps_enc_out_ctxt->ps_cu_prms->i4_num_bytes_ecd_data;

    (*ps_cu_update_prms->pps_cu_final)++;
    (*ps_cu_update_prms->pu1_num_cus_in_ctb_out)++;

    /* Updated for each CU in bottom row  of CTB */
    if(((ps_cu_final->b3_cu_pos_y << 3) + u4_cu_size) == ps_ctxt->u4_cur_ctb_ht)
    {
        /* copy the bottom data to row buffers */
        ((pf_enc_loop_cu_bot_copy)ps_ctxt->pv_enc_loop_cu_bot_copy)(
            ps_ctxt,
            ps_cu_prms,
            ps_enc_out_ctxt,
            curr_cu_pos_in_row,
            (ps_enc_out_ctxt->b3_cu_pos_x << 3));

        /* Setting Dependency for CU TopRight */
        ihevce_dmgr_set_row_row_sync(
            ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
            (curr_cu_pos_in_row + ps_enc_out_ctxt->u1_cu_size),
            vert_ctb_ctr,
            ps_ctxt->i4_tile_col_idx /* Col Tile No. */);

        /* Setting Dependency for Entropy to consume is made at CTB level */
    }
}

/**
******************************************************************************
* \if Function name : ihevce_cu_recurse_decide \endif
*
* \brief
*    Coding Unit mode decide function. Performs RD opt and decides the best mode
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_cu_prms  : pointer to coding unit params (position, buffer pointers)
* \param[in] ps_cu_analyse : pointer to cu analyse
* \param[out] ps_cu_final : pointer to cu final
* \param[out] pu1_ecd_data :pointer to store coeff data for ECD
* \param[out]ps_row_col_pu; colocated pu buffer pointer
* \param[out]pu1_row_pu_map; colocated pu map buffer pointer
* \param[in]col_start_pu_idx : pu index start value
*
* \return
*    None
*
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_cu_recurse_decide(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse,
    cur_ctb_cu_tree_t *ps_cu_tree_analyse_parent,
    ipe_l0_ctb_analyse_for_me_t *ps_cur_ipe_ctb,
    me_ctb_data_t *ps_cu_me_data,
    pu_col_mv_t **pps_col_pu,
    cu_final_update_prms *ps_cu_update_prms,
    UWORD8 *pu1_col_pu_map,
    WORD32 *pi4_col_start_pu_idx,
    WORD32 i4_tree_depth,
    WORD32 i4_ctb_x_off,
    WORD32 i4_ctb_y_off,
    WORD32 cur_ctb_ht)
{
    cur_ctb_cu_tree_t *ps_cu_tree_analyse_child[4];
    final_mode_state_t s_final_mode_state;

    WORD32 i;
    WORD32 child_nodes_null;
    LWORD64 i8_least_child_cost;

    WORD32 num_children_encoded = 0;

    /* Take backup of collocated start PU index for parent node rdo for PQ */
    WORD32 i4_col_pu_idx_bkup = *pi4_col_start_pu_idx;
    pu_col_mv_t *ps_col_mv_bkup = *pps_col_pu;

#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
    WORD32 x0_frm = i4_ctb_x_off + (ps_cu_tree_analyse->b3_cu_pos_x << 3);
    WORD32 y0_frm = i4_ctb_y_off + (ps_cu_tree_analyse->b3_cu_pos_y << 3);
    WORD32 pic_wd = ps_ctxt->s_sao_ctxt_t.ps_sps->i2_pic_width_in_luma_samples;
    WORD32 pic_ht = ps_ctxt->s_sao_ctxt_t.ps_sps->i2_pic_height_in_luma_samples;
    WORD32 log2_min_cb_size = ps_ctxt->s_sao_ctxt_t.ps_sps->i1_log2_min_coding_block_size;
    WORD32 cu_size = ps_cu_tree_analyse->u1_cu_size;

    /* bits for coding split_cu_flag = 1 */
    WORD32 split_cu1_bits_q12 = 0;

    /* bits for coding split_cu_flag = 0 */
    WORD32 split_cu0_bits_q12 = 0;
#endif

    UWORD8 u1_is_cu_noisy = ps_ctxt->u1_is_stasino_enabled
                                ? ihevce_determine_cu_noise_based_on_8x8Blk_data(
                                      ps_cu_prms->pu1_is_8x8Blk_noisy,
                                      ((ps_cu_tree_analyse->b3_cu_pos_x << 3) >> 4) << 4,
                                      ((ps_cu_tree_analyse->b3_cu_pos_y << 3) >> 4) << 4,
                                      MAX(16, ps_cu_tree_analyse->u1_cu_size))
                                : 0;

#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
    LWORD64 i8_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
#endif

    (void)ps_cu_tree_analyse_parent;

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
    if(!ps_ctxt->u1_enable_psyRDOPT && u1_is_cu_noisy)
    {
        ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
        ps_ctxt->i8_cl_ssd_lambda_chroma_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
    }
#endif

    if(u1_is_cu_noisy && !ps_ctxt->u1_enable_psyRDOPT)
    {
        i8_lambda_qf = ((float)i8_lambda_qf * (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);
    }

    ps_cu_tree_analyse_child[0] = ps_cu_tree_analyse->ps_child_node_tl;
    ps_cu_tree_analyse_child[1] = ps_cu_tree_analyse->ps_child_node_tr;
    ps_cu_tree_analyse_child[2] = ps_cu_tree_analyse->ps_child_node_bl;
    ps_cu_tree_analyse_child[3] = ps_cu_tree_analyse->ps_child_node_br;

    child_nodes_null =
        ((ps_cu_tree_analyse_child[0] == NULL) + (ps_cu_tree_analyse_child[1] == NULL) +
         (ps_cu_tree_analyse_child[2] == NULL) + (ps_cu_tree_analyse_child[3] == NULL));

#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
#endif
    {
        /*----------------------------------------------*/
        /* ---------- CU Depth Bit Estimation --------- */
        /*----------------------------------------------*/

        /* Encode cu split flags based on following conditions; See section 7.3.8*/
        if(((x0_frm + cu_size) <= pic_wd) && ((y0_frm + cu_size) <= pic_ht) &&
           (cu_size > (1 << log2_min_cb_size))) /* &&(ps_entropy_ctxt->i1_ctb_num_pcm_blks == 0)) */
        {
            WORD32 left_cu_depth = 0;
            WORD32 top_cu_depth = 0;
            WORD32 pos_x_4x4 = ps_cu_tree_analyse->b3_cu_pos_x << 1;
            WORD32 pos_y_4x4 = ps_cu_tree_analyse->b3_cu_pos_y << 1;
            WORD32 num_4x4_in_ctb = (ps_cu_prms->i4_ctb_size >> 2);
            WORD32 cur_4x4_in_ctb = pos_x_4x4 + (pos_y_4x4 * num_4x4_in_ctb);
            UWORD8 u1_split_cu_flag_cab_model;
            WORD32 split_cu_ctxt_inc;

            /* Left and Top CU depth is required for cabac context */

            /* CU left */
            if(0 == pos_x_4x4)
            {
                /* CTB boundary */
                if(i4_ctb_x_off)
                {
                    left_cu_depth = ps_ctxt->as_left_col_nbr[pos_y_4x4].b2_cu_depth;
                }
            }
            else
            {
                /* inside CTB */
                left_cu_depth = ps_ctxt->as_ctb_nbr_arr[cur_4x4_in_ctb - 1].b2_cu_depth;
            }

            /* CU top */
            if(0 == pos_y_4x4)
            {
                /* CTB boundary */
                if(i4_ctb_y_off)
                {
                    /* Wait till top cu depth is available */
                    ihevce_dmgr_chk_row_row_sync(
                        ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
                        (i4_ctb_x_off) + (pos_x_4x4 << 2),
                        4,
                        ((i4_ctb_y_off >> 6) - 1),
                        ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                        ps_ctxt->thrd_id);

                    top_cu_depth =
                        ps_ctxt->ps_top_row_nbr[(i4_ctb_x_off >> 2) + pos_x_4x4].b2_cu_depth;
                }
            }
            else
            {
                /* inside CTB */
                top_cu_depth = ps_ctxt->as_ctb_nbr_arr[cur_4x4_in_ctb - num_4x4_in_ctb].b2_cu_depth;
            }

            split_cu_ctxt_inc = IHEVC_CAB_SPLIT_CU_FLAG + (left_cu_depth > i4_tree_depth) +
                                (top_cu_depth > i4_tree_depth);

            u1_split_cu_flag_cab_model =
                ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][split_cu_ctxt_inc];

            /* bits for coding split_cu_flag = 1 */
            split_cu1_bits_q12 = gau2_ihevce_cabac_bin_to_bits[u1_split_cu_flag_cab_model ^ 1];

            /* bits for coding split_cu_flag = 0 */
            split_cu0_bits_q12 = gau2_ihevce_cabac_bin_to_bits[u1_split_cu_flag_cab_model ^ 0];

            /* update the cu split cabac context of all child nodes before evaluating child */
            for(i = (i4_tree_depth + 1); i < 4; i++)
            {
                ps_ctxt->au1_rdopt_recur_ctxt_models[i][split_cu_ctxt_inc] =
                    gau1_ihevc_next_state[(u1_split_cu_flag_cab_model << 1) | 1];
            }

            /* update the cu split cabac context of the parent node with split flag = 0 */
            ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][split_cu_ctxt_inc] =
                gau1_ihevc_next_state[(u1_split_cu_flag_cab_model << 1) | 0];
        }
    }
#endif

    /* If all the child nodes are null, then do rdo for this node and return the cost */
    if((1 == ps_cu_tree_analyse->is_node_valid) && (4 == child_nodes_null))
    {
        WORD32 i4_num_bytes_ecd_data;

#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
        COPY_CABAC_STATES(
            &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
            &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
            IHEVC_CAB_CTXT_END * sizeof(UWORD8));
#else
        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
            COPY_CABAC_STATES(
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                IHEVC_CAB_CTXT_END * sizeof(UWORD8));
        }
#endif

        ps_cu_prms->u1_is_cu_noisy = u1_is_cu_noisy;
        ihevce_update_pred_qp(
            ps_ctxt, ps_cu_tree_analyse->b3_cu_pos_x, ps_cu_tree_analyse->b3_cu_pos_y);
        /* DO rdo for current node here */
        /* return rdo cost for current node*/
        ps_cu_tree_analyse->i8_best_rdopt_cost = ihevce_compute_rdo(
            ps_ctxt,
            ps_cu_prms,
            ps_cu_tree_analyse,
            ps_cur_ipe_ctb,
            ps_cu_me_data,
            *pps_col_pu,
            &s_final_mode_state,
            pu1_col_pu_map,
            *ps_cu_update_prms->ppu1_row_ecd_data,
            *pi4_col_start_pu_idx,
            i4_ctb_x_off,
            i4_ctb_y_off);

        if((((ps_cu_tree_analyse->b3_cu_pos_y << 3) + ps_cu_tree_analyse->u1_cu_size) ==
            cur_ctb_ht) &&
           (ps_cu_tree_analyse->b3_cu_pos_x == 0) && (i4_ctb_x_off == 0))
        {
            /* copy the state to row level context after 1st Cu, in the Last CU row of CTB */
            /* copy current ctb CU states into a entropy sync state */
            /* to be used for next row                              */
            COPY_CABAC_STATES(
                ps_ctxt->pu1_curr_row_cabac_state,
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                IHEVC_CAB_CTXT_END * sizeof(UWORD8));
        }

#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
        {
#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
            /* Add parent split cu = 0 cost signalling */
            ps_cu_tree_analyse->i8_best_rdopt_cost += COMPUTE_RATE_COST_CLIP30(
                split_cu0_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
#endif
            for(i = (i4_tree_depth); i < 4; i++)
            {
                COPY_CABAC_STATES(
                    &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    IHEVC_CAB_CTXT_END * sizeof(UWORD8));
            }
        }
#else
        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
            /* Add parent split cu = 0 cost signalling */
            ps_cu_tree_analyse->i8_best_rdopt_cost += COMPUTE_RATE_COST_CLIP30(
                split_cu0_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
#endif

            for(i = (i4_tree_depth); i < 4; i++)
            {
                COPY_CABAC_STATES(
                    &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    IHEVC_CAB_CTXT_END * sizeof(UWORD8));
            }
        }
#endif

        ((pf_store_cu_results)ps_ctxt->pv_store_cu_results)(
            ps_ctxt, ps_cu_prms, &s_final_mode_state);

#if(!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
        if(ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P2)
        {
            ihevce_update_final_cu_results(
                ps_ctxt,
                ps_ctxt->ps_enc_out_ctxt,
                ps_cu_prms,
                pps_col_pu,
                pi4_col_start_pu_idx,
                ps_cu_update_prms,
                i4_ctb_x_off >> 6,
                i4_ctb_y_off >> 6);
        }
        else
        {
            /* ---- copy the luma & chroma coeffs to final output -------- */
            i4_num_bytes_ecd_data = ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->i4_num_bytes_ecd_data;

            if(0 != i4_num_bytes_ecd_data)
            {
                memcpy(
                    ps_ctxt->pu1_ecd_data,
                    &ps_ctxt->pu1_cu_recur_coeffs[0],
                    i4_num_bytes_ecd_data * sizeof(UWORD8));

                ps_ctxt->pu1_ecd_data += i4_num_bytes_ecd_data;
            }

            /* Collocated PU updates */
            *pps_col_pu += ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
            *pi4_col_start_pu_idx += ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
        }
#else
        /* ---- copy the luma & chroma coeffs to final output -------- */
        i4_num_bytes_ecd_data = ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->i4_num_bytes_ecd_data;
        if(0 != i4_num_bytes_ecd_data)
        {
            memcpy(
                ps_ctxt->pu1_ecd_data,
                &ps_ctxt->pu1_cu_recur_coeffs[0],
                i4_num_bytes_ecd_data * sizeof(UWORD8));

            ps_ctxt->pu1_ecd_data += i4_num_bytes_ecd_data;
        }

        /* Collocated PU updates */
        *pps_col_pu += ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
        *pi4_col_start_pu_idx += ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
#endif

        ps_ctxt->ps_enc_out_ctxt++;
        num_children_encoded++;
    }
    else
    {
        i8_least_child_cost = 0;

        for(i = 0; i < 4; i++)
        {
            if(ps_cu_tree_analyse_child[i] != NULL)
            {
                num_children_encoded += ihevce_cu_recurse_decide(
                    ps_ctxt,
                    ps_cu_prms,
                    ps_cu_tree_analyse_child[i],
                    ps_cu_tree_analyse,
                    ps_cur_ipe_ctb,
                    ps_cu_me_data,
                    pps_col_pu,
                    ps_cu_update_prms,
                    pu1_col_pu_map,
                    pi4_col_start_pu_idx,
                    i4_tree_depth + 1,
                    i4_ctb_x_off,
                    i4_ctb_y_off,
                    cur_ctb_ht);

                /* In case of incomplete ctb, */
                //if(MAX_COST != ps_cu_tree_analyse_child[i]->i4_best_rdopt_cost)
                if(((ULWORD64)(
                       i8_least_child_cost + ps_cu_tree_analyse_child[i]->i8_best_rdopt_cost)) >
                   MAX_COST_64)
                {
                    i8_least_child_cost = MAX_COST_64;
                }
                else
                {
                    i8_least_child_cost += ps_cu_tree_analyse_child[i]->i8_best_rdopt_cost;
                }
            }
            else
            {
                /* If the child node is NULL, return MAX_COST*/
                i8_least_child_cost = MAX_COST_64;
            }
        }

        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
#if !ENABLE_4CTB_EVALUATION
            if((ps_cu_tree_analyse->u1_cu_size == 64) && (num_children_encoded > 10) &&
               (ps_ctxt->i1_slice_type != ISLICE))
            {
                ps_cu_tree_analyse->is_node_valid = 0;
            }
#endif
        }

        /* If current CU node is valid, do rdo for the node and decide btwn child nodes and parent nodes  */
        if(ps_cu_tree_analyse->is_node_valid)
        {
            UWORD8 au1_cu_pu_map[(MAX_CTB_SIZE / MIN_PU_SIZE) * (MAX_CTB_SIZE / MIN_PU_SIZE)];
            pu_col_mv_t as_col_mv[2]; /* Max of 2 PUs only per CU */

            WORD32 i4_col_pu_idx_start = i4_col_pu_idx_bkup;

            /* Copy the collocated PU map to the local array */
            memcpy(
                au1_cu_pu_map,
                pu1_col_pu_map,
                (MAX_CTB_SIZE / MIN_PU_SIZE) * (MAX_CTB_SIZE / MIN_PU_SIZE));

#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
            COPY_CABAC_STATES(
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                IHEVC_CAB_CTXT_END * sizeof(UWORD8));

            /* Reset the nbr maps while computing Parent CU node ()*/
            /* set the neighbour map to 0 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ps_cu_tree_analyse->b3_cu_pos_x << 1),
                (ps_cu_tree_analyse->b3_cu_pos_y << 1),
                (ps_cu_tree_analyse->u1_cu_size >> 2),
                0);
#else
            if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
            {
                COPY_CABAC_STATES(
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                    IHEVC_CAB_CTXT_END * sizeof(UWORD8));

                /* Reset the nbr maps while computing Parent CU node ()*/
                /* set the neighbour map to 0 */
                ihevce_set_nbr_map(
                    ps_ctxt->pu1_ctb_nbr_map,
                    ps_ctxt->i4_nbr_map_strd,
                    (ps_cu_tree_analyse->b3_cu_pos_x << 1),
                    (ps_cu_tree_analyse->b3_cu_pos_y << 1),
                    (ps_cu_tree_analyse->u1_cu_size >> 2),
                    0);
            }
#endif

            /* Do rdo for the parent node */
            /* Compare parent node cost vs child node costs */
            ps_ctxt->is_parent_cu_rdopt = 1;

            ps_cu_prms->u1_is_cu_noisy = u1_is_cu_noisy;

            ihevce_update_pred_qp(
                ps_ctxt, ps_cu_tree_analyse->b3_cu_pos_x, ps_cu_tree_analyse->b3_cu_pos_y);

            ps_cu_tree_analyse->i8_best_rdopt_cost = ihevce_compute_rdo(
                ps_ctxt,
                ps_cu_prms,
                ps_cu_tree_analyse,
                ps_cur_ipe_ctb,
                ps_cu_me_data,
                as_col_mv,
                &s_final_mode_state,
                au1_cu_pu_map,
                *ps_cu_update_prms->ppu1_row_ecd_data,
                i4_col_pu_idx_start,
                i4_ctb_x_off,
                i4_ctb_y_off);

            ps_ctxt->is_parent_cu_rdopt = 0;

#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
            /* Add parent split cu cost signalling */
            ps_cu_tree_analyse->i8_best_rdopt_cost += COMPUTE_RATE_COST_CLIP30(
                split_cu0_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

            COPY_CABAC_STATES(
                &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                IHEVC_CAB_CTXT_END * sizeof(UWORD8));

            /* i8_least_child_cost += (num_children_encoded * ps_ctxt->i4_sad_lamda\
            + ((1 << (LAMBDA_Q_SHIFT)))) >> (LAMBDA_Q_SHIFT + 1) */
            ;
            /* bits for coding cu split flag as  1 */
            i8_least_child_cost += COMPUTE_RATE_COST_CLIP30(
                split_cu1_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
#else
#if ENABLE_CU_SPLIT_FLAG_RATE_ESTIMATION
            if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
            {
                /* Add parent split cu cost signalling */
                ps_cu_tree_analyse->i8_best_rdopt_cost += COMPUTE_RATE_COST_CLIP30(
                    split_cu0_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

                COPY_CABAC_STATES(
                    &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    IHEVC_CAB_CTXT_END * sizeof(UWORD8));

                /* i8_least_child_cost += (num_children_encoded * ps_ctxt->i4_sad_lamda\
                + ((1 << (LAMBDA_Q_SHIFT)))) >> (LAMBDA_Q_SHIFT + 1) */
                ;
                /* bits for coding cu split flag as  1 */
                i8_least_child_cost += COMPUTE_RATE_COST_CLIP30(
                    split_cu1_bits_q12, i8_lambda_qf, (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
            }
#else
            i8_least_child_cost +=
                (num_children_encoded * ps_ctxt->i4_sad_lamda + ((1 << (LAMBDA_Q_SHIFT)))) >>
                (LAMBDA_Q_SHIFT + 1);
#endif
#endif

            /* If child modes win over parent, discard parent enc ctxt */
            /* else discard child ctxt */
            if(ps_cu_tree_analyse->i8_best_rdopt_cost > i8_least_child_cost)
            {
#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
                /* Store child node Models for evalution of next CU */
                for(i = (i4_tree_depth); i < 4; i++)
                {
                    COPY_CABAC_STATES(
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }
                /* Reset cabac states if child has won */
                COPY_CABAC_STATES(
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                    IHEVC_CAB_CTXT_END * sizeof(UWORD8));
#else
                if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
                {
                    for(i = i4_tree_depth; i < 4; i++)
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                            IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                    }
                    /* Reset cabac states if child has won */
                    COPY_CABAC_STATES(
                        &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }
#endif
                ps_cu_tree_analyse->i8_best_rdopt_cost = i8_least_child_cost;
                ps_cu_tree_analyse->is_node_valid = 0;
            }
            else
            {
                /* Parent node wins over child node */
                ihevce_enc_cu_node_ctxt_t *ps_enc_tmp_out_ctxt;
                WORD32 i4_num_bytes_ecd_data;
                WORD32 num_child_nodes = 0;
                WORD32 i4_num_pus_in_cu;

                if((((ps_cu_tree_analyse->b3_cu_pos_y << 3) + ps_cu_tree_analyse->u1_cu_size) ==
                    cur_ctb_ht) &&
                   (ps_cu_tree_analyse->b3_cu_pos_x == 0) && (i4_ctb_x_off == 0))
                {
                    /* copy the state to row level context after 1st Cu, in the Last CU row of CTB */
                    /* copy current ctb CU states into a entropy sync state */
                    /* to be used for next row                              */
                    COPY_CABAC_STATES(
                        ps_ctxt->pu1_curr_row_cabac_state,
                        &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }

#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
                /* Store parent node Models for evalution of next CU */
                for(i = (i4_tree_depth + 1); i < 4; i++)
                {
                    COPY_CABAC_STATES(
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }
#else
                if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
                {
                    for(i = (i4_tree_depth + 1); i < 4; i++)
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth][0],
                            IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                    }
                }
#endif
                ((pf_store_cu_results)ps_ctxt->pv_store_cu_results)(
                    ps_ctxt, ps_cu_prms, &s_final_mode_state);

#if(!PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
                if(ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P2)
                {
                    ihevce_update_final_cu_results(
                        ps_ctxt,
                        ps_ctxt->ps_enc_out_ctxt,
                        ps_cu_prms,
                        pps_col_pu,
                        pi4_col_start_pu_idx,
                        ps_cu_update_prms,
                        i4_ctb_x_off >> 6,
                        i4_ctb_y_off >> 6);

                    ps_ctxt->ps_enc_out_ctxt++;
                }
                else
                {
                    ps_enc_tmp_out_ctxt = ps_ctxt->ps_enc_out_ctxt;

                    num_child_nodes = num_children_encoded;

                    /* ---- copy the luma & chroma coeffs to final output -------- */
                    for(i = 0; i < num_child_nodes; i++)
                    {
                        i4_num_bytes_ecd_data =
                            (ps_ctxt->ps_enc_out_ctxt - i - 1)->ps_cu_prms->i4_num_bytes_ecd_data;
                        ps_ctxt->pu1_ecd_data -= i4_num_bytes_ecd_data;
                    }

                    i4_num_bytes_ecd_data =
                        ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->i4_num_bytes_ecd_data;
                    if(0 != i4_num_bytes_ecd_data)
                    {
                        memcpy(
                            ps_ctxt->pu1_ecd_data,
                            &ps_ctxt->pu1_cu_recur_coeffs[0],
                            i4_num_bytes_ecd_data);

                        ps_ctxt->pu1_ecd_data += i4_num_bytes_ecd_data;
                    }

                    ps_enc_tmp_out_ctxt = ps_ctxt->ps_enc_out_ctxt - num_child_nodes;

                    memcpy(
                        ps_enc_tmp_out_ctxt,
                        ps_ctxt->ps_enc_out_ctxt,
                        sizeof(ihevce_enc_cu_node_ctxt_t));
                    ps_enc_tmp_out_ctxt->ps_cu_prms = &ps_enc_tmp_out_ctxt->s_cu_prms;

                    /* Collocated PU updates */
                    i4_num_pus_in_cu = ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
                    /* Copy the collocated MVs and the PU map to frame buffers */
                    memcpy(ps_col_mv_bkup, as_col_mv, sizeof(pu_col_mv_t) * i4_num_pus_in_cu);
                    memcpy(
                        pu1_col_pu_map,
                        au1_cu_pu_map,
                        (MAX_CTB_SIZE / MIN_PU_SIZE) * (MAX_CTB_SIZE / MIN_PU_SIZE));
                    /* Update the frame buffer pointer and the map index */
                    *pps_col_pu = ps_col_mv_bkup + i4_num_pus_in_cu;
                    *pi4_col_start_pu_idx = i4_col_pu_idx_bkup + i4_num_pus_in_cu;

                    ps_ctxt->ps_enc_out_ctxt = ps_enc_tmp_out_ctxt + 1;
                }
#else

                ps_enc_tmp_out_ctxt = ps_ctxt->ps_enc_out_ctxt;

                num_child_nodes = num_children_encoded;

                /* ---- copy the luma & chroma coeffs to final output -------- */
                for(i = 0; i < num_child_nodes; i++)
                {
                    i4_num_bytes_ecd_data =
                        (ps_ctxt->ps_enc_out_ctxt - i - 1)->ps_cu_prms->i4_num_bytes_ecd_data;
                    ps_ctxt->pu1_ecd_data -= i4_num_bytes_ecd_data;
                }

                i4_num_bytes_ecd_data = ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->i4_num_bytes_ecd_data;
                if(0 != i4_num_bytes_ecd_data)
                {
                    memcpy(
                        ps_ctxt->pu1_ecd_data,
                        &ps_ctxt->pu1_cu_recur_coeffs[0],
                        i4_num_bytes_ecd_data * sizeof(UWORD8));

                    ps_ctxt->pu1_ecd_data += i4_num_bytes_ecd_data;
                }

                ps_enc_tmp_out_ctxt = ps_ctxt->ps_enc_out_ctxt - num_child_nodes;

                memcpy(
                    ps_enc_tmp_out_ctxt,
                    ps_ctxt->ps_enc_out_ctxt,
                    sizeof(ihevce_enc_cu_node_ctxt_t));

                ps_enc_tmp_out_ctxt->ps_cu_prms = &ps_enc_tmp_out_ctxt->s_cu_prms;

                /* Collocated PU updates */
                i4_num_pus_in_cu = ps_ctxt->ps_enc_out_ctxt->ps_cu_prms->u2_num_pus_in_cu;
                /* Copy the collocated MVs and the PU map to frame buffers */
                memcpy(ps_col_mv_bkup, as_col_mv, sizeof(pu_col_mv_t) * i4_num_pus_in_cu);
                memcpy(
                    pu1_col_pu_map,
                    au1_cu_pu_map,
                    (MAX_CTB_SIZE / MIN_PU_SIZE) * (MAX_CTB_SIZE / MIN_PU_SIZE));
                /* Update the frame buffer pointer and the map index */
                *pps_col_pu = ps_col_mv_bkup + i4_num_pus_in_cu;
                *pi4_col_start_pu_idx = i4_col_pu_idx_bkup + i4_num_pus_in_cu;

                ps_ctxt->ps_enc_out_ctxt = ps_enc_tmp_out_ctxt + 1;
#endif

                num_children_encoded = 1;
                DISABLE_THE_CHILDREN_NODES(ps_cu_tree_analyse);
            }
        }
        else /* if(ps_cu_tree_analyse->is_node_valid) */
        {
            ps_cu_tree_analyse->i8_best_rdopt_cost = i8_least_child_cost;

            /* Tree depth of four will occur for Incomplete CTB */
            if((i8_least_child_cost > 0) && (i4_tree_depth != 3))
            {
#if(PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS)
                /* Store child node Models for evalution of next CU */
                for(i = i4_tree_depth; i < 4; i++)
                {
                    COPY_CABAC_STATES(
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }
#else
                if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
                {
                    for(i = (i4_tree_depth); i < 4; i++)
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i4_tree_depth + 1][0],
                            IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                    }
                }
#endif
            }
        }
    }

    return num_children_encoded;
}

static UWORD8 ihevce_intraData_availability_extractor(
    WORD8 *pi1_8x8CULevel_intraData_availability_indicator,
    UWORD8 u1_cu_size,
    UWORD8 u1_x_8x8CU_units,
    UWORD8 u1_y_8x8CU_units)
{
    if(8 == u1_cu_size)
    {
        return (!pi1_8x8CULevel_intraData_availability_indicator
                    [u1_x_8x8CU_units + MAX_CU_IN_CTB_ROW * u1_y_8x8CU_units]);
    }
    else
    {
        UWORD8 u1_data_availability = 0;
        UWORD8 u1_child_cu_size = u1_cu_size / 2;

        u1_data_availability |= ihevce_intraData_availability_extractor(
            pi1_8x8CULevel_intraData_availability_indicator,
            u1_child_cu_size,
            u1_x_8x8CU_units,
            u1_y_8x8CU_units);

        u1_data_availability |= ihevce_intraData_availability_extractor(
            pi1_8x8CULevel_intraData_availability_indicator,
            u1_child_cu_size,
            u1_x_8x8CU_units + u1_child_cu_size / 8,
            u1_y_8x8CU_units);

        u1_data_availability |= ihevce_intraData_availability_extractor(
            pi1_8x8CULevel_intraData_availability_indicator,
            u1_child_cu_size,
            u1_x_8x8CU_units,
            u1_y_8x8CU_units + u1_child_cu_size / 8);

        u1_data_availability |= ihevce_intraData_availability_extractor(
            pi1_8x8CULevel_intraData_availability_indicator,
            u1_child_cu_size,
            u1_x_8x8CU_units + u1_child_cu_size / 8,
            u1_y_8x8CU_units + u1_child_cu_size / 8);

        return u1_data_availability;
    }
}

void ihevce_intra_and_inter_cuTree_merger(
    cur_ctb_cu_tree_t *ps_merged_tree,
    cur_ctb_cu_tree_t *ps_intra_tree,
    cur_ctb_cu_tree_t *ps_inter_tree,
    WORD8 *pi1_8x8CULevel_intraData_availability_indicator)
{
    /* 0 => Intra and inter children valid */
    /* 1 => Only Intra valid */
    /* 2 => Only Inter valid */
    /* 3 => Neither */
    UWORD8 au1_children_recursive_call_type[4];

    if(NULL != ps_intra_tree)
    {
        ps_intra_tree->is_node_valid =
            ps_intra_tree->is_node_valid &
            ihevce_intraData_availability_extractor(
                pi1_8x8CULevel_intraData_availability_indicator,
                ps_intra_tree->u1_cu_size,
                ps_intra_tree->b3_cu_pos_x & ((8 == ps_intra_tree->u1_cu_size) ? 0xfe : 0xff),
                ps_intra_tree->b3_cu_pos_y & ((8 == ps_intra_tree->u1_cu_size) ? 0xfe : 0xff));
    }

    switch(((NULL == ps_intra_tree) << 1) | (NULL == ps_inter_tree))
    {
    case 0:
    {
        ps_merged_tree->is_node_valid = ps_intra_tree->is_node_valid ||
                                        ps_inter_tree->is_node_valid;
        ps_merged_tree->u1_inter_eval_enable = ps_inter_tree->is_node_valid;
        ps_merged_tree->u1_intra_eval_enable = ps_intra_tree->is_node_valid;

        au1_children_recursive_call_type[POS_TL] =
            ((NULL == ps_intra_tree->ps_child_node_tl) << 1) |
            (NULL == ps_inter_tree->ps_child_node_tl);
        au1_children_recursive_call_type[POS_TR] =
            ((NULL == ps_intra_tree->ps_child_node_tr) << 1) |
            (NULL == ps_inter_tree->ps_child_node_tr);
        au1_children_recursive_call_type[POS_BL] =
            ((NULL == ps_intra_tree->ps_child_node_bl) << 1) |
            (NULL == ps_inter_tree->ps_child_node_bl);
        au1_children_recursive_call_type[POS_BR] =
            ((NULL == ps_intra_tree->ps_child_node_br) << 1) |
            (NULL == ps_inter_tree->ps_child_node_br);

        break;
    }
    case 1:
    {
        ps_merged_tree->is_node_valid = ps_intra_tree->is_node_valid;
        ps_merged_tree->u1_inter_eval_enable = 0;
        ps_merged_tree->u1_intra_eval_enable = ps_intra_tree->is_node_valid;

        au1_children_recursive_call_type[POS_TL] =
            ((NULL == ps_intra_tree->ps_child_node_tl) << 1) + 1;
        au1_children_recursive_call_type[POS_TR] =
            ((NULL == ps_intra_tree->ps_child_node_tr) << 1) + 1;
        au1_children_recursive_call_type[POS_BL] =
            ((NULL == ps_intra_tree->ps_child_node_bl) << 1) + 1;
        au1_children_recursive_call_type[POS_BR] =
            ((NULL == ps_intra_tree->ps_child_node_br) << 1) + 1;

        break;
    }
    case 2:
    {
        ps_merged_tree->is_node_valid = ps_inter_tree->is_node_valid;
        ps_merged_tree->u1_inter_eval_enable = ps_inter_tree->is_node_valid;
        ps_merged_tree->u1_intra_eval_enable = 0;

        au1_children_recursive_call_type[POS_TL] = 2 + (NULL == ps_inter_tree->ps_child_node_tl);
        au1_children_recursive_call_type[POS_TR] = 2 + (NULL == ps_inter_tree->ps_child_node_tr);
        au1_children_recursive_call_type[POS_BL] = 2 + (NULL == ps_inter_tree->ps_child_node_bl);
        au1_children_recursive_call_type[POS_BR] = 2 + (NULL == ps_inter_tree->ps_child_node_br);

        break;
    }
    case 3:
    {
        /* The swamps of Dagobah! */
        ASSERT(0);

        break;
    }
    }

    switch(au1_children_recursive_call_type[POS_TL])
    {
    case 0:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tl,
            ps_intra_tree->ps_child_node_tl,
            ps_inter_tree->ps_child_node_tl,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 2:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tl,
            NULL,
            ps_inter_tree->ps_child_node_tl,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 1:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tl,
            ps_intra_tree->ps_child_node_tl,
            NULL,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    }

    switch(au1_children_recursive_call_type[POS_TR])
    {
    case 0:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tr,
            ps_intra_tree->ps_child_node_tr,
            ps_inter_tree->ps_child_node_tr,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 2:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tr,
            NULL,
            ps_inter_tree->ps_child_node_tr,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 1:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_tr,
            ps_intra_tree->ps_child_node_tr,
            NULL,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    }

    switch(au1_children_recursive_call_type[POS_BL])
    {
    case 0:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_bl,
            ps_intra_tree->ps_child_node_bl,
            ps_inter_tree->ps_child_node_bl,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 2:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_bl,
            NULL,
            ps_inter_tree->ps_child_node_bl,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 1:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_bl,
            ps_intra_tree->ps_child_node_bl,
            NULL,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    }

    switch(au1_children_recursive_call_type[POS_BR])
    {
    case 0:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_br,
            ps_intra_tree->ps_child_node_br,
            ps_inter_tree->ps_child_node_br,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 2:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_br,
            NULL,
            ps_inter_tree->ps_child_node_br,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    case 1:
    {
        ihevce_intra_and_inter_cuTree_merger(
            ps_merged_tree->ps_child_node_br,
            ps_intra_tree->ps_child_node_br,
            NULL,
            pi1_8x8CULevel_intraData_availability_indicator);

        break;
    }
    }
}
