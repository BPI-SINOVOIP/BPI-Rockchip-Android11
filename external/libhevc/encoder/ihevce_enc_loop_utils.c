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
* \file ihevce_enc_loop_utils.c
*
* \brief
*    This file contains utility functions of Encode loop
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
#include <limits.h>

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
#include "ihevc_common_tables.h"

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
#include "ihevce_nbr_avail.h"
#include "ihevce_enc_loop_utils.h"
#include "ihevce_sub_pic_rc.h"
#include "ihevce_global_tables.h"
#include "ihevce_bs_compute_ctb.h"
#include "ihevce_cabac_rdo.h"
#include "ihevce_deblk.h"
#include "ihevce_frame_process.h"
#include "ihevce_rc_enc_structs.h"
#include "hme_datatype.h"
#include "hme_interface.h"
#include "hme_common_defs.h"
#include "hme_defs.h"
#include "hme_common_utils.h"
#include "ihevce_me_instr_set_router.h"
#include "ihevce_enc_subpel_gen.h"
#include "ihevce_inter_pred.h"
#include "ihevce_mv_pred.h"
#include "ihevce_mv_pred_merge.h"
#include "ihevce_enc_loop_inter_mode_sifter.h"
#include "ihevce_enc_cu_recursion.h"
#include "ihevce_enc_loop_pass.h"
#include "ihevce_common_utils.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_sao.h"
#include "ihevce_tile_interface.h"
#include "ihevce_profile.h"
#include "ihevce_stasino_helpers.h"
#include "ihevce_tu_tree_selector.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

extern UWORD16 gau2_ihevce_cabac_bin_to_bits[64 * 2];
extern const UWORD8 gu1_hevce_scan4x4[3][16];
extern const UWORD8 gu1_hevce_sigcoeff_ctxtinc[4][16];
extern const UWORD8 gu1_hevce_sigcoeff_ctxtinc_tr4[16];
extern const UWORD8 gu1_hevce_sigcoeff_ctxtinc_00[16];

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define ENABLE_ZERO_CBF 1
#define DISABLE_RDOQ_INTRA 0

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void *ihevce_tu_tree_update(
    tu_prms_t *ps_tu_prms,
    WORD32 *pnum_tu_in_cu,
    WORD32 depth,
    WORD32 tu_split_flag,
    WORD32 tu_early_cbf,
    WORD32 i4_x_off,
    WORD32 i4_y_off)
{
    //WORD32 tu_split_flag = p_tu_split_flag[0];
    WORD32 p_tu_split_flag[4];
    WORD32 p_tu_early_cbf[4];

    WORD32 tu_size = ps_tu_prms->u1_tu_size;

    if(((tu_size >> depth) >= 16) && (tu_split_flag & 0x1))
    {
        if((tu_size >> depth) == 32)
        {
            /* Get the individual TU split flags */
            p_tu_split_flag[0] = (tu_split_flag >> 16) & 0x1F;
            p_tu_split_flag[1] = (tu_split_flag >> 11) & 0x1F;
            p_tu_split_flag[2] = (tu_split_flag >> 6) & 0x1F;
            p_tu_split_flag[3] = (tu_split_flag >> 1) & 0x1F;

            /* Get the early CBF flags */
            p_tu_early_cbf[0] = (tu_early_cbf >> 16) & 0x1F;
            p_tu_early_cbf[1] = (tu_early_cbf >> 11) & 0x1F;
            p_tu_early_cbf[2] = (tu_early_cbf >> 6) & 0x1F;
            p_tu_early_cbf[3] = (tu_early_cbf >> 1) & 0x1F;
        }
        else
        {
            /* Get the individual TU split flags */
            p_tu_split_flag[0] = ((tu_split_flag >> 4) & 0x1);
            p_tu_split_flag[1] = ((tu_split_flag >> 3) & 0x1);
            p_tu_split_flag[2] = ((tu_split_flag >> 2) & 0x1);
            p_tu_split_flag[3] = ((tu_split_flag >> 1) & 0x1);

            /* Get the early CBF flags */
            p_tu_early_cbf[0] = ((tu_early_cbf >> 4) & 0x1);
            p_tu_early_cbf[1] = ((tu_early_cbf >> 3) & 0x1);
            p_tu_early_cbf[2] = ((tu_early_cbf >> 2) & 0x1);
            p_tu_early_cbf[3] = ((tu_early_cbf >> 1) & 0x1);
        }

        ps_tu_prms = (tu_prms_t *)ihevce_tu_tree_update(
            ps_tu_prms,
            pnum_tu_in_cu,
            depth + 1,
            p_tu_split_flag[0],
            p_tu_early_cbf[0],
            i4_x_off,
            i4_y_off);

        ps_tu_prms = (tu_prms_t *)ihevce_tu_tree_update(
            ps_tu_prms,
            pnum_tu_in_cu,
            depth + 1,
            p_tu_split_flag[1],
            p_tu_early_cbf[1],
            (i4_x_off + (tu_size >> (depth + 1))),
            i4_y_off);

        ps_tu_prms = (tu_prms_t *)ihevce_tu_tree_update(
            ps_tu_prms,
            pnum_tu_in_cu,
            depth + 1,
            p_tu_split_flag[2],
            p_tu_early_cbf[2],
            i4_x_off,
            (i4_y_off + (tu_size >> (depth + 1))));

        ps_tu_prms = (tu_prms_t *)ihevce_tu_tree_update(
            ps_tu_prms,
            pnum_tu_in_cu,
            depth + 1,
            p_tu_split_flag[3],
            p_tu_early_cbf[3],
            (i4_x_off + (tu_size >> (depth + 1))),
            (i4_y_off + (tu_size >> (depth + 1))));
    }
    else
    {
        if(tu_split_flag & 0x1)
        {
            /* This piece of code will be entered for the 8x8, if it is split
            Update the 4 child TU's accordingly. */

            (*pnum_tu_in_cu) += 4;

            /* TL TU update */
            ps_tu_prms->u1_tu_size = tu_size >> (depth + 1);

            ps_tu_prms->u1_x_off = i4_x_off;

            ps_tu_prms->u1_y_off = i4_y_off;

            /* Early CBF is not done for 4x4 transforms */
            ps_tu_prms->i4_early_cbf = 1;

            ps_tu_prms++;

            /* TR TU update */
            ps_tu_prms->u1_tu_size = tu_size >> (depth + 1);

            ps_tu_prms->u1_x_off = i4_x_off + (tu_size >> (depth + 1));

            ps_tu_prms->u1_y_off = i4_y_off;

            /* Early CBF is not done for 4x4 transforms */
            ps_tu_prms->i4_early_cbf = 1;

            ps_tu_prms++;

            /* BL TU update */
            ps_tu_prms->u1_tu_size = tu_size >> (depth + 1);

            ps_tu_prms->u1_x_off = i4_x_off;

            ps_tu_prms->u1_y_off = i4_y_off + (tu_size >> (depth + 1));

            /* Early CBF is not done for 4x4 transforms */
            ps_tu_prms->i4_early_cbf = 1;

            ps_tu_prms++;

            /* BR TU update */
            ps_tu_prms->u1_tu_size = tu_size >> (depth + 1);

            ps_tu_prms->u1_x_off = i4_x_off + (tu_size >> (depth + 1));

            ps_tu_prms->u1_y_off = i4_y_off + (tu_size >> (depth + 1));

            /* Early CBF is not done for 4x4 transforms */
            ps_tu_prms->i4_early_cbf = 1;
        }
        else
        {
            /* Update the TU params */
            ps_tu_prms->u1_tu_size = tu_size >> depth;

            ps_tu_prms->u1_x_off = i4_x_off;

            ps_tu_prms->u1_y_off = i4_y_off;

            (*pnum_tu_in_cu)++;

            /* Early CBF update for current TU */
            ps_tu_prms->i4_early_cbf = tu_early_cbf & 0x1;
        }
        if((*pnum_tu_in_cu) < MAX_TU_IN_CTB)
        {
            ps_tu_prms++;

            ps_tu_prms->u1_tu_size = tu_size;
        }
    }

    return ps_tu_prms;
}

/*!
******************************************************************************
* \if Function name : ihevce_compute_quant_rel_param \endif
*
* \brief
*    This function updates quantization related parameters like qp_mod_6 etc in
*       context according to new qp
*
* \date
*    08/01/2013
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
void ihevce_compute_quant_rel_param(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD8 i1_cu_qp)
{
    WORD32 i4_div_factor;

    ps_ctxt->i4_chrm_cu_qp =
        (ps_ctxt->u1_chroma_array_type == 2)
            ? MIN(i1_cu_qp + ps_ctxt->i4_chroma_qp_offset, 51)
            : gai1_ihevc_chroma_qp_scale[i1_cu_qp + ps_ctxt->i4_chroma_qp_offset + MAX_QP_BD_OFFSET];
    ps_ctxt->i4_cu_qp_div6 = (i1_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) / 6;
    i4_div_factor = (i1_cu_qp + 3) / 6;
    i4_div_factor = CLIP3(i4_div_factor, 3, 6);
    ps_ctxt->i4_cu_qp_mod6 = (i1_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) % 6;
    ps_ctxt->i4_chrm_cu_qp_div6 = (ps_ctxt->i4_chrm_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) / 6;
    ps_ctxt->i4_chrm_cu_qp_mod6 = (ps_ctxt->i4_chrm_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) % 6;

#define INTER_RND_QP_BY_6
#ifdef INTER_RND_QP_BY_6
    /* quant factor without RDOQ is 1/6th of shift for inter : like in H264 */
    {
        ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER] =
            (WORD32)(((1 << QUANT_ROUND_FACTOR_Q) / (float)6) + 0.5f);
    }
#else
    /* quant factor without RDOQ is 1/6th of shift for inter : like in H264 */
    ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER] = (1 << QUANT_ROUND_FACTOR_Q) / 3;
#endif

    if(ISLICE == ps_ctxt->i1_slice_type)
    {
        /* quant factor without RDOQ is 1/3rd of shift for intra : like in H264 */
        ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTRA] =
            (WORD32)(((1 << QUANT_ROUND_FACTOR_Q) / (float)3) + 0.5f);
    }
    else
    {
        if(0) /*TRAQO_EXT_ENABLE_ONE_THIRD_RND*/
        {
            /* quant factor without RDOQ is 1/3rd of shift for intra : like in H264 */
            ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTRA] =
                (WORD32)(((1 << QUANT_ROUND_FACTOR_Q) / (float)3) + 0.5f);
        }
        else
        {
            /* quant factor without RDOQ is 1/6th of shift for intra in inter pic */
            ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTRA] =
                ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER];
            /* (1 << QUANT_ROUND_FACTOR_Q) / 6; */
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_populate_cl_cu_lambda_prms \endif
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
void ihevce_populate_cl_cu_lambda_prms(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    frm_lambda_ctxt_t *ps_frm_lamda,
    WORD32 i4_slice_type,
    WORD32 i4_temporal_lyr_id,
    WORD32 i4_lambda_type)
{
    WORD32 i4_curr_cu_qp, i4_curr_cu_qp_offset;
    double lambda_modifier;
    double lambda_uv_modifier;
    double lambda;
    double lambda_uv;

    WORD32 i4_qp_bdoffset = 6 * (ps_ctxt->u1_bit_depth - 8);

    /*Populate lamda modifier */
    ps_ctxt->i4_lamda_modifier = ps_frm_lamda->lambda_modifier;
    ps_ctxt->i4_uv_lamda_modifier = ps_frm_lamda->lambda_uv_modifier;
    ps_ctxt->i4_temporal_layer_id = i4_temporal_lyr_id;

    for(i4_curr_cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qp;
        i4_curr_cu_qp <= ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;
        i4_curr_cu_qp++)
    {
        WORD32 chroma_qp = (ps_ctxt->i4_chroma_format == IV_YUV_422SP_UV)
                               ? MIN(i4_curr_cu_qp, 51)
                               : gai1_ihevc_chroma_qp_scale[i4_curr_cu_qp + MAX_QP_BD_OFFSET];

        i4_curr_cu_qp_offset = i4_curr_cu_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

        lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bdoffset - 12)) / 3.0));
        lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

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
            if(ISLICE == ps_ctxt->i1_slice_type)
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
            i4_qp_bdoffset = 0;

            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bdoffset - 12)) / 3.0));
            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;

            ps_ctxt->au4_chroma_cost_weighing_factor_array[i4_curr_cu_qp_offset] =
                (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda * 1.9) * (1 << LAMBDA_Q_SHIFT));
            }

            ps_ctxt->i4_sad_lamda_array[i4_curr_cu_qp_offset] =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_type2_lambda_qf_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_curr_cu_qp_offset];

            ps_ctxt->i8_cl_ssd_type2_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf_array[i4_curr_cu_qp_offset];

            ps_ctxt->i4_satd_type2_lamda_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset];

            ps_ctxt->i4_sad_type2_lamda_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i4_sad_lamda_array[i4_curr_cu_qp_offset];

            break;
        }
        case 1:
        {
            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bdoffset - 12)) / 3.0));
            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;

            ps_ctxt->au4_chroma_cost_weighing_factor_array[i4_curr_cu_qp_offset] =
                (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda * 1.9) * (1 << LAMBDA_Q_SHIFT));
            }
            ps_ctxt->i4_sad_lamda_array[i4_curr_cu_qp_offset] =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_type2_lambda_qf_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_curr_cu_qp_offset];

            ps_ctxt->i8_cl_ssd_type2_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf_array[i4_curr_cu_qp_offset];

            ps_ctxt->i4_satd_type2_lamda_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset];

            ps_ctxt->i4_sad_type2_lamda_array[i4_curr_cu_qp_offset] =
                ps_ctxt->i4_sad_lamda_array[i4_curr_cu_qp_offset];

            break;
        }
        case 2:
        {
            lambda = pow(2.0, (((double)(i4_curr_cu_qp + i4_qp_bdoffset - 12)) / 3.0));
            lambda_uv = pow(2.0, (((double)(chroma_qp + i4_qp_bdoffset - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;

            ps_ctxt->au4_chroma_cost_weighing_factor_array[i4_curr_cu_qp_offset] =
                (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));

            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_satd_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda * 1.9) * (1 << LAMBDA_Q_SHIFT));
            }
            ps_ctxt->i4_sad_lamda_array[i4_curr_cu_qp_offset] =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

            /* lambda corresponding to 8- bit, for metrics based on 8- bit ( Example 8bit SAD in encloop)*/
            lambda = pow(2.0, (((double)(i4_curr_cu_qp - 12)) / 3.0));
            lambda_uv = pow(2.0, (((double)(chroma_qp - 12)) / 3.0));

            lambda *= lambda_modifier;
            lambda_uv *= lambda_uv_modifier;

            ps_ctxt->au4_chroma_cost_weighing_factor_array[i4_curr_cu_qp_offset] =
                (UWORD32)((lambda / lambda_uv) * (1 << CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_type2_lambda_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda * (1 << LAMBDA_Q_SHIFT));

            ps_ctxt->i8_cl_ssd_type2_lambda_chroma_qf_array[i4_curr_cu_qp_offset] =
                (LWORD64)(lambda_uv * (1 << LAMBDA_Q_SHIFT));
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                ps_ctxt->i4_satd_type2_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));
            }
            else
            {
                ps_ctxt->i4_satd_type2_lamda_array[i4_curr_cu_qp_offset] =
                    (WORD32)(sqrt(lambda * 1.9) * (1 << LAMBDA_Q_SHIFT));
            }

            ps_ctxt->i4_sad_type2_lamda_array[i4_curr_cu_qp_offset] =
                (WORD32)(sqrt(lambda) * (1 << LAMBDA_Q_SHIFT));

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

/*!
******************************************************************************
* \if Function name : ihevce_get_cl_cu_lambda_prms \endif
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
void ihevce_get_cl_cu_lambda_prms(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD32 i4_cur_cu_qp)
{
    WORD32 chroma_qp = (ps_ctxt->u1_chroma_array_type == 2)
                           ? MIN(i4_cur_cu_qp + ps_ctxt->i4_chroma_qp_offset, 51)
                           : gai1_ihevc_chroma_qp_scale
                                 [i4_cur_cu_qp + ps_ctxt->i4_chroma_qp_offset + MAX_QP_BD_OFFSET];

    /* closed loop ssd lambda is same as final lambda */
    ps_ctxt->i8_cl_ssd_lambda_qf =
        ps_ctxt->i8_cl_ssd_lambda_qf_array[i4_cur_cu_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
    ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
        ps_ctxt
            ->i8_cl_ssd_lambda_chroma_qf_array[chroma_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
    ps_ctxt->u4_chroma_cost_weighing_factor =
        ps_ctxt->au4_chroma_cost_weighing_factor_array
            [chroma_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
    /* --- Initialized the lambda for SATD computations --- */
    /* --- 0.95 is the multiplication factor as per HM --- */
    /* --- 1.9 is the multiplication factor for Hadamard Transform --- */
    ps_ctxt->i4_satd_lamda =
        ps_ctxt->i4_satd_lamda_array[i4_cur_cu_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
    ps_ctxt->i4_sad_lamda =
        ps_ctxt->i4_sad_type2_lamda_array[i4_cur_cu_qp + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset];
}

/*!
******************************************************************************
* \if Function name : ihevce_update_pred_qp \endif
*
* \brief
*    Computes pred qp for the given CU
*
* \param[in]
*
* \return
*
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_update_pred_qp(ihevce_enc_loop_ctxt_t *ps_ctxt, WORD32 cu_pos_x, WORD32 cu_pos_y)
{
    WORD32 i4_pred_qp = 0x7FFFFFFF;
    WORD32 i4_top, i4_left;
    if(cu_pos_x == 0 && cu_pos_y == 0) /*CTB start*/
    {
        i4_pred_qp = ps_ctxt->i4_prev_QP;
    }
    else
    {
        if(cu_pos_y == 0) /*CTB boundary*/
        {
            i4_top = ps_ctxt->i4_prev_QP;
        }
        else /*within CTB*/
        {
            i4_top = ps_ctxt->ai4_qp_qg[(cu_pos_y - 1) * 8 + (cu_pos_x)];
        }
        if(cu_pos_x == 0) /*CTB boundary*/
        {
            i4_left = ps_ctxt->i4_prev_QP;
        }
        else /*within CTB*/
        {
            i4_left = ps_ctxt->ai4_qp_qg[(cu_pos_y)*8 + (cu_pos_x - 1)];
        }
        i4_pred_qp = (i4_left + i4_top + 1) >> 1;
    }
    ps_ctxt->i4_pred_qp = i4_pred_qp;
    return;
}
/*!
******************************************************************************
* \if Function name : ihevce_compute_cu_level_QP \endif
*
* \brief
*    Computes cu level QP with Traqo,Spatial Mod and In-frame RC
*
* \param[in]
*
* \return
*
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_compute_cu_level_QP(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD32 i4_activity_for_qp,
    WORD32 i4_activity_for_lamda,
    WORD32 i4_reduce_qp)
{
    /*modify quant related param in ctxt based on current cu qp*/
    WORD32 i4_input_QP = ps_ctxt->i4_frame_mod_qp;
    WORD32 cu_qp = i4_input_QP + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset;

    WORD32 i4_max_qp_allowed;
    WORD32 i4_min_qp_allowed;
    WORD32 i4_pred_qp;

    i4_pred_qp = ps_ctxt->i4_pred_qp;

    if(ps_ctxt->i4_sub_pic_level_rc)
    {
        i4_max_qp_allowed = (i4_pred_qp + (25 + (ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset / 2)));
        i4_min_qp_allowed = (i4_pred_qp - (26 + (ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset / 2)));
    }
    else
    {
        i4_max_qp_allowed = (i4_input_QP + (7 + (ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset / 4)));
        i4_min_qp_allowed = (i4_input_QP - (18 + (ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset / 4)));
    }
    if((ps_ctxt->i1_slice_type == BSLICE) && (ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6))
        return;

#if LAMDA_BASED_ON_QUANT
    i4_activity_for_lamda = i4_activity_for_qp;
#endif

    if(i4_activity_for_qp != -1)
    {
        cu_qp = (ps_ctxt->ps_rc_quant_ctxt
                     ->pi4_qp_to_qscale[i4_input_QP + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]);
        if(ps_ctxt->i4_qp_mod)
        {
            /*Recompute the Qp as per enc thread's frame level Qp*/
            ASSERT(i4_activity_for_qp > 0);
            cu_qp = ((cu_qp * i4_activity_for_qp) + (1 << (QP_LEVEL_MOD_ACT_FACTOR - 1))) >>
                    QP_LEVEL_MOD_ACT_FACTOR;
        }

        // To avoid access of uninitialised Qscale to qp conversion table
        if(cu_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale;
        else if(cu_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale;

        cu_qp = ps_ctxt->ps_rc_quant_ctxt->pi4_qscale_to_qp[cu_qp];

        if((1 == i4_reduce_qp) && (cu_qp > 1))
            cu_qp--;

        /*CLIP the delta to obey standard allowed QP variation of (-26 + offset/2) to (25 + offset/2)*/
        if(cu_qp > i4_max_qp_allowed)
            cu_qp = i4_max_qp_allowed;
        else if(cu_qp < i4_min_qp_allowed)
            cu_qp = i4_min_qp_allowed;

        /* CLIP to maintain Qp between user configured and min and max Qp values*/
        if(cu_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qp)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;
        else if(cu_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qp)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qp;

        /*cu qp must be populated in cu_analyse_t struct*/
        ps_ctxt->i4_cu_qp = cu_qp;
        /*recompute quant related param at every cu level*/
        ihevce_compute_quant_rel_param(ps_ctxt, cu_qp);
    }

    /*Decoupling qp and lamda calculation */
    if(i4_activity_for_lamda != -1)
    {
        cu_qp = (ps_ctxt->ps_rc_quant_ctxt
                     ->pi4_qp_to_qscale[i4_input_QP + ps_ctxt->ps_rc_quant_ctxt->i1_qp_offset]);

        if(ps_ctxt->i4_qp_mod)
        {
#if MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON
            /*Recompute the Qp as per enc thread's frame level Qp*/
            ASSERT(i4_activity_for_lamda > 0);
            cu_qp = ((cu_qp * i4_activity_for_lamda) + (1 << (QP_LEVEL_MOD_ACT_FACTOR - 1))) >>
                    QP_LEVEL_MOD_ACT_FACTOR;
#endif
        }
        if(cu_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qscale;
        else if(cu_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qscale;

        cu_qp = ps_ctxt->ps_rc_quant_ctxt->pi4_qscale_to_qp[cu_qp];

        /*CLIP the delta to obey standard allowed QP variation of (-26 + offset/2) to (25 + offset/2)*/
        if(cu_qp > i4_max_qp_allowed)
            cu_qp = i4_max_qp_allowed;
        else if(cu_qp < i4_min_qp_allowed)
            cu_qp = i4_min_qp_allowed;

        /* CLIP to maintain Qp between user configured and min and max Qp values*/
        if(cu_qp > ps_ctxt->ps_rc_quant_ctxt->i2_max_qp)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_max_qp;
        else if(cu_qp < ps_ctxt->ps_rc_quant_ctxt->i2_min_qp)
            cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qp;
        /* get frame level lambda params */
        ihevce_get_cl_cu_lambda_prms(
            ps_ctxt, MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON ? cu_qp : ps_ctxt->i4_frame_qp);
    }
}

void ihevce_update_cu_level_qp_lamda(
    ihevce_enc_loop_ctxt_t *ps_ctxt, cu_analyse_t *ps_cu_analyse, WORD32 trans_size, WORD32 is_intra)
{
    WORD32 i4_act_counter = 0, i4_act_counter_lamda = 0;

    if(ps_cu_analyse->u1_cu_size == 64)
    {
        ASSERT((trans_size == 32) || (trans_size == 16) || (trans_size == 8) || (trans_size == 4));
        i4_act_counter = (trans_size == 16) + 2 * ((trans_size == 8) || (trans_size == 4));
        i4_act_counter_lamda = 3;
    }
    else if(ps_cu_analyse->u1_cu_size == 32)
    {
        ASSERT((trans_size == 32) || (trans_size == 16) || (trans_size == 8) || (trans_size == 4));
        i4_act_counter = (trans_size == 16) + 2 * ((trans_size == 8) || (trans_size == 4));
        i4_act_counter_lamda = 0;
    }
    else if(ps_cu_analyse->u1_cu_size == 16)
    {
        ASSERT((trans_size == 16) || (trans_size == 8) || (trans_size == 4));
        i4_act_counter = (trans_size == 8) || (trans_size == 4);
        i4_act_counter_lamda = 0;
    }
    else if(ps_cu_analyse->u1_cu_size == 8)
    {
        ASSERT((trans_size == 8) || (trans_size == 4));
        i4_act_counter = 1;
        i4_act_counter_lamda = 0;
    }
    else
    {
        ASSERT(0);
    }

    if(ps_ctxt->i4_use_ctb_level_lamda)
    {
        ihevce_compute_cu_level_QP(
            ps_ctxt, ps_cu_analyse->i4_act_factor[i4_act_counter][is_intra], -1, 0);
    }
    else
    {
        ihevce_compute_cu_level_QP(
            ps_ctxt,
            ps_cu_analyse->i4_act_factor[i4_act_counter][is_intra],
            ps_cu_analyse->i4_act_factor[i4_act_counter_lamda][is_intra],
            0);
    }

    ps_cu_analyse->i1_cu_qp = ps_ctxt->i4_cu_qp;
}

/**
*******************************************************************************
* \if Function name : ihevce_scan_coeffs \endif
*
* @brief * Computes the coeff buffer for a coded TU for entropy coding
*
* @par   Description
* Computes the coeff buffer for a coded TU for entropy coding
*
* \param[in] pi2_quan_coeffs Quantized coefficient context
*
* \param[in] scan_idx Scan index specifying the scan order
*
* \param[in] trans_size Transform unit size
*
* \param[inout] pu1_out_data output coeff buffer for a coded TU for entropy coding
*
* \param[in] pu1_csbf_buf csb flag buffer
*
* @returns num_bytes
* Number of bytes written to pu1_out_data
*
* @remarks
*
* \author
*  Ittiam
*
*******************************************************************************
*/

WORD32 ihevce_scan_coeffs(
    WORD16 *pi2_quant_coeffs,
    WORD32 *pi4_subBlock2csbfId_map,
    WORD32 scan_idx,
    WORD32 trans_size,
    UWORD8 *pu1_out_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 i4_csbf_stride)
{
    WORD32 i, trans_unit_idx, num_gt1_flag;
    UWORD16 u2_csbf0flags;
    WORD32 num_bytes = 0;
    UWORD8 *pu1_trans_table;
    UWORD8 *pu1_csb_table;
    WORD32 shift_value, mask_value;
    UWORD16 u2_sig_coeff_abs_gt0_flags = 0, u2_sig_coeff_abs_gt1_flags = 0;
    UWORD16 u2_sign_flags;
    UWORD16 u2_abs_coeff_remaining[16];
    WORD32 blk_row, blk_col;

    UWORD8 *pu1_out_data_header;
    UWORD16 *pu2_out_data_coeff;

    WORD32 x_pos, y_pos;
    WORD32 quant_coeff;

    WORD32 num_gt0_flag;
    (void)i4_csbf_stride;
    pu1_out_data_header = pu1_out_data;
    /* Need only last 3 bits, rest are reserved for debugging and making */
    /* WORD alignment */
    u2_csbf0flags = 0xBAD0;

    /* Select proper order for your transform unit and csb based on scan_idx*/
    /* and the trans_size */

    /* scan order inside a csb */
    pu1_csb_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);
    /* GETRANGE will give the log_2 of trans_size to shift_value */
    GETRANGE(shift_value, trans_size);
    shift_value = shift_value - 3; /* for finding. row no. from scan index */
    mask_value = (trans_size / 4) - 1; /*for finding the col. no. from scan index*/
    switch(trans_size)
    {
    case 32:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_8x8[scan_idx][0]);
        break;
    case 16:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);
        break;
    case 8:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_2x2[scan_idx][0]);
        break;
    case 4:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_1x1[0]);
        break;
    default:
        DBG_PRINTF("Invalid Trans Size\n");
        return -1;
        break;
    }

    /*go through each csb in the scan order for first non-zero coded sub-block*/
    for(trans_unit_idx = (trans_size * trans_size / 16) - 1; trans_unit_idx >= 0; trans_unit_idx--)
    {
        /* check for the first csb flag in our scan order */
        if(pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[trans_unit_idx]]])
        {
            UWORD8 u1_last_x, u1_last_y;
            /* row of csb */
            blk_row = pu1_trans_table[trans_unit_idx] >> shift_value;
            /* col of csb */
            blk_col = pu1_trans_table[trans_unit_idx] & mask_value;

            /*check for the 1st non-0 values inside the csb in our scan order*/
            for(i = 15; i >= 0; i--)
            {
                x_pos = (pu1_csb_table[i] & 0x3) + blk_col * 4;
                y_pos = (pu1_csb_table[i] >> 2) + blk_row * 4;

                quant_coeff = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];

                if(quant_coeff != 0)
                    break;
            }

            ASSERT(i >= 0);

            u1_last_x = x_pos;
            u1_last_y = y_pos;

            /* storing last_x and last_y */
            *pu1_out_data_header = u1_last_x;
            pu1_out_data_header++;
            num_bytes++;
            *pu1_out_data_header = u1_last_y;
            pu1_out_data_header++;
            num_bytes++;

            /* storing the scan order */
            *pu1_out_data_header = scan_idx;
            pu1_out_data_header++;
            num_bytes++;
            /* storing last_sub_block pos. in scan order count */
            *pu1_out_data_header = trans_unit_idx;
            pu1_out_data_header++;
            num_bytes++;

            /*stored the first 4 bytes, now all are word16. So word16 pointer*/
            pu2_out_data_coeff = (UWORD16 *)pu1_out_data_header;

            /* u2_csbf0flags word */
            u2_csbf0flags = 0xBAD0 | 1; /*since right&bottom csbf is 0*/
            /* storing u2_csbf0flags word */
            *pu2_out_data_coeff = u2_csbf0flags;
            pu2_out_data_coeff++;
            num_bytes += 2;

            num_gt0_flag = 1;
            num_gt1_flag = 0;
            u2_sign_flags = 0;

            /* set the i th bit of u2_sig_coeff_abs_gt0_flags */
            u2_sig_coeff_abs_gt0_flags = u2_sig_coeff_abs_gt0_flags | (1 << i);
            if(abs(quant_coeff) > 1)
            {
                /* set the i th bit of u2_sig_coeff_abs_gt1_flags */
                u2_sig_coeff_abs_gt1_flags = u2_sig_coeff_abs_gt1_flags | (1 << i);
                /* update u2_abs_coeff_remaining */
                u2_abs_coeff_remaining[num_gt1_flag] = (UWORD16)abs(quant_coeff) - 1;

                num_gt1_flag++;
            }

            if(quant_coeff < 0)
            {
                /* set the i th bit of u2_sign_flags */
                u2_sign_flags = u2_sign_flags | (1 << i);
            }

            /* Test remaining elements in our scan order */
            /* Can optimize further by CLZ macro */
            for(i = i - 1; i >= 0; i--)
            {
                x_pos = (pu1_csb_table[i] & 0x3) + blk_col * 4;
                y_pos = (pu1_csb_table[i] >> 2) + blk_row * 4;

                quant_coeff = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];

                if(quant_coeff != 0)
                {
                    /* set the i th bit of u2_sig_coeff_abs_gt0_flags */
                    u2_sig_coeff_abs_gt0_flags |= (1 << i);

                    if((abs(quant_coeff) > 1) || (num_gt0_flag >= MAX_GT_ONE))
                    {
                        /* set the i th bit of u2_sig_coeff_abs_gt1_flags */
                        u2_sig_coeff_abs_gt1_flags |= (1 << i);

                        /* update u2_abs_coeff_remaining */
                        u2_abs_coeff_remaining[num_gt1_flag] = (UWORD16)abs(quant_coeff) - 1;

                        num_gt1_flag++; /*n0. of Ones in sig_coeff_abs_gt1_flag*/
                    }

                    if(quant_coeff < 0)
                    {
                        /* set the i th bit of u2_sign_flags */
                        u2_sign_flags |= (1 << i);
                    }

                    num_gt0_flag++;
                }
            }

            /* storing u2_sig_coeff_abs_gt0_flags 2 bytes */
            *pu2_out_data_coeff = u2_sig_coeff_abs_gt0_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;
            /* storing u2_sig_coeff_abs_gt1_flags 2 bytes */
            *pu2_out_data_coeff = u2_sig_coeff_abs_gt1_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;
            /* storing u2_sign_flags 2 bytes */
            *pu2_out_data_coeff = u2_sign_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;

            /* Store the u2_abs_coeff_remaining[] */
            for(i = 0; i < num_gt1_flag; i++)
            {
                /* storing u2_abs_coeff_remaining[i] 2 bytes */
                *pu2_out_data_coeff = u2_abs_coeff_remaining[i];
                pu2_out_data_coeff++;
                num_bytes += 2;
            }

            break; /*We just need this loop for finding 1st non-zero csb only*/
        }
    }

    /* go through remaining csb in the scan order */
    for(trans_unit_idx = trans_unit_idx - 1; trans_unit_idx >= 0; trans_unit_idx--)
    {
        blk_row = pu1_trans_table[trans_unit_idx] >> shift_value; /*row of csb*/
        blk_col = pu1_trans_table[trans_unit_idx] & mask_value; /*col of csb*/

        /* u2_csbf0flags word */
        u2_csbf0flags = 0xBAD0 | /* assuming csbf_buf has only 0 or 1 values */
                        (pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[trans_unit_idx]]]);

        /********************************************************************/
        /* Minor hack: As per HEVC spec csbf in not signalled in stream for */
        /* block0, instead sig coeff map is directly signalled. This is     */
        /* taken care by forcing csbf for block0 to be 1 even if it is 0    */
        /********************************************************************/
        if(0 == trans_unit_idx)
        {
            u2_csbf0flags |= 1;
        }

        if((blk_col + 1 < trans_size / 4)) /* checking right boundary */
        {
            if(pu1_csbf_buf[pi4_subBlock2csbfId_map[blk_row * trans_size / 4 + blk_col + 1]])
            {
                /* set the 2nd bit of u2_csbf0flags for right csbf */
                u2_csbf0flags = u2_csbf0flags | (1 << 1);
            }
        }
        if((blk_row + 1 < trans_size / 4)) /* checking bottom oundary */
        {
            if(pu1_csbf_buf[pi4_subBlock2csbfId_map[(blk_row + 1) * trans_size / 4 + blk_col]])
            {
                /* set the 3rd bit of u2_csbf0flags  for bottom csbf */
                u2_csbf0flags = u2_csbf0flags | (1 << 2);
            }
        }

        /* storing u2_csbf0flags word */
        *pu2_out_data_coeff = u2_csbf0flags;
        pu2_out_data_coeff++;
        num_bytes += 2;

        /* check for the csb flag in our scan order */
        if(u2_csbf0flags & 0x1)
        {
            u2_sig_coeff_abs_gt0_flags = 0;
            u2_sig_coeff_abs_gt1_flags = 0;
            u2_sign_flags = 0;

            num_gt0_flag = 0;
            num_gt1_flag = 0;
            /* check for the non-0 values inside the csb in our scan order */
            /* Can optimize further by CLZ macro */
            for(i = 15; i >= 0; i--)
            {
                x_pos = (pu1_csb_table[i] & 0x3) + blk_col * 4;
                y_pos = (pu1_csb_table[i] >> 2) + blk_row * 4;

                quant_coeff = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];

                if(quant_coeff != 0)
                {
                    /* set the i th bit of u2_sig_coeff_abs_gt0_flags */
                    u2_sig_coeff_abs_gt0_flags |= (1 << i);

                    if((abs(quant_coeff) > 1) || (num_gt0_flag >= MAX_GT_ONE))
                    {
                        /* set the i th bit of u2_sig_coeff_abs_gt1_flags */
                        u2_sig_coeff_abs_gt1_flags |= (1 << i);

                        /* update u2_abs_coeff_remaining */
                        u2_abs_coeff_remaining[num_gt1_flag] = (UWORD16)abs(quant_coeff) - 1;

                        num_gt1_flag++;
                    }

                    if(quant_coeff < 0)
                    {
                        /* set the i th bit of u2_sign_flags */
                        u2_sign_flags = u2_sign_flags | (1 << i);
                    }

                    num_gt0_flag++;
                }
            }

            /* storing u2_sig_coeff_abs_gt0_flags 2 bytes */
            *pu2_out_data_coeff = u2_sig_coeff_abs_gt0_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;

            /* storing u2_sig_coeff_abs_gt1_flags 2 bytes */
            *pu2_out_data_coeff = u2_sig_coeff_abs_gt1_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;

            /* storing u2_sign_flags 2 bytes */
            *pu2_out_data_coeff = u2_sign_flags;
            pu2_out_data_coeff++;
            num_bytes += 2;

            /* Store the u2_abs_coeff_remaining[] */
            for(i = 0; i < num_gt1_flag; i++)
            {
                /* storing u2_abs_coeff_remaining[i] 2 bytes */
                *pu2_out_data_coeff = u2_abs_coeff_remaining[i];
                pu2_out_data_coeff++;
                num_bytes += 2;
            }
        }
    }

    return num_bytes; /* Return the number of bytes written to out_data */
}

/**
*******************************************************************************
* \if Function name : ihevce_populate_intra_pred_mode \endif
*
* \brief * populates intra pred modes,b2_mpm_idx,b1_prev_intra_luma_pred_flag &
* b5_rem_intra_pred_mode for a CU based on nieghbouring CUs,
*
* \par   Description
* Computes the b1_prev_intra_luma_pred_flag, b2_mpm_idx & b5_rem_intra_pred_mode
* for a CU
*
* \param[in] top_intra_mode Top intra mode
* \param[in] left_intra_mode Left intra mode
* \param[in] available_top Top availability flag
* \param[in] available_left Left availability flag
* \param[in] cu_pos_y CU 'y' position
* \param[in] ps_cand_mode_list pointer to populate candidate list
*
* \returns none
*
* \author
*  Ittiam
*
*******************************************************************************
*/

void ihevce_populate_intra_pred_mode(
    WORD32 top_intra_mode,
    WORD32 left_intra_mode,
    WORD32 available_top,
    WORD32 available_left,
    WORD32 cu_pos_y,
    WORD32 *ps_cand_mode_list)
{
    /* local variables */
    WORD32 cand_intra_pred_mode_left, cand_intra_pred_mode_top;

    /* Calculate cand_intra_pred_mode_N as per sec. 8.4.2 in JCTVC-J1003_d7 */
    /* N = top */
    if(0 == available_top)
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else if(0 == cu_pos_y) /* It's on the CTB boundary */
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    else
    {
        cand_intra_pred_mode_top = top_intra_mode;
    }

    /* N = left */
    if(0 == available_left)
    {
        cand_intra_pred_mode_left = INTRA_DC;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else
    {
        cand_intra_pred_mode_left = left_intra_mode;
    }

    /* Calculate cand_mode_list as per sec. 8.4.2 in JCTVC-J1003_d7 */
    if(cand_intra_pred_mode_left == cand_intra_pred_mode_top)
    {
        if(cand_intra_pred_mode_left < 2)
        {
            ps_cand_mode_list[0] = INTRA_PLANAR;
            ps_cand_mode_list[1] = INTRA_DC;
            ps_cand_mode_list[2] = INTRA_ANGULAR(26); /* angular 26 = Vertical */
        }
        else
        {
            ps_cand_mode_list[0] = cand_intra_pred_mode_left;
            ps_cand_mode_list[1] = 2 + ((cand_intra_pred_mode_left + 29) % 32);
            ps_cand_mode_list[2] = 2 + ((cand_intra_pred_mode_left - 2 + 1) % 32);
        }
    }
    else
    {
        ps_cand_mode_list[0] = cand_intra_pred_mode_left;
        ps_cand_mode_list[1] = cand_intra_pred_mode_top;

        if((cand_intra_pred_mode_left != INTRA_PLANAR) &&
           (cand_intra_pred_mode_top != INTRA_PLANAR))
        {
            ps_cand_mode_list[2] = INTRA_PLANAR;
        }
        else if((cand_intra_pred_mode_left != INTRA_DC) && (cand_intra_pred_mode_top != INTRA_DC))
        {
            ps_cand_mode_list[2] = INTRA_DC;
        }
        else
        {
            ps_cand_mode_list[2] = INTRA_ANGULAR(26);
        }
    }
}
/**
*******************************************************************************
* \if Function name : ihevce_intra_pred_mode_signaling \endif
*
* \brief * Computes the b1_prev_intra_luma_pred_flag, b2_mpm_idx &
* b5_rem_intra_pred_mode for a CU
*
* \par   Description
* Computes the b1_prev_intra_luma_pred_flag, b2_mpm_idx & b5_rem_intra_pred_mode
* for a CU
*
* \param[in] ps_nbr_top Top neighbour context
* \param[in] ps_nbr_left Left neighbour context
* \param[in] available_top Top availability flag
* \param[in] available_left Left availability flag
* \param[in] cu_pos_y CU 'y' position
* \param[in] luma_intra_pred_mode_current the intra_pred_mode of current block
* \param[inout] ps_intra_pred_mode_current
* Pointer to structure having b1_prev_intra_luma_pred_flag, b2_mpm_idx and
* b5_rem_intra_pred_mode
*
* \returns none
*
* \author
*  Ittiam
*
*******************************************************************************
*/

void ihevce_intra_pred_mode_signaling(
    WORD32 top_intra_mode,
    WORD32 left_intra_mode,
    WORD32 available_top,
    WORD32 available_left,
    WORD32 cu_pos_y,
    WORD32 luma_intra_pred_mode_current,
    intra_prev_rem_flags_t *ps_intra_pred_mode_current)
{
    /* local variables */
    WORD32 cand_intra_pred_mode_left, cand_intra_pred_mode_top;
    WORD32 cand_mode_list[3];

    ps_intra_pred_mode_current->b1_prev_intra_luma_pred_flag = 0;
    ps_intra_pred_mode_current->b2_mpm_idx = 0;  // for safety purpose
    ps_intra_pred_mode_current->b5_rem_intra_pred_mode = 0;

    /* Calculate cand_intra_pred_mode_N as per sec. 8.4.2 in JCTVC-J1003_d7 */
    /* N = top */
    if(0 == available_top)
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else if(0 == cu_pos_y) /* It's on the CTB boundary */
    {
        cand_intra_pred_mode_top = INTRA_DC;
    }
    else
    {
        cand_intra_pred_mode_top = top_intra_mode;
    }

    /* N = left */
    if(0 == available_left)
    {
        cand_intra_pred_mode_left = INTRA_DC;
    }
    /* for neighbour != INTRA, setting DC is done outside */
    else
    {
        cand_intra_pred_mode_left = left_intra_mode;
    }

    /* Calculate cand_mode_list as per sec. 8.4.2 in JCTVC-J1003_d7 */
    if(cand_intra_pred_mode_left == cand_intra_pred_mode_top)
    {
        if(cand_intra_pred_mode_left < 2)
        {
            cand_mode_list[0] = INTRA_PLANAR;
            cand_mode_list[1] = INTRA_DC;
            cand_mode_list[2] = INTRA_ANGULAR(26); /* angular 26 = Vertical */
        }
        else
        {
            cand_mode_list[0] = cand_intra_pred_mode_left;
            cand_mode_list[1] = 2 + ((cand_intra_pred_mode_left + 29) % 32);
            cand_mode_list[2] = 2 + ((cand_intra_pred_mode_left - 2 + 1) % 32);
        }
    }
    else
    {
        cand_mode_list[0] = cand_intra_pred_mode_left;
        cand_mode_list[1] = cand_intra_pred_mode_top;

        if((cand_intra_pred_mode_left != INTRA_PLANAR) &&
           (cand_intra_pred_mode_top != INTRA_PLANAR))
        {
            cand_mode_list[2] = INTRA_PLANAR;
        }
        else if((cand_intra_pred_mode_left != INTRA_DC) && (cand_intra_pred_mode_top != INTRA_DC))
        {
            cand_mode_list[2] = INTRA_DC;
        }
        else
        {
            cand_mode_list[2] = INTRA_ANGULAR(26);
        }
    }

    /* Signal Generation */

    /* Flag & mpm_index generation */
    if(cand_mode_list[0] == luma_intra_pred_mode_current)
    {
        ps_intra_pred_mode_current->b1_prev_intra_luma_pred_flag = 1;
        ps_intra_pred_mode_current->b2_mpm_idx = 0;
    }
    else if(cand_mode_list[1] == luma_intra_pred_mode_current)
    {
        ps_intra_pred_mode_current->b1_prev_intra_luma_pred_flag = 1;
        ps_intra_pred_mode_current->b2_mpm_idx = 1;
    }
    else if(cand_mode_list[2] == luma_intra_pred_mode_current)
    {
        ps_intra_pred_mode_current->b1_prev_intra_luma_pred_flag = 1;
        ps_intra_pred_mode_current->b2_mpm_idx = 2;
    }
    /* Flag & b5_rem_intra_pred_mode generation */
    else
    {
        WORD32 rem_mode;

        ps_intra_pred_mode_current->b1_prev_intra_luma_pred_flag = 0;

        /* sorting cand_mode_list */
        if(cand_mode_list[0] > cand_mode_list[1])
        {
            SWAP(cand_mode_list[0], cand_mode_list[1]);
        }
        if(cand_mode_list[0] > cand_mode_list[2])
        {
            SWAP(cand_mode_list[0], cand_mode_list[2]);
        }
        if(cand_mode_list[1] > cand_mode_list[2])
        {
            SWAP(cand_mode_list[1], cand_mode_list[2]);
        }

        rem_mode = luma_intra_pred_mode_current;

        if((rem_mode) >= cand_mode_list[2])
        {
            (rem_mode)--;
        }
        if((rem_mode) >= cand_mode_list[1])
        {
            (rem_mode)--;
        }
        if((rem_mode) >= cand_mode_list[0])
        {
            (rem_mode)--;
        }
        ps_intra_pred_mode_current->b5_rem_intra_pred_mode = rem_mode;
    }
}

void ihevce_quant_rounding_factor_gen(
    WORD32 i4_trans_size,
    WORD32 is_luma,
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    WORD32 *pi4_quant_round_0_1,
    WORD32 *pi4_quant_round_1_2,
    double i4_lamda_modifier,
    UWORD8 i4_is_tu_level_quant_rounding)
{
    //WORD32 i4_scan_idx = ps_ctxt->i4_scan_idx;
    UWORD8 *pu1_ctxt_model;
    WORD32 scan_pos;
    WORD32 sig_coeff_base_ctxt; /* cabac context for sig coeff flag    */
    WORD32 abs_gt1_base_ctxt;
    WORD32 log2_tr_size, i;
    UWORD16 u4_bits_estimated_r0, u4_bits_estimated_r1, u4_bits_estimated_r2;
    UWORD16 u4_bits_estimated_r1_temp;
    WORD32 j = 0;
    WORD32 k = 0;
    WORD32 temp2;

    double i4_lamda_mod = i4_lamda_modifier * pow(2.0, (-8.0 / 3.0));
    LWORD64 lamda_mod = (LWORD64)(i4_lamda_mod * (1 << LAMDA_Q_SHIFT_FACT));
    /* transform size to log2transform size */
    GETRANGE(log2_tr_size, i4_trans_size);
    log2_tr_size -= 1;

    if(1 == i4_is_tu_level_quant_rounding)
    {
        entropy_context_t *ps_cur_tu_entropy;
        cab_ctxt_t *ps_cabac;
        WORD32 curr_buf_idx = ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
        ps_cur_tu_entropy = &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[curr_buf_idx];

        ps_cabac = &ps_cur_tu_entropy->s_cabac_ctxt;

        pu1_ctxt_model = &ps_cabac->au1_ctxt_models[0];
    }
    else
    {
        pu1_ctxt_model = &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0];
    }
    /*If transform size is 4x4, then only one sub-block*/
    if(is_luma)
    {
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG;
        abs_gt1_base_ctxt = IHEVC_CAB_COEFABS_GRTR1_FLAG;

        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            /* Assuming diagnol scan idx for now */
            sig_coeff_base_ctxt += 9;
        }
        else if(3 < log2_tr_size)
        {
            /* larger transform sizes */
            sig_coeff_base_ctxt += 21;
        }
    }
    else
    {
        /* chroma context initializations */
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG + 27;
        abs_gt1_base_ctxt = IHEVC_CAB_COEFABS_GRTR1_FLAG + 16;

        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            sig_coeff_base_ctxt += 9;
        }
        else if(3 < log2_tr_size)
        {
            /* larger transform sizes */
            sig_coeff_base_ctxt += 12;
        }
    }

    /*Transform size of 4x4 will have only a single CSB */
    /* derive the context inc as per section 9.3.3.1.4 */

    if(2 == log2_tr_size)
    {
        UWORD8 sig_ctxinc;
        WORD32 state_mps;
        WORD32 gt1_ctxt = 0;
        WORD32 ctxt_set = 0;
        WORD32 ctxt_idx = 0;

        /* context set based on luma subblock pos */

        /* Encodet the abs level gt1 bins */
        /* Currently calculating trade off between mps(2) and mps(1)*/
        /* The estimation has to be further done for mps(11) and mps(111)*/
        /*ctxt_set = 0 as transform 4x4 has only one csb with DC */
        /* gt1_ctxt = 0 for the co-ef value to be 2 */

        ctxt_set = gt1_ctxt = 0;
        ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

        state_mps = pu1_ctxt_model[ctxt_idx];

        u4_bits_estimated_r2 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1];

        u4_bits_estimated_r1_temp = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

        QUANT_ROUND_FACTOR(temp2, u4_bits_estimated_r2, u4_bits_estimated_r1_temp, lamda_mod);
        for(scan_pos = 0; scan_pos < 16; scan_pos++)
        {
            *(pi4_quant_round_1_2 + scan_pos) = temp2;
        }

        for(scan_pos = 0; scan_pos < 16; scan_pos++)
        {
            //UWORD8 nbr_csbf = 1;
            /* derive the x,y pos */
            UWORD8 y_pos_x_pos = scan_pos;  //gu1_hevce_scan4x4[i4_scan_idx][scan_pos];

            /* 4x4 transform size increment uses lookup */
            sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc_tr4[y_pos_x_pos];

            /*Get the mps state based on ctxt modes */
            state_mps = pu1_ctxt_model[sig_ctxinc + sig_coeff_base_ctxt];

            /* Bits taken to encode sig co-ef flag as 0 */
            u4_bits_estimated_r0 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

            /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
            //
            u4_bits_estimated_r1 =
                (gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1] + ROUND_Q12(1.000000000));

            /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
            u4_bits_estimated_r1 += u4_bits_estimated_r1_temp;

            QUANT_ROUND_FACTOR(temp2, u4_bits_estimated_r1, u4_bits_estimated_r0, lamda_mod);
            *(pi4_quant_round_0_1 + scan_pos) = temp2;
        }
    }
    else
    {
        UWORD8 *pu1_hevce_sigcoeff_ctxtinc;
        WORD32 is_nbr_csb_state_mps;

        WORD32 state_mps;
        WORD32 gt1_ctxt = 0;
        WORD32 ctxt_set = 0;
        WORD32 ctxt_idx;
        /*1to2 rounding factor is same for all sub blocks except for sub-block = 0*/
        /*Hence will write all the sub-block with i >=1 coeff, and then overwrite for i = 0*/

        /*ctxt_set = 0 DC subblock, the previous state did not have 2
        ctxt_set = 1 DC subblock, the previous state did have >= 2
        ctxt_set = 2 AC subblock, the previous state did not have 2
        ctxt_set = 3 AC subblock, the previous state did have >= 2*/
        i = 1;
        ctxt_set = (i && is_luma) ? 2 : 0;

        ctxt_set++;

        /*0th position indicates the probability of 2 */
        /*1th position indicates the probability of 1 */
        /*2th position indicates the probability of 11 */
        /*3th position indicates the probability of 111 */

        gt1_ctxt = 0;
        ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

        state_mps = pu1_ctxt_model[ctxt_idx];

        u4_bits_estimated_r2 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1];

        u4_bits_estimated_r1 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];
        QUANT_ROUND_FACTOR(temp2, u4_bits_estimated_r2, u4_bits_estimated_r1, lamda_mod);

        for(scan_pos = 0; scan_pos < (16 * (i4_trans_size * i4_trans_size >> 4)); scan_pos++)
        {
            *(pi4_quant_round_1_2 + scan_pos) = temp2;
        }

        i = 0;
        ctxt_set = (i && is_luma) ? 2 : 0;
        ctxt_set++;

        /*0th position indicates the probability of 2 */
        /*1th position indicates the probability of 1 */
        /*2th position indicates the probability of 11 */
        /*3th position indicates the probability of 111 */

        gt1_ctxt = 0;
        ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

        state_mps = pu1_ctxt_model[ctxt_idx];

        u4_bits_estimated_r2 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1];

        u4_bits_estimated_r1 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];
        QUANT_ROUND_FACTOR(temp2, u4_bits_estimated_r2, u4_bits_estimated_r1, lamda_mod);

        for(scan_pos = 0; scan_pos < 16; scan_pos++)
        {
            *(pi4_quant_round_1_2 + ((scan_pos % 4) + ((scan_pos >> 2) * i4_trans_size))) = temp2;
        }

        {
            WORD32 ctxt_idx;

            WORD32 nbr_csbf_0, nbr_csbf_1;
            WORD32 state_mps_0, state_mps_1;
            ctxt_idx = IHEVC_CAB_CODED_SUBLK_IDX;
            ctxt_idx += is_luma ? 0 : 2;

            /* ctxt based on right / bottom avail csbf, section 9.3.3.1.3 */
            /* if neibhor not available, ctxt idx = 0*/
            nbr_csbf_0 = 0;
            ctxt_idx += nbr_csbf_0 ? 1 : 0;
            state_mps_0 = pu1_ctxt_model[ctxt_idx];

            nbr_csbf_1 = 1;
            ctxt_idx += nbr_csbf_1 ? 1 : 0;
            state_mps_1 = pu1_ctxt_model[ctxt_idx];

            is_nbr_csb_state_mps = ((state_mps_0 % 2) == 1) && ((state_mps_1 % 2) == 1);
        }

        if(1 == is_nbr_csb_state_mps)
        {
            for(i = 0; i < (i4_trans_size * i4_trans_size >> 4); i++)
            {
                UWORD8 sig_ctxinc;
                WORD32 state_mps;
                WORD32 gt1_ctxt = 0;
                WORD32 ctxt_set = 0;

                WORD32 ctxt_idx;

                /*Check if the cabac states had previous nbr available */

                if(i == 0)
                    pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc[3][0];
                else if(i < (i4_trans_size >> 2))
                    pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc[1][0];
                else if((i % (i4_trans_size >> 2)) == 0)
                    pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc[2][0];
                else
                    pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc[0][0];

                if(((i % (i4_trans_size >> 2)) == 0) && (i != 0))
                    k++;

                j = ((i4_trans_size * 4) * k) + ((i % (i4_trans_size >> 2)) * 4);
                /*ctxt_set = 0 DC subblock, the previous state did not have 2
                ctxt_set = 1 DC subblock, the previous state did have >= 2
                ctxt_set = 2 AC subblock, the previous state did not have 2
                ctxt_set = 3 AC subblock, the previous state did have >= 2*/

                ctxt_set = (i && is_luma) ? 2 : 0;

                /* gt1_ctxt = 1 for the co-ef value to be 1 */
                gt1_ctxt = 0;
                ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                state_mps = pu1_ctxt_model[ctxt_idx];

                /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
                u4_bits_estimated_r1_temp = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                for(scan_pos = 0; scan_pos < 16; scan_pos++)
                {
                    UWORD8 y_pos_x_pos;

                    if(scan_pos || i)
                    {
                        y_pos_x_pos = scan_pos;  // gu1_hevce_scan4x4[i4_scan_idx][scan_pos];
                        /* ctxt for AC coeff depends on curpos and neigbour csbf */
                        sig_ctxinc = pu1_hevce_sigcoeff_ctxtinc[y_pos_x_pos];

                        /* based on luma subblock pos */
                        sig_ctxinc += (i && is_luma) ? 3 : 0;

                        sig_ctxinc += sig_coeff_base_ctxt;
                    }
                    else
                    {
                        /*MAM : both scan pos and i 0 impies the DC coef of 1st block only */
                        /* DC coeff has fixed context for luma and chroma */
                        sig_ctxinc = is_luma ? IHEVC_CAB_COEFF_FLAG : IHEVC_CAB_COEFF_FLAG + 27;
                    }

                    /*Get the mps state based on ctxt modes */
                    state_mps = pu1_ctxt_model[sig_ctxinc];

                    /* Bits taken to encode sig co-ef flag as 0 */
                    u4_bits_estimated_r0 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                    u4_bits_estimated_r1 =
                        (gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1] + ROUND_Q12(1.000000000));

                    /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
                    u4_bits_estimated_r1 += u4_bits_estimated_r1_temp;
                    {
                        QUANT_ROUND_FACTOR(
                            temp2, u4_bits_estimated_r1, u4_bits_estimated_r0, lamda_mod);
                        *(pi4_quant_round_0_1 +
                          ((scan_pos % 4) + ((scan_pos >> 2) * i4_trans_size)) + j) = temp2;
                    }
                }
            }
        }
        else
        {
            /*If Both nbr csbfs are 0, then all the coef in sub-blocks will have same value except for 1st subblock,
            Hence will write the same value to all sub block, and overwrite for the 1st one */
            i = 1;
            {
                UWORD8 sig_ctxinc;
                UWORD8 y_pos_x_pos;
                WORD32 quant_rounding_0_1;

                pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc_00[0];

                scan_pos = 0;
                y_pos_x_pos = scan_pos;  // gu1_hevce_scan4x4[i4_scan_idx][scan_pos];
                /* ctxt for AC coeff depends on curpos and neigbour csbf */
                sig_ctxinc = pu1_hevce_sigcoeff_ctxtinc[y_pos_x_pos];

                /* based on luma subblock pos */
                sig_ctxinc += (is_luma) ? 3 : 0;

                sig_ctxinc += sig_coeff_base_ctxt;

                /*Get the mps state based on ctxt modes */
                state_mps = pu1_ctxt_model[sig_ctxinc];

                /* Bits taken to encode sig co-ef flag as 0 */
                u4_bits_estimated_r0 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                u4_bits_estimated_r1 =
                    (gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1] + ROUND_Q12(1.000000000));

                /*ctxt_set = 0 DC subblock, the previous state did not have 2
                ctxt_set = 1 DC subblock, the previous state did have >= 2
                ctxt_set = 2 AC subblock, the previous state did not have 2
                ctxt_set = 3 AC subblock, the previous state did have >= 2*/

                ctxt_set = (i && is_luma) ? 2 : 0;

                /* gt1_ctxt = 1 for the co-ef value to be 1 */
                gt1_ctxt = 0;
                ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                state_mps = pu1_ctxt_model[ctxt_idx];

                /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
                u4_bits_estimated_r1 += gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                QUANT_ROUND_FACTOR(
                    quant_rounding_0_1, u4_bits_estimated_r1, u4_bits_estimated_r0, lamda_mod);

                for(scan_pos = 0; scan_pos < (16 * (i4_trans_size * i4_trans_size >> 4));
                    scan_pos++)
                {
                    *(pi4_quant_round_0_1 + scan_pos) = quant_rounding_0_1;
                }
            }

            /*First Subblock*/
            i = 0;

            {
                UWORD8 sig_ctxinc;
                WORD32 state_mps;
                WORD32 gt1_ctxt = 0;
                WORD32 ctxt_set = 0;

                WORD32 ctxt_idx;

                /*Check if the cabac states had previous nbr available */

                {
                    pu1_hevce_sigcoeff_ctxtinc = (UWORD8 *)&gu1_hevce_sigcoeff_ctxtinc[0][0];

                    /*ctxt_set = 0 DC subblock, the previous state did not have 2
                    ctxt_set = 1 DC subblock, the previous state did have >= 2
                    ctxt_set = 2 AC subblock, the previous state did not have 2
                    ctxt_set = 3 AC subblock, the previous state did have >= 2*/
                    ctxt_set = (i && is_luma) ? 2 : 0;

                    /* gt1_ctxt = 1 for the co-ef value to be 1 */
                    gt1_ctxt = 0;
                    ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                    state_mps = pu1_ctxt_model[ctxt_idx];

                    /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
                    u4_bits_estimated_r1_temp = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                    for(scan_pos = 0; scan_pos < 16; scan_pos++)
                    {
                        UWORD8 y_pos_x_pos;

                        if(scan_pos)
                        {
                            y_pos_x_pos = scan_pos;  // gu1_hevce_scan4x4[i4_scan_idx][scan_pos];
                            /* ctxt for AC coeff depends on curpos and neigbour csbf */
                            sig_ctxinc = pu1_hevce_sigcoeff_ctxtinc[y_pos_x_pos];

                            /* based on luma subblock pos */
                            sig_ctxinc += (i && is_luma) ? 3 : 0;

                            sig_ctxinc += sig_coeff_base_ctxt;
                        }
                        else
                        {
                            /*MAM : both scan pos and i 0 impies the DC coef of 1st block only */
                            /* DC coeff has fixed context for luma and chroma */
                            sig_ctxinc = is_luma ? IHEVC_CAB_COEFF_FLAG : IHEVC_CAB_COEFF_FLAG + 27;
                        }

                        /*Get the mps state based on ctxt modes */
                        state_mps = pu1_ctxt_model[sig_ctxinc];

                        /* Bits taken to encode sig co-ef flag as 0 */
                        u4_bits_estimated_r0 = gau2_ihevce_cabac_bin_to_bits[state_mps ^ 0];

                        u4_bits_estimated_r1 =
                            (gau2_ihevce_cabac_bin_to_bits[state_mps ^ 1] + ROUND_Q12(1.000000000));

                        /* Bits taken to encode sig co-ef flag as 1, also account for sign bit worst case */
                        u4_bits_estimated_r1 += u4_bits_estimated_r1_temp;
                        {
                            QUANT_ROUND_FACTOR(
                                temp2, u4_bits_estimated_r1, u4_bits_estimated_r0, lamda_mod);
                            *(pi4_quant_round_0_1 +
                              ((scan_pos % 4) + ((scan_pos >> 2) * i4_trans_size))) = temp2;
                        }
                    }
                }
            }
        }
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_t_q_iq_ssd_scan_fxn \endif
*
* \brief
*    Transform unit level (Luma) enc_loop function
*
* \param[in] ps_ctxt    enc_loop module ctxt pointer
* \param[in] pu1_pred   pointer to predicted data buffer
* \param[in] pred_strd  predicted buffer stride
* \param[in] pu1_src    pointer to source data buffer
* \param[in] src_strd   source buffer stride
* \param[in] pi2_deq_data   pointer to store iq data
* \param[in] deq_data_strd  iq data buffer stride
* \param[out] pu1_ecd_data pointer coeff output buffer (input to ent cod)
* \param[out] pu1_csbf_buf  pointer to store the csbf for all 4x4 in a current
*                           block
* \param[out] csbf_strd  csbf buffer stride
* \param[in] trans_size transform size (4, 8, 16,32)
* \param[in] packed_pred_mode   0:Inter 1:Intra 2:Skip
* \param[out] pi4_cost      pointer to store the cost
* \param[out] pi4_coeff_off pointer to store the number of bytes produced in
*                           coeff buffer
* \param[out] pu4_tu_bits   pointer to store the best TU bits required encode
the current TU in RDopt Mode
* \param[out] pu4_blk_sad   pointer to store the block sad for RC
* \param[out] pi4_zero_col  pointer to store the zero_col info for the TU
* \param[out] pi4_zero_row  pointer to store the zero_row info for the TU
* \param[in]  i4_perform_rdoq Indicates if RDOQ should be performed or not
* \param[in]  i4_perform_sbh Indicates if SBH should be performed or not
*
* \return
*    CBF of the current block
*
* \author
*  Ittiam
*
*****************************************************************************
*/

WORD32 ihevce_t_q_iq_ssd_scan_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD16 *pi2_deq_data,
    WORD32 deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 csbf_strd,
    WORD32 trans_size,
    WORD32 packed_pred_mode,
    LWORD64 *pi8_cost,
    WORD32 *pi4_coeff_off,
    WORD32 *pi4_tu_bits,
    UWORD32 *pu4_blk_sad,
    WORD32 *pi4_zero_col,
    WORD32 *pi4_zero_row,
    UWORD8 *pu1_is_recon_available,
    WORD32 i4_perform_rdoq,
    WORD32 i4_perform_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    SSD_TYPE_T e_ssd_type,
    WORD32 early_cbf)
{
    WORD32 cbf = 0;
    WORD32 trans_idx;
    WORD32 quant_scale_mat_offset;
    WORD32 *pi4_trans_scratch;
    WORD16 *pi2_trans_values;
    WORD16 *pi2_quant_coeffs;
    WORD32 *pi4_subBlock2csbfId_map = NULL;

#if PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
    WORD32 ai4_quant_rounding_factors[3][MAX_TU_SIZE * MAX_TU_SIZE], i;
#endif

    rdoq_sbh_ctxt_t *ps_rdoq_sbh_ctxt = &ps_ctxt->s_rdoq_sbh_ctxt;

    WORD32 i4_perform_zcbf = (ENABLE_INTER_ZCU_COST && (PRED_MODE_INTRA != packed_pred_mode)) ||
                             (ps_ctxt->i4_zcbf_rdo_level == ZCBF_ENABLE);
    WORD32 i4_perform_coeff_level_rdoq = (ps_ctxt->i4_quant_rounding_level != FIXED_QUANT_ROUNDING);
    WORD8 intra_flag = 0;
    ASSERT(csbf_strd == MAX_TU_IN_CTB_ROW);

    *pi4_tu_bits = 0;
    *pi4_coeff_off = 0;
    pu1_is_recon_available[0] = 0;

    if((PRED_MODE_SKIP == packed_pred_mode) || (0 == early_cbf))
    {
        if(e_ssd_type != NULL_TYPE)
        {
            /* SSD cost is stored to the pointer */
            pi8_cost[0] =

                ps_ctxt->s_cmn_opt_func.pf_ssd_and_sad_calculator(
                    pu1_pred, pred_strd, pu1_src, src_strd, trans_size, pu4_blk_sad);

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                pi8_cost[0] = ihevce_inject_stim_into_distortion(
                    pu1_src,
                    src_strd,
                    pu1_pred,
                    pred_strd,
                    pi8_cost[0],
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                                 100.0,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
            }
#endif

            /* copy pred to recon for skip mode */
            if(SPATIAL_DOMAIN_SSD == e_ssd_type)
            {
                ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                    pu1_recon, i4_recon_stride, pu1_pred, pred_strd, trans_size, trans_size);
                pu1_is_recon_available[0] = 1;
            }
            else
            {
                pu1_is_recon_available[0] = 0;
            }

#if ENABLE_INTER_ZCU_COST
            ps_ctxt->i8_cu_not_coded_cost += pi8_cost[0];
#endif
        }
        else
        {
            pi8_cost[0] = UINT_MAX;
        }

        /* cbf is returned as 0 */
        return (0);
    }

    /* derive context variables */
    pi4_trans_scratch = (WORD32 *)&ps_ctxt->ai2_scratch[0];
    pi2_quant_coeffs = &ps_ctxt->ai2_scratch[0];
    pi2_trans_values = &ps_ctxt->ai2_scratch[0] + (MAX_TRANS_SIZE * 2);

    /* translate the transform size to index for 4x4 and 8x8 */
    trans_idx = trans_size >> 2;

    if(PRED_MODE_INTRA == packed_pred_mode)
    {
        quant_scale_mat_offset = 0;
        intra_flag = 1;
#if PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
        ai4_quant_rounding_factors[0][0] =
            MAX(ps_ctxt->i4_quant_rnd_factor[intra_flag], (1 << QUANT_ROUND_FACTOR_Q) / 3);

        for(i = 0; i < trans_size * trans_size; i++)
        {
            ai4_quant_rounding_factors[1][i] =
                MAX(ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3][i],
                    (1 << QUANT_ROUND_FACTOR_Q) / 3);
            ai4_quant_rounding_factors[2][i] =
                MAX(ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3][i],
                    (1 << QUANT_ROUND_FACTOR_Q) / 3);
        }
#endif
    }
    else
    {
        quant_scale_mat_offset = NUM_TRANS_TYPES;
    }
    /* for intra 4x4 DST transform should be used */
    if((1 == trans_idx) && (1 == intra_flag))
    {
        trans_idx = 0;
    }
    /* for 16x16 cases */
    else if(16 == trans_size)
    {
        trans_idx = 3;
    }
    /* for 32x32 cases */
    else if(32 == trans_size)
    {
        trans_idx = 4;
    }

    switch(trans_size)
    {
    case 4:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map4x4TU;

        break;
    }
    case 8:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map8x8TU;

        break;
    }
    case 16:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map16x16TU;

        break;
    }
    case 32:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map32x32TU;

        break;
    }
    }

    /* Do not call the FT and Quant functions if early_cbf is 0 */
    if(1 == early_cbf)
    {
        /* ---------- call residue and transform block ------- */
        *pu4_blk_sad = ps_ctxt->apf_resd_trns[trans_idx](
            pu1_src,
            pu1_pred,
            pi4_trans_scratch,
            pi2_trans_values,
            src_strd,
            pred_strd,
            trans_size,
            NULL_PLANE);

        cbf = ps_ctxt->apf_quant_iquant_ssd
                  [i4_perform_coeff_level_rdoq + (e_ssd_type != FREQUENCY_DOMAIN_SSD) * 2](
                      pi2_trans_values,
                      ps_ctxt->api2_rescal_mat[trans_idx + quant_scale_mat_offset],
                      pi2_quant_coeffs,
                      pi2_deq_data,
                      trans_size,
                      ps_ctxt->i4_cu_qp_div6,
                      ps_ctxt->i4_cu_qp_mod6,
#if !PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
                      ps_ctxt->i4_quant_rnd_factor[intra_flag],
                      ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                      ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
#else
                      intra_flag ? ai4_quant_rounding_factors[0][0]
                                 : ps_ctxt->i4_quant_rnd_factor[intra_flag],
                      intra_flag ? ai4_quant_rounding_factors[1]
                                 : ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                      intra_flag ? ai4_quant_rounding_factors[2]
                                 : ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
#endif
                      trans_size,
                      trans_size,
                      deq_data_strd,
                      pu1_csbf_buf,
                      csbf_strd,
                      pi4_zero_col,
                      pi4_zero_row,
                      ps_ctxt->api2_scal_mat[trans_idx + quant_scale_mat_offset],
                      pi8_cost);

        if(e_ssd_type != FREQUENCY_DOMAIN_SSD)
        {
            pi8_cost[0] = UINT_MAX;
        }
    }

    if(0 != cbf)
    {
        if(i4_perform_sbh || i4_perform_rdoq)
        {
            ps_rdoq_sbh_ctxt->i4_iq_data_strd = deq_data_strd;
            ps_rdoq_sbh_ctxt->i4_q_data_strd = trans_size;
            ps_rdoq_sbh_ctxt->pi4_subBlock2csbfId_map = pi4_subBlock2csbfId_map;

            ps_rdoq_sbh_ctxt->i4_qp_div = ps_ctxt->i4_cu_qp_div6;
            ps_rdoq_sbh_ctxt->i2_qp_rem = ps_ctxt->i4_cu_qp_mod6;
            ps_rdoq_sbh_ctxt->i4_scan_idx = ps_ctxt->i4_scan_idx;
            ps_rdoq_sbh_ctxt->i8_ssd_cost = *pi8_cost;
            ps_rdoq_sbh_ctxt->i4_trans_size = trans_size;

            ps_rdoq_sbh_ctxt->pi2_dequant_coeff =
                ps_ctxt->api2_scal_mat[trans_idx + quant_scale_mat_offset];
            ps_rdoq_sbh_ctxt->pi2_iquant_coeffs = pi2_deq_data;
            ps_rdoq_sbh_ctxt->pi2_quant_coeffs = pi2_quant_coeffs;
            ps_rdoq_sbh_ctxt->pi2_trans_values = pi2_trans_values;
            ps_rdoq_sbh_ctxt->pu1_csbf_buf = pu1_csbf_buf;

            /* ------- call coeffs scan function ------- */
            if((!i4_perform_rdoq))
            {
                ihevce_sign_data_hiding(ps_rdoq_sbh_ctxt);

                pi8_cost[0] = ps_rdoq_sbh_ctxt->i8_ssd_cost;
            }
        }

        *pi4_coeff_off = ps_ctxt->s_cmn_opt_func.pf_scan_coeffs(
            pi2_quant_coeffs,
            pi4_subBlock2csbfId_map,
            ps_ctxt->i4_scan_idx,
            trans_size,
            pu1_ecd_data,
            pu1_csbf_buf,
            csbf_strd);
    }
    *pi8_cost >>= ga_trans_shift[trans_idx];

#if RDOPT_ZERO_CBF_ENABLE
    /* compare null cbf cost with encode tu rd-cost */
    if(cbf != 0)
    {
        WORD32 tu_bits;
        LWORD64 tu_rd_cost;

        LWORD64 zero_cbf_cost = 0;

        /*Populating the feilds of rdoq_ctxt structure*/
        if(i4_perform_rdoq)
        {
            /* transform size to log2transform size */
            GETRANGE(ps_rdoq_sbh_ctxt->i4_log2_trans_size, trans_size);
            ps_rdoq_sbh_ctxt->i4_log2_trans_size -= 1;
            ps_rdoq_sbh_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->i8_cl_ssd_lambda_qf;
            ps_rdoq_sbh_ctxt->i4_is_luma = 1;
            ps_rdoq_sbh_ctxt->i4_shift_val_ssd_in_td = ga_trans_shift[trans_idx];
            ps_rdoq_sbh_ctxt->i4_round_val_ssd_in_td =
                (1 << ps_rdoq_sbh_ctxt->i4_shift_val_ssd_in_td) / 2;
            ps_rdoq_sbh_ctxt->i1_tu_is_coded = 0;
            ps_rdoq_sbh_ctxt->pi4_zero_col = pi4_zero_col;
            ps_rdoq_sbh_ctxt->pi4_zero_row = pi4_zero_row;
        }
        else if(i4_perform_zcbf)
        {
            zero_cbf_cost =

                ps_ctxt->s_cmn_opt_func.pf_ssd_calculator(
                    pu1_src, pu1_pred, src_strd, pred_strd, trans_size, trans_size, NULL_PLANE);
        }

        /************************************************************************/
        /* call the entropy rdo encode to get the bit estimate for current tu   */
        /* note that tu includes only residual coding bits and does not include */
        /* tu split, cbf and qp delta encoding bits for a TU                    */
        /************************************************************************/
        if(i4_perform_rdoq)
        {
            tu_bits = ihevce_entropy_rdo_encode_tu_rdoq(
                &ps_ctxt->s_rdopt_entropy_ctxt,
                (pu1_ecd_data),
                trans_size,
                1,
                ps_rdoq_sbh_ctxt,
                pi8_cost,
                &zero_cbf_cost,
                0);

            if(ps_rdoq_sbh_ctxt->i1_tu_is_coded == 0)
            {
                cbf = 0;
                *pi4_coeff_off = 0;
            }

            if((i4_perform_sbh) && (0 != cbf))
            {
                ps_rdoq_sbh_ctxt->i8_ssd_cost = *pi8_cost;
                ihevce_sign_data_hiding(ps_rdoq_sbh_ctxt);
                *pi8_cost = ps_rdoq_sbh_ctxt->i8_ssd_cost;
            }

            /*Add round value before normalizing*/
            *pi8_cost += ps_rdoq_sbh_ctxt->i4_round_val_ssd_in_td;
            *pi8_cost >>= ga_trans_shift[trans_idx];

            if(ps_rdoq_sbh_ctxt->i1_tu_is_coded == 1)
            {
                pi2_quant_coeffs = &ps_ctxt->ai2_scratch[0];
                *pi4_coeff_off = ps_ctxt->s_cmn_opt_func.pf_scan_coeffs(
                    pi2_quant_coeffs,
                    pi4_subBlock2csbfId_map,
                    ps_ctxt->i4_scan_idx,
                    trans_size,
                    pu1_ecd_data,
                    pu1_csbf_buf,
                    csbf_strd);
            }
        }
        else
        {
            tu_bits = ihevce_entropy_rdo_encode_tu(
                &ps_ctxt->s_rdopt_entropy_ctxt, pu1_ecd_data, trans_size, 1, i4_perform_sbh);
        }

        *pi4_tu_bits = tu_bits;

        if(e_ssd_type == SPATIAL_DOMAIN_SSD)
        {
            *pi8_cost = ihevce_it_recon_ssd(
                ps_ctxt,
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi2_deq_data,
                deq_data_strd,
                pu1_recon,
                i4_recon_stride,
                pu1_ecd_data,
                trans_size,
                packed_pred_mode,
                cbf,
                *pi4_zero_col,
                *pi4_zero_row,
                NULL_PLANE);

            pu1_is_recon_available[0] = 1;
        }

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        if(u1_is_cu_noisy && (e_ssd_type == SPATIAL_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_recon,
                i4_recon_stride,
                pi8_cost[0],
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                NULL_PLANE);
        }
        else if(u1_is_cu_noisy && (e_ssd_type == FREQUENCY_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi8_cost[0],
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                NULL_PLANE);
        }
#endif

        /* add the SSD cost to bits estimate given by ECD */
        tu_rd_cost = *pi8_cost + COMPUTE_RATE_COST_CLIP30(
                                     tu_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

        if(i4_perform_zcbf)
        {
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                zero_cbf_cost = ihevce_inject_stim_into_distortion(
                    pu1_src,
                    src_strd,
                    pu1_pred,
                    pred_strd,
                    zero_cbf_cost,
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                                 100.0,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
            }
#endif

            /* force the tu as zero cbf if zero_cbf_cost is lower */
            if(zero_cbf_cost < tu_rd_cost)
            {
                /* num bytes is set to 0 */
                *pi4_coeff_off = 0;

                /* cbf is returned as 0 */
                cbf = 0;

                /* cost is returned as 0 cbf cost */
                *pi8_cost = zero_cbf_cost;

                /* TU bits is set to 0 */
                *pi4_tu_bits = 0;
                pu1_is_recon_available[0] = 0;

                if(SPATIAL_DOMAIN_SSD == e_ssd_type)
                {
                    /* copy pred to recon for zcbf mode */

                    ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                        pu1_recon, i4_recon_stride, pu1_pred, pred_strd, trans_size, trans_size);

                    pu1_is_recon_available[0] = 1;
                }
            }
            /* accumulate cu not coded cost with zcbf cost */
#if ENABLE_INTER_ZCU_COST
            ps_ctxt->i8_cu_not_coded_cost += zero_cbf_cost;
#endif
        }
    }
    else
    {
        /* cbf = 0, accumulate cu not coded cost */
        if(e_ssd_type == SPATIAL_DOMAIN_SSD)
        {
            *pi8_cost = ihevce_it_recon_ssd(
                ps_ctxt,
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi2_deq_data,
                deq_data_strd,
                pu1_recon,
                i4_recon_stride,
                pu1_ecd_data,
                trans_size,
                packed_pred_mode,
                cbf,
                *pi4_zero_col,
                *pi4_zero_row,
                NULL_PLANE);

            pu1_is_recon_available[0] = 1;
        }

#if ENABLE_INTER_ZCU_COST
        {
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            if(u1_is_cu_noisy && (e_ssd_type == SPATIAL_DOMAIN_SSD) && i4_alpha_stim_multiplier)
            {
                pi8_cost[0] = ihevce_inject_stim_into_distortion(
                    pu1_src,
                    src_strd,
                    pu1_recon,
                    i4_recon_stride,
                    pi8_cost[0],
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                                 100.0,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
            }
            else if(u1_is_cu_noisy && (e_ssd_type == FREQUENCY_DOMAIN_SSD) && i4_alpha_stim_multiplier)
            {
                pi8_cost[0] = ihevce_inject_stim_into_distortion(
                    pu1_src,
                    src_strd,
                    pu1_pred,
                    pred_strd,
                    pi8_cost[0],
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                                 100.0,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
            }
#endif

            ps_ctxt->i8_cu_not_coded_cost += *pi8_cost;
        }
#endif /* ENABLE_INTER_ZCU_COST */
    }
#endif

    return (cbf);
}

/*!
******************************************************************************
* \if Function name : ihevce_it_recon_fxn \endif
*
* \brief
*    Transform unit level (Luma) IT Recon function
*
* \param[in] ps_ctxt        enc_loop module ctxt pointer
* \param[in] pi2_deq_data   pointer to iq data
* \param[in] deq_data_strd  iq data buffer stride
* \param[in] pu1_pred       pointer to predicted data buffer
* \param[in] pred_strd      predicted buffer stride
* \param[in] pu1_recon      pointer to recon buffer
* \param[in] recon_strd     recon buffer stride
* \param[out] pu1_ecd_data  pointer coeff output buffer (input to ent cod)
* \param[in] trans_size     transform size (4, 8, 16,32)
* \param[in] packed_pred_mode   0:Inter 1:Intra 2:Skip
* \param[in] cbf            CBF of the current block
* \param[in] zero_cols      zero_cols of the current block
* \param[in] zero_rows      zero_rows of the current block
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/

void ihevce_it_recon_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD16 *pi2_deq_data,
    WORD32 deq_dat_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_ecd_data,
    WORD32 trans_size,
    WORD32 packed_pred_mode,
    WORD32 cbf,
    WORD32 zero_cols,
    WORD32 zero_rows)
{
    WORD32 dc_add_flag = 0;
    WORD32 trans_idx;

    /* translate the transform size to index for 4x4 and 8x8 */
    trans_idx = trans_size >> 2;

    /* if SKIP mode needs to be evaluated the pred is copied to recon */
    if(PRED_MODE_SKIP == packed_pred_mode)
    {
        UWORD8 *pu1_curr_recon, *pu1_curr_pred;

        pu1_curr_pred = pu1_pred;
        pu1_curr_recon = pu1_recon;

        /* 2D copy of data */

        ps_ctxt->s_cmn_opt_func.pf_2d_square_copy(
            pu1_curr_recon, recon_strd, pu1_curr_pred, pred_strd, trans_size, sizeof(UWORD8));

        return;
    }

    /* for intra 4x4 DST transform should be used */
    if((1 == trans_idx) && (PRED_MODE_INTRA == packed_pred_mode))
    {
        trans_idx = 0;
    }
    /* for 16x16 cases */
    else if(16 == trans_size)
    {
        trans_idx = 3;
    }
    /* for 32x32 cases */
    else if(32 == trans_size)
    {
        trans_idx = 4;
    }

    /*if (lastx == 0 && lasty == 0) , ie only 1 coefficient */
    if((0 == pu1_ecd_data[0]) && (0 == pu1_ecd_data[1]))
    {
        dc_add_flag = 1;
    }

    if(0 == cbf)
    {
        /* buffer copy */
        ps_ctxt->s_cmn_opt_func.pf_2d_square_copy(
            pu1_recon, recon_strd, pu1_pred, pred_strd, trans_size, 1);
    }
    else if((1 == dc_add_flag) && (0 != trans_idx))
    {
        /* dc add */
        ps_ctxt->s_cmn_opt_func.pf_itrans_recon_dc(
            pu1_pred,
            pred_strd,
            pu1_recon,
            recon_strd,
            trans_size,
            pi2_deq_data[0],
            NULL_PLANE /* luma */
        );
    }
    else
    {
        ps_ctxt->apf_it_recon[trans_idx](
            pi2_deq_data,
            &ps_ctxt->ai2_scratch[0],
            pu1_pred,
            pu1_recon,
            deq_dat_strd,
            pred_strd,
            recon_strd,
            zero_cols,
            zero_rows);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_chroma_it_recon_fxn \endif
*
* \brief
*    Transform unit level (Chroma) IT Recon function
*
* \param[in] ps_ctxt        enc_loop module ctxt pointer
* \param[in] pi2_deq_data   pointer to iq data
* \param[in] deq_data_strd  iq data buffer stride
* \param[in] pu1_pred       pointer to predicted data buffer
* \param[in] pred_strd      predicted buffer stride
* \param[in] pu1_recon      pointer to recon buffer
* \param[in] recon_strd     recon buffer stride
* \param[out] pu1_ecd_data  pointer coeff output buffer (input to ent cod)
* \param[in] trans_size     transform size (4, 8, 16)
* \param[in] cbf            CBF of the current block
* \param[in] zero_cols      zero_cols of the current block
* \param[in] zero_rows      zero_rows of the current block
*
* \return
*
* \author
*  Ittiam
*
*****************************************************************************
*/

void ihevce_chroma_it_recon_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD16 *pi2_deq_data,
    WORD32 deq_dat_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_ecd_data,
    WORD32 trans_size,
    WORD32 cbf,
    WORD32 zero_cols,
    WORD32 zero_rows,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 trans_idx;

    ASSERT((e_chroma_plane == U_PLANE) || (e_chroma_plane == V_PLANE));

    /* since 2x2 transform is not allowed for chroma*/
    if(2 == trans_size)
    {
        trans_size = 4;
    }

    /* translate the transform size to index */
    trans_idx = trans_size >> 2;

    /* for 16x16 cases */
    if(16 == trans_size)
    {
        trans_idx = 3;
    }

    if(0 == cbf)
    {
        /* buffer copy */
        ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
            pu1_pred, pred_strd, pu1_recon, recon_strd, trans_size, trans_size, e_chroma_plane);
    }
    else if((0 == pu1_ecd_data[0]) && (0 == pu1_ecd_data[1]))
    {
        /* dc add */
        ps_ctxt->s_cmn_opt_func.pf_itrans_recon_dc(
            pu1_pred,
            pred_strd,
            pu1_recon,
            recon_strd,
            trans_size,
            pi2_deq_data[0],
            e_chroma_plane /* chroma plane */
        );
    }
    else
    {
        ps_ctxt->apf_chrm_it_recon[trans_idx - 1](
            pi2_deq_data,
            &ps_ctxt->ai2_scratch[0],
            pu1_pred + (WORD32)e_chroma_plane,
            pu1_recon + (WORD32)e_chroma_plane,
            deq_dat_strd,
            pred_strd,
            recon_strd,
            zero_cols,
            zero_rows);
    }
}

/**
*******************************************************************************
* \if Function name : ihevce_mpm_idx_based_filter_RDOPT_cand \endif
*
* \brief * Filters the RDOPT candidates based on mpm_idx
*
* \par   Description
* Computes the b1_prev_intra_luma_pred_flag, b2_mpm_idx & b5_rem_intra_pred_mode
* for a CU
*
* \param[in] ps_ctxt : ptr to enc loop context
* \param[in] ps_cu_analyse : ptr to CU analyse structure
* \param[in] ps_top_nbr_4x4 top 4x4 neighbour pointer
* \param[in] ps_left_nbr_4x4 left 4x4 neighbour pointer
* \param[in] pu1_luma_mode luma mode
*
* \returns none
*
* \author
*  Ittiam
*
*******************************************************************************
*/

void ihevce_mpm_idx_based_filter_RDOPT_cand(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_analyse_t *ps_cu_analyse,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    UWORD8 *pu1_luma_mode,
    UWORD8 *pu1_eval_mark)
{
    WORD32 cu_pos_x;
    WORD32 cu_pos_y;
    nbr_avail_flags_t s_nbr;
    WORD32 trans_size;
    WORD32 au4_cand_mode_list[3];
    WORD32 nbr_flags;
    UWORD8 *pu1_intra_luma_modes;
    WORD32 rdopt_cand_ctr = 0;
    UWORD8 *pu1_luma_eval_mark;

    cu_pos_x = ps_cu_analyse->b3_cu_pos_x << 1;
    cu_pos_y = ps_cu_analyse->b3_cu_pos_y << 1;
    trans_size = ps_cu_analyse->u1_cu_size;

    /* get the neighbour availability flags */
    nbr_flags = ihevce_get_nbr_intra(
        &s_nbr,
        ps_ctxt->pu1_ctb_nbr_map,
        ps_ctxt->i4_nbr_map_strd,
        cu_pos_x,
        cu_pos_y,
        trans_size >> 2);
    (void)nbr_flags;
    /*Call the fun to populate luma intra pred mode fro TU=CU and use the same list fro
    *TU=CU/2 also since the modes are same in both the cases.
    */
    ihevce_populate_intra_pred_mode(
        ps_top_nbr_4x4->b6_luma_intra_mode,
        ps_left_nbr_4x4->b6_luma_intra_mode,
        s_nbr.u1_top_avail,
        s_nbr.u1_left_avail,
        cu_pos_y,
        &au4_cand_mode_list[0]);

    /*Loop through all the RDOPT candidates of TU=CU and TU=CU/2 and check if the current RDOPT
    *cand is present in a4_cand_mode_list, If yes set eval flag to 1 else set it to zero
    */

    pu1_intra_luma_modes = pu1_luma_mode;
    pu1_luma_eval_mark = pu1_eval_mark;

    while(pu1_intra_luma_modes[rdopt_cand_ctr] != 255)
    {
        WORD32 i;
        WORD32 found_flag = 0;

        /*1st candidate of TU=CU list and TU=CU/2 list must go through RDOPT stage
        *irrespective of whether the cand is present in the mpm idx list or not
        */
        if(rdopt_cand_ctr == 0)
        {
            rdopt_cand_ctr++;
            continue;
        }

        for(i = 0; i < 3; i++)
        {
            if(pu1_intra_luma_modes[rdopt_cand_ctr] == au4_cand_mode_list[i])
            {
                found_flag = 1;
                break;
            }
        }

        if(found_flag == 0)
        {
            pu1_luma_eval_mark[rdopt_cand_ctr] = 0;
        }

        rdopt_cand_ctr++;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_intra_rdopt_cu_ntu \endif
*
* \brief
*    Intra Coding unit funtion for RD opt mode
*
* \param[in] ps_ctxt    enc_loop module ctxt pointer
* \param[in] ps_chrm_cu_buf_prms pointer to chroma buffer pointers structure
* \param[in] pu1_luma_mode : pointer to luma mode
* \param[in] ps_cu_analyse  pointer to cu analyse pointer
* \param[in] pu1_src    pointer to source data buffer
* \param[in] src_strd   source buffer stride
* \param[in] pu1_cu_left pointer to left recon data buffer
* \param[in] pu1_cu_top  pointer to top recon data buffer
* \param[in] pu1_cu_top_left pointer to top left recon data buffer
* \param[in] ps_left_nbr_4x4 : left 4x4 neighbour pointer
* \param[in] ps_top_nbr_4x4 : top 4x4 neighbour pointer
* \param[in] nbr_4x4_left_strd left nbr4x4 stride
* \param[in] cu_left_stride left recon buffer stride
* \param[in] curr_buf_idx RD opt buffer index for current usage
* \param[in] func_proc_mode : function procesing mode @sa TU_SIZE_WRT_CU_T
*
* \return
*    RDopt cost
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_intra_rdopt_cu_ntu(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_pred_org,
    WORD32 pred_strd_org,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    UWORD8 *pu1_luma_mode,
    cu_analyse_t *ps_cu_analyse,
    void *pv_curr_src,
    void *pv_cu_left,
    void *pv_cu_top,
    void *pv_cu_top_left,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 cu_left_stride,
    WORD32 curr_buf_idx,
    WORD32 func_proc_mode,
    WORD32 i4_alpha_stim_multiplier)
{
    enc_loop_cu_final_prms_t *ps_final_prms;
    nbr_avail_flags_t s_nbr;
    nbr_4x4_t *ps_nbr_4x4;
    nbr_4x4_t *ps_tmp_lt_4x4;
    recon_datastore_t *ps_recon_datastore;

    ihevc_intra_pred_luma_ref_substitution_ft *ihevc_intra_pred_luma_ref_substitution_fptr;

    UWORD32 *pu4_nbr_flags;
    UWORD8 *pu1_intra_pred_mode;
    WORD32 cu_pos_x;
    WORD32 cu_pos_y;
    WORD32 trans_size = 0;
    UWORD8 *pu1_left;
    UWORD8 *pu1_top;
    UWORD8 *pu1_top_left;
    UWORD8 *pu1_recon;
    UWORD8 *pu1_csbf_buf;
    UWORD8 *pu1_ecd_data;
    WORD16 *pi2_deq_data;
    WORD32 deq_data_strd;
    LWORD64 total_rdopt_cost;
    WORD32 ctr;
    WORD32 left_strd;
    WORD32 i4_recon_stride;
    WORD32 csbf_strd;
    WORD32 ecd_data_bytes_cons;
    WORD32 num_4x4_in_tu;
    WORD32 num_4x4_in_cu;
    WORD32 chrm_present_flag;
    WORD32 tx_size;
    WORD32 cu_bits;
    WORD32 num_cu_parts = 0;
    WORD32 num_cands = 0;
    WORD32 cu_pos_x_8pelunits;
    WORD32 cu_pos_y_8pelunits;
    WORD32 i4_perform_rdoq;
    WORD32 i4_perform_sbh;
    UWORD8 u1_compute_spatial_ssd;
    UWORD8 u1_compute_recon;
    UWORD8 au1_intra_nxn_rdopt_ctxt_models[2][IHEVC_CAB_CTXT_END];

    UWORD16 u2_num_tus_in_cu = 0;
    WORD32 is_sub_pu_in_hq = 0;
    /* Get the RDOPT cost of the best CU mode for early_exit */
    LWORD64 prev_best_rdopt_cost = ps_ctxt->as_cu_prms[!curr_buf_idx].i8_best_rdopt_cost;
    /* cabac context of prev intra luma pred flag */
    UWORD8 u1_prev_flag_cabac_ctxt =
        ps_ctxt->au1_rdopt_init_ctxt_models[IHEVC_CAB_INTRA_LUMA_PRED_FLAG];
    WORD32 src_strd = ps_cu_prms->i4_luma_src_stride;

    UWORD8 u1_is_cu_noisy = ps_cu_prms->u1_is_cu_noisy && !DISABLE_INTRA_WHEN_NOISY;

    total_rdopt_cost = 0;
    ps_final_prms = &ps_ctxt->as_cu_prms[curr_buf_idx];
    ps_recon_datastore = &ps_final_prms->s_recon_datastore;
    i4_recon_stride = ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;
    csbf_strd = ps_ctxt->i4_cu_csbf_strd;
    pu1_csbf_buf = &ps_ctxt->au1_cu_csbf[0];
    pu1_ecd_data = &ps_final_prms->pu1_cu_coeffs[0];
    pi2_deq_data = &ps_final_prms->pi2_cu_deq_coeffs[0];
    deq_data_strd = ps_cu_analyse->u1_cu_size; /* deq_data stride is cu size */
    ps_nbr_4x4 = &ps_ctxt->as_cu_nbr[curr_buf_idx][0];
    ps_tmp_lt_4x4 = ps_left_nbr_4x4;
    pu4_nbr_flags = &ps_final_prms->au4_nbr_flags[0];
    pu1_intra_pred_mode = &ps_final_prms->au1_intra_pred_mode[0];
    cu_pos_x = ps_cu_analyse->b3_cu_pos_x;
    cu_pos_y = ps_cu_analyse->b3_cu_pos_y;
    cu_pos_x_8pelunits = cu_pos_x;
    cu_pos_y_8pelunits = cu_pos_y;

    /* reset cu not coded cost */
    ps_ctxt->i8_cu_not_coded_cost = 0;

    /* based on the Processng mode */
    if(TU_EQ_CU == func_proc_mode)
    {
        ps_final_prms->u1_part_mode = SIZE_2Nx2N;
        trans_size = ps_cu_analyse->u1_cu_size;
        num_cu_parts = 1;
        num_cands = 1;
        u2_num_tus_in_cu = 1;
    }
    else if(TU_EQ_CU_DIV2 == func_proc_mode)
    {
        ps_final_prms->u1_part_mode = SIZE_2Nx2N;
        trans_size = ps_cu_analyse->u1_cu_size >> 1;
        num_cu_parts = 4;
        num_cands = 1;
        u2_num_tus_in_cu = 4;
    }
    else if(TU_EQ_SUBCU == func_proc_mode)
    {
        ps_final_prms->u1_part_mode = SIZE_NxN;
        trans_size = ps_cu_analyse->u1_cu_size >> 1;
        num_cu_parts = 4;
        /*In HQ for TU = SUBPU, all 35 modes used for RDOPT instead of 3 modes */
        if(IHEVCE_QUALITY_P3 > ps_ctxt->i4_quality_preset)
        {
            if(ps_ctxt->i1_slice_type != BSLICE)
            {
                num_cands = (4 * MAX_INTRA_CU_CANDIDATES) + 2;
            }
            else
            {
                num_cands = (2 * MAX_INTRA_CU_CANDIDATES);
            }
        }
        else
        {
            num_cands = MAX_INTRA_CU_CANDIDATES;
        }
        u2_num_tus_in_cu = 4;
    }
    else
    {
        /* should not enter here */
        ASSERT(0);
    }

    if(ps_ctxt->i1_cu_qp_delta_enable)
    {
        ihevce_update_cu_level_qp_lamda(ps_ctxt, ps_cu_analyse, trans_size, 1);
    }

    if(u1_is_cu_noisy && !ps_ctxt->u1_enable_psyRDOPT)
    {
        ps_ctxt->i8_cl_ssd_lambda_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_qf * (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) /
             100.0f);
        ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_chroma_qf *
             (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);
    }

    u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_QP_WHERE_SPATIAL_SSD_ENABLED) &&
                             (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
                             CONVERT_SSDS_TO_SPATIAL_DOMAIN;

    if(u1_is_cu_noisy || ps_ctxt->u1_enable_psyRDOPT)
    {
        u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_HEVC_QP) &&
                                 CONVERT_SSDS_TO_SPATIAL_DOMAIN;
    }

    /* populate the neigbours */
    pu1_left = (UWORD8 *)pv_cu_left;
    pu1_top = (UWORD8 *)pv_cu_top;
    pu1_top_left = (UWORD8 *)pv_cu_top_left;
    left_strd = cu_left_stride;
    num_4x4_in_tu = (trans_size >> 2);
    num_4x4_in_cu = (ps_cu_analyse->u1_cu_size >> 2);
    chrm_present_flag = 1;
    ecd_data_bytes_cons = 0;
    cu_bits = 0;

    /* get the 4x4 level postion of current cu */
    cu_pos_x = cu_pos_x << 1;
    cu_pos_y = cu_pos_y << 1;

    /* pouplate cu level params knowing that current is intra */
    ps_final_prms->u1_skip_flag = 0;
    ps_final_prms->u1_intra_flag = PRED_MODE_INTRA;
    ps_final_prms->u2_num_pus_in_cu = 1;
    /*init the is_cu_coded flag*/
    ps_final_prms->u1_is_cu_coded = 0;
    ps_final_prms->u4_cu_sad = 0;

    ps_final_prms->as_pu_enc_loop[0].b1_intra_flag = PRED_MODE_INTRA;
    ps_final_prms->as_pu_enc_loop[0].b4_wd = (trans_size >> 1) - 1;
    ps_final_prms->as_pu_enc_loop[0].b4_ht = (trans_size >> 1) - 1;
    ps_final_prms->as_pu_enc_loop[0].b4_pos_x = cu_pos_x;
    ps_final_prms->as_pu_enc_loop[0].b4_pos_y = cu_pos_y;
    ps_final_prms->as_pu_enc_loop[0].b1_merge_flag = 0;

    ps_final_prms->as_col_pu_enc_loop[0].b1_intra_flag = 1;

    /*copy qp directly as intra cant be skip*/
    ps_nbr_4x4->b8_qp = ps_ctxt->i4_cu_qp;
    ps_nbr_4x4->mv.s_l0_mv.i2_mvx = 0;
    ps_nbr_4x4->mv.s_l0_mv.i2_mvy = 0;
    ps_nbr_4x4->mv.s_l1_mv.i2_mvx = 0;
    ps_nbr_4x4->mv.s_l1_mv.i2_mvy = 0;
    ps_nbr_4x4->mv.i1_l0_ref_pic_buf_id = -1;
    ps_nbr_4x4->mv.i1_l1_ref_pic_buf_id = -1;
    ps_nbr_4x4->mv.i1_l0_ref_idx = -1;
    ps_nbr_4x4->mv.i1_l1_ref_idx = -1;

    /* RDOPT copy States :  TU init (best until prev TU) to current */
    memcpy(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
             .s_cabac_ctxt.au1_ctxt_models[0],
        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
        IHEVC_CAB_COEFFX_PREFIX);

    /* RDOPT copy States :update to init state if 0 cbf */
    memcpy(
        &au1_intra_nxn_rdopt_ctxt_models[0][0],
        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
        IHEVC_CAB_COEFFX_PREFIX);
    memcpy(
        &au1_intra_nxn_rdopt_ctxt_models[1][0],
        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
        IHEVC_CAB_COEFFX_PREFIX);

    /* loop for all partitions in CU  blocks */
    for(ctr = 0; ctr < num_cu_parts; ctr++)
    {
        UWORD8 *pu1_curr_mode;
        WORD32 cand_ctr;
        WORD32 nbr_flags;

        /* for NxN case to track the best mode       */
        /* for other cases zeroth index will be used */
        intra_prev_rem_flags_t as_intra_prev_rem[2];
        LWORD64 ai8_cand_rdopt_cost[2];
        UWORD32 au4_tu_sad[2];
        WORD32 ai4_tu_bits[2];
        WORD32 ai4_cbf[2];
        WORD32 ai4_curr_bytes[2];
        WORD32 ai4_zero_col[2];
        WORD32 ai4_zero_row[2];
        /* To store the pred, coeff and dequant for TU_EQ_SUBCU case (since mul.
        cand. are there) ping-pong buffer to store the best and current */
        UWORD8 au1_cur_pred_data[2][MIN_TU_SIZE * MIN_TU_SIZE];
        UWORD8 au1_intra_coeffs[2][MAX_SCAN_COEFFS_BYTES_4x4];
        WORD16 ai2_intra_deq_coeffs[2][MIN_TU_SIZE * MIN_TU_SIZE];
        /* Context models stored for RDopt store and restore purpose */

        UWORD8 au1_recon_availability[2];

        WORD32 best_cand_idx = 0;
        LWORD64 best_cand_cost = MAX_COST_64;
        /* counters to toggle b/w best and current */
        WORD32 best_intra_buf_idx = 1;
        WORD32 curr_intra_buf_idx = 0;

        /* copy the mode pointer to be used in inner loop */
        pu1_curr_mode = pu1_luma_mode;

        /* get the neighbour availability flags */
        nbr_flags = ihevce_get_nbr_intra(
            &s_nbr,
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            cu_pos_x,
            cu_pos_y,
            num_4x4_in_tu);

        /* copy the nbr flags for chroma reuse */
        if(4 != trans_size)
        {
            *pu4_nbr_flags = nbr_flags;
        }
        else if(1 == chrm_present_flag)
        {
            /* compute the avail flags assuming luma trans is 8x8 */
            /* get the neighbour availability flags */
            *pu4_nbr_flags = ihevce_get_nbr_intra_mxn_tu(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x,
                cu_pos_y,
                (num_4x4_in_tu << 1),
                (num_4x4_in_tu << 1));
        }

        u1_compute_recon = !u1_compute_spatial_ssd && ((num_cu_parts > 1) && (ctr < 3));

        if(!ctr && (u1_compute_spatial_ssd || u1_compute_recon))
        {
            ps_recon_datastore->u1_is_lumaRecon_available = 1;
        }
        else if(!ctr)
        {
            ps_recon_datastore->u1_is_lumaRecon_available = 0;
        }

        ihevc_intra_pred_luma_ref_substitution_fptr =
            ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_ref_substitution_fptr;

        /* call reference array substitution */
        ihevc_intra_pred_luma_ref_substitution_fptr(
            pu1_top_left,
            pu1_top,
            pu1_left,
            left_strd,
            trans_size,
            nbr_flags,
            (UWORD8 *)ps_ctxt->pv_ref_sub_out,
            1);

        /* Intra Mode gating based on MPM cand list and encoder quality preset */
        if((ps_ctxt->i1_slice_type != ISLICE) && (TU_EQ_SUBCU == func_proc_mode) &&
           (ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P3))
        {
            ihevce_mpm_idx_based_filter_RDOPT_cand(
                ps_ctxt,
                ps_cu_analyse,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                pu1_luma_mode,
                &ps_cu_analyse->s_cu_intra_cand.au1_nxn_eval_mark[ctr][0]);
        }

        if((TU_EQ_SUBCU == func_proc_mode) && (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
           (ps_cu_analyse->s_cu_intra_cand.au1_num_modes_added[ctr] >= MAX_INTRA_CU_CANDIDATES))
        {
            WORD32 ai4_mpm_mode_list[3];
            WORD32 i;

            WORD32 i4_curr_index = ps_cu_analyse->s_cu_intra_cand.au1_num_modes_added[ctr];

            ihevce_populate_intra_pred_mode(
                ps_top_nbr_4x4->b6_luma_intra_mode,
                ps_tmp_lt_4x4->b6_luma_intra_mode,
                s_nbr.u1_top_avail,
                s_nbr.u1_left_avail,
                cu_pos_y,
                &ai4_mpm_mode_list[0]);

            for(i = 0; i < 3; i++)
            {
                if(ps_cu_analyse->s_cu_intra_cand
                       .au1_intra_luma_mode_nxn_hash[ctr][ai4_mpm_mode_list[i]] == 0)
                {
                    ASSERT(ai4_mpm_mode_list[i] < 35);

                    ps_cu_analyse->s_cu_intra_cand
                        .au1_intra_luma_mode_nxn_hash[ctr][ai4_mpm_mode_list[i]] = 1;
                    pu1_luma_mode[i4_curr_index] = ai4_mpm_mode_list[i];
                    ps_cu_analyse->s_cu_intra_cand.au1_num_modes_added[ctr]++;
                    i4_curr_index++;
                }
            }

            pu1_luma_mode[i4_curr_index] = 255;
        }

        /* loop over candidates for each partition */
        for(cand_ctr = 0; cand_ctr < num_cands; cand_ctr++)
        {
            WORD32 curr_pred_mode;
            WORD32 bits = 0;
            LWORD64 curr_cost;
            WORD32 luma_pred_func_idx;
            UWORD8 *pu1_curr_ecd_data;
            WORD16 *pi2_curr_deq_data;
            WORD32 curr_deq_data_strd;
            WORD32 pred_strd;
            UWORD8 *pu1_pred;

            /* if NXN case the recon and ecd data is stored in temp buffers */
            if(TU_EQ_SUBCU == func_proc_mode)
            {
                pu1_pred = &au1_cur_pred_data[curr_intra_buf_idx][0];
                pred_strd = trans_size;
                pu1_curr_ecd_data = &au1_intra_coeffs[curr_intra_buf_idx][0];
                pi2_curr_deq_data = &ai2_intra_deq_coeffs[curr_intra_buf_idx][0];
                curr_deq_data_strd = trans_size;

                ASSERT(trans_size == MIN_TU_SIZE);
            }
            else
            {
                pu1_pred = (UWORD8 *)pv_pred_org;
                pred_strd = pred_strd_org;
                pu1_curr_ecd_data = pu1_ecd_data;
                pi2_curr_deq_data = pi2_deq_data;
                curr_deq_data_strd = deq_data_strd;
            }

            pu1_recon = ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs[curr_intra_buf_idx]) +
                        (ctr & 1) * trans_size + (ctr > 1) * trans_size * i4_recon_stride;

            if(is_sub_pu_in_hq == 1)
            {
                curr_pred_mode = cand_ctr;
            }
            else
            {
                curr_pred_mode = pu1_curr_mode[cand_ctr];
            }

            /* If the candidate mode is 255, then break */
            if(255 == curr_pred_mode)
            {
                break;
            }
            else if(250 == curr_pred_mode)
            {
                continue;
            }

            /* check if this mode needs to be evaluated or not. For 2nx2n cases, this   */
            /* function will be called once per candidate, so this check has been done  */
            /* outside this function call. For NxN case, this function will be called   */
            /* only once, and all the candidates will be evaluated here.                */
            if(ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P3)
            {
                if((TU_EQ_SUBCU == func_proc_mode) &&
                   (0 == ps_cu_analyse->s_cu_intra_cand.au1_nxn_eval_mark[ctr][cand_ctr]))
                {
                    continue;
                }
            }

            /* call reference filtering */
            ps_ctxt->ps_func_selector->ihevc_intra_pred_ref_filtering_fptr(
                (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                trans_size,
                (UWORD8 *)ps_ctxt->pv_ref_filt_out,
                curr_pred_mode,
                ps_ctxt->i1_strong_intra_smoothing_enable_flag);

            /* use the look up to get the function idx */
            luma_pred_func_idx = g_i4_ip_funcs[curr_pred_mode];

            /* call the intra prediction function */
            ps_ctxt->apf_lum_ip[luma_pred_func_idx](
                (UWORD8 *)ps_ctxt->pv_ref_filt_out,
                1,
                pu1_pred,
                pred_strd,
                trans_size,
                curr_pred_mode);

            /* populate the coeffs scan idx */
            ps_ctxt->i4_scan_idx = SCAN_DIAG_UPRIGHT;

            /* for luma 4x4 and 8x8 transforms based on intra pred mode scan is choosen*/
            if(trans_size < 16)
            {
                /* for modes from 22 upto 30 horizontal scan is used */
                if((curr_pred_mode > 21) && (curr_pred_mode < 31))
                {
                    ps_ctxt->i4_scan_idx = SCAN_HORZ;
                }
                /* for modes from 6 upto 14 horizontal scan is used */
                else if((curr_pred_mode > 5) && (curr_pred_mode < 15))
                {
                    ps_ctxt->i4_scan_idx = SCAN_VERT;
                }
            }

            /* RDOPT copy States :  TU init (best until prev TU) to current */
            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                        .s_cabac_ctxt.au1_ctxt_models[0] +
                    IHEVC_CAB_COEFFX_PREFIX,
                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

            i4_perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq;
            i4_perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh;

#if DISABLE_RDOQ_INTRA
            i4_perform_rdoq = 0;
#endif

            /*2 Multi- dimensinal array based on trans size  of rounding factor to be added here */
            /* arrays are for rounding factor corr. to 0-1 decision and 1-2 decision */
            /* Currently the complete array will contain only single value*/
            /*The rounding factor is calculated with the formula
            Deadzone val = (((R1 - R0) * (2^(-8/3)) * lamMod) + 1)/2
            rounding factor = (1 - DeadZone Val)

            Assumption: Cabac states of All the sub-blocks in the TU are considered independent
            */
            if((ps_ctxt->i4_quant_rounding_level != FIXED_QUANT_ROUNDING))
            {
                if((ps_ctxt->i4_quant_rounding_level == TU_LEVEL_QUANT_ROUNDING) && (ctr != 0))
                {
                    double i4_lamda_modifier;

                    if((BSLICE == ps_ctxt->i1_slice_type) && (ps_ctxt->i4_temporal_layer_id))
                    {
                        i4_lamda_modifier =
                            ps_ctxt->i4_lamda_modifier *
                            CLIP3((((double)(ps_ctxt->i4_cu_qp - 12)) / 6.0), 2.00, 4.00);
                    }
                    else
                    {
                        i4_lamda_modifier = ps_ctxt->i4_lamda_modifier;
                    }
                    if(ps_ctxt->i4_use_const_lamda_modifier)
                    {
                        if(ISLICE == ps_ctxt->i1_slice_type)
                        {
                            i4_lamda_modifier = ps_ctxt->f_i_pic_lamda_modifier;
                        }
                        else
                        {
                            i4_lamda_modifier = CONST_LAMDA_MOD_VAL;
                        }
                    }

                    ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3] =
                        &ps_ctxt->i4_quant_round_tu[0][0];
                    ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3] =
                        &ps_ctxt->i4_quant_round_tu[1][0];

                    memset(
                        ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                        0,
                        trans_size * trans_size * sizeof(WORD32));
                    memset(
                        ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
                        0,
                        trans_size * trans_size * sizeof(WORD32));

                    ihevce_quant_rounding_factor_gen(
                        trans_size,
                        1,
                        &ps_ctxt->s_rdopt_entropy_ctxt,
                        ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                        ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
                        i4_lamda_modifier,
                        1);
                }
                else
                {
                    ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3] =
                        ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[trans_size >> 3];
                    ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3] =
                        ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[trans_size >> 3];
                }
            }

            /* call T Q IT IQ and recon function */
            ai4_cbf[curr_intra_buf_idx] = ihevce_t_q_iq_ssd_scan_fxn(
                ps_ctxt,
                pu1_pred,
                pred_strd,
                (UWORD8 *)pv_curr_src,
                src_strd,
                pi2_curr_deq_data,
                curr_deq_data_strd,
                pu1_recon,
                i4_recon_stride,
                pu1_curr_ecd_data,
                pu1_csbf_buf,
                csbf_strd,
                trans_size,
                PRED_MODE_INTRA,
                &ai8_cand_rdopt_cost[curr_intra_buf_idx],
                &ai4_curr_bytes[curr_intra_buf_idx],
                &ai4_tu_bits[curr_intra_buf_idx],
                &au4_tu_sad[curr_intra_buf_idx],
                &ai4_zero_col[curr_intra_buf_idx],
                &ai4_zero_row[curr_intra_buf_idx],
                &au1_recon_availability[curr_intra_buf_idx],
                i4_perform_rdoq,
                i4_perform_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                i4_alpha_stim_multiplier,
                u1_is_cu_noisy,
#endif
                u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                1 /*early_cbf */
            );

#if COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL && !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
                ai8_cand_rdopt_cost[curr_intra_buf_idx] = ihevce_inject_stim_into_distortion(
                    pv_curr_src,
                    src_strd,
                    pu1_pred,
                    pred_strd,
                    ai8_cand_rdopt_cost[curr_intra_buf_idx],
                    i4_alpha_stim_multiplier,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
#else
                if(u1_compute_spatial_ssd && au1_recon_availability[curr_intra_buf_idx])
                {
                    ai8_cand_rdopt_cost[curr_intra_buf_idx] = ihevce_inject_stim_into_distortion(
                        pv_curr_src,
                        src_strd,
                        pu1_recon,
                        i4_recon_stride,
                        ai8_cand_rdopt_cost[curr_intra_buf_idx],
                        i4_alpha_stim_multiplier,
                        trans_size,
                        0,
                        ps_ctxt->u1_enable_psyRDOPT,
                        NULL_PLANE);
                }
                else
                {
                    ai8_cand_rdopt_cost[curr_intra_buf_idx] = ihevce_inject_stim_into_distortion(
                        pv_curr_src,
                        src_strd,
                        pu1_pred,
                        pred_strd,
                        ai8_cand_rdopt_cost[curr_intra_buf_idx],
                        i4_alpha_stim_multiplier,
                        trans_size,
                        0,
                        ps_ctxt->u1_enable_psyRDOPT,
                        NULL_PLANE);
                }
#endif
            }
#endif

            if(TU_EQ_SUBCU == func_proc_mode)
            {
                ASSERT(ai4_curr_bytes[curr_intra_buf_idx] < MAX_SCAN_COEFFS_BYTES_4x4);
            }

            /* based on CBF/No CBF copy the corresponding state */
            if(0 == ai4_cbf[curr_intra_buf_idx])
            {
                /* RDOPT copy States :update to init state if 0 cbf */
                COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                    &au1_intra_nxn_rdopt_ctxt_models[curr_intra_buf_idx][0] +
                        IHEVC_CAB_COEFFX_PREFIX,
                    &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                    IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
            }
            else
            {
                /* RDOPT copy States :update to new state only if CBF is non zero */
                COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                    &au1_intra_nxn_rdopt_ctxt_models[curr_intra_buf_idx][0] +
                        IHEVC_CAB_COEFFX_PREFIX,
                    &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                            .s_cabac_ctxt.au1_ctxt_models[0] +
                        IHEVC_CAB_COEFFX_PREFIX,
                    IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
            }

            /* call the function which perform intra mode prediction */
            ihevce_intra_pred_mode_signaling(
                ps_top_nbr_4x4->b6_luma_intra_mode,
                ps_tmp_lt_4x4->b6_luma_intra_mode,
                s_nbr.u1_top_avail,
                s_nbr.u1_left_avail,
                cu_pos_y,
                curr_pred_mode,
                &as_intra_prev_rem[curr_intra_buf_idx]);
            /******************************************************************/
            /* PREV INTRA LUMA FLAG, MPM MODE and REM INTRA MODE bits for I_NxN
            The bits for these are evaluated for every RDO mode of current subcu
            as they can significantly contribute to RDO cost.  Note that these
            bits are not accounted for here (ai8_cand_rdopt_cost) as they
            are accounted for in encode_cu call later */

            /******************************************************************/
            /* PREV INTRA LUMA FLAG, MPM MODE and REM INTRA MODE bits for I_NxN
            The bits for these are evaluated for every RDO mode of current subcu
            as they can significantly contribute to RDO cost.  Note that these
            bits are not accounted for here (ai8_cand_rdopt_cost) as they
            are accounted for in encode_cu call later */

            /* Estimate bits to encode prev rem flag  for NXN mode */
            {
                WORD32 bits_frac = gau2_ihevce_cabac_bin_to_bits
                    [u1_prev_flag_cabac_ctxt ^
                     as_intra_prev_rem[curr_intra_buf_idx].b1_prev_intra_luma_pred_flag];

                /* rounding the fractional bits to nearest integer */
                bits = ((bits_frac + (1 << (CABAC_FRAC_BITS_Q - 1))) >> CABAC_FRAC_BITS_Q);
            }

            /* based on prev flag all the mpmidx bits and rem bits */
            if(1 == as_intra_prev_rem[curr_intra_buf_idx].b1_prev_intra_luma_pred_flag)
            {
                /* mpm_idx */
                bits += as_intra_prev_rem[curr_intra_buf_idx].b2_mpm_idx ? 2 : 1;
            }
            else
            {
                /* rem intra mode */
                bits += 5;
            }

            bits += ai4_tu_bits[curr_intra_buf_idx];

            /* compute the total cost for current candidate */
            curr_cost = ai8_cand_rdopt_cost[curr_intra_buf_idx];

            /* get the final ssd cost */
            curr_cost +=
                COMPUTE_RATE_COST_CLIP30(bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

            /* check of the best candidate cost */
            if(curr_cost < best_cand_cost)
            {
                best_cand_cost = curr_cost;
                best_cand_idx = cand_ctr;
                best_intra_buf_idx = curr_intra_buf_idx;
                curr_intra_buf_idx = !curr_intra_buf_idx;
            }
        }

        /***************    For TU_EQ_SUBCU case    *****************/
        /* Copy the pred for best cand. to the final pred array     */
        /* Copy the iq-coeff for best cand. to the final array      */
        /* copy the best coeffs data to final buffer                */
        if(TU_EQ_SUBCU == func_proc_mode)
        {
            /* Copy the pred for best cand. to the final pred array */

            ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                (UWORD8 *)pv_pred_org,
                pred_strd_org,
                &au1_cur_pred_data[best_intra_buf_idx][0],
                trans_size,
                trans_size,
                trans_size);

            /* Copy the deq-coeff for best cand. to the final array */

            ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                (UWORD8 *)pi2_deq_data,
                deq_data_strd << 1,
                (UWORD8 *)&ai2_intra_deq_coeffs[best_intra_buf_idx][0],
                trans_size << 1,
                trans_size << 1,
                trans_size);
            /* copy the coeffs to final cu ecd bytes buffer */
            memcpy(
                pu1_ecd_data,
                &au1_intra_coeffs[best_intra_buf_idx][0],
                ai4_curr_bytes[best_intra_buf_idx]);

            pu1_recon = ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs[best_intra_buf_idx]) +
                        (ctr & 1) * trans_size + (ctr > 1) * trans_size * i4_recon_stride;
        }

        /*----------   Calculate Recon for the best INTRA mode     ---------*/
        /* TU_EQ_CU case : No need for recon, otherwise recon is required   */
        /* Compute recon only for the best mode for TU_EQ_SUBCU case        */
        if(u1_compute_recon)
        {
            ihevce_it_recon_fxn(
                ps_ctxt,
                pi2_deq_data,
                deq_data_strd,
                (UWORD8 *)pv_pred_org,
                pred_strd_org,
                pu1_recon,
                i4_recon_stride,
                pu1_ecd_data,
                trans_size,
                PRED_MODE_INTRA,
                ai4_cbf[best_intra_buf_idx],
                ai4_zero_col[best_intra_buf_idx],
                ai4_zero_row[best_intra_buf_idx]);

            ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr] = best_intra_buf_idx;
        }
        else if(u1_compute_spatial_ssd && au1_recon_availability[best_intra_buf_idx])
        {
            ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr] = best_intra_buf_idx;
        }
        else
        {
            ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr] = UCHAR_MAX;
        }

        /* RDOPT copy States :update to best modes state */
        COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
            &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
            &au1_intra_nxn_rdopt_ctxt_models[best_intra_buf_idx][0] + IHEVC_CAB_COEFFX_PREFIX,
            IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

        /* copy the prev,mpm_idx and rem modes from best cand */
        ps_final_prms->as_intra_prev_rem[ctr] = as_intra_prev_rem[best_intra_buf_idx];

        /* update the cabac context of prev intra pred mode flag */
        u1_prev_flag_cabac_ctxt = gau1_ihevc_next_state
            [(u1_prev_flag_cabac_ctxt << 1) |
             as_intra_prev_rem[best_intra_buf_idx].b1_prev_intra_luma_pred_flag];

        /* accumulate the TU bits into cu bits */
        cu_bits += ai4_tu_bits[best_intra_buf_idx];

        /* copy the intra pred mode for chroma reuse */
        if(is_sub_pu_in_hq == 0)
        {
            *pu1_intra_pred_mode = pu1_curr_mode[best_cand_idx];
        }
        else
        {
            *pu1_intra_pred_mode = best_cand_idx;
        }

        /* Store luma mode as chroma mode. If chroma prcs happens, and
        if a diff. mode wins, it should update this!! */
        if(1 == chrm_present_flag)
        {
            if(is_sub_pu_in_hq == 0)
            {
                ps_final_prms->u1_chroma_intra_pred_actual_mode =
                    ((ps_ctxt->u1_chroma_array_type == 2)
                         ? gau1_chroma422_intra_angle_mapping[pu1_curr_mode[best_cand_idx]]
                         : pu1_curr_mode[best_cand_idx]);
            }
            else
            {
                ps_final_prms->u1_chroma_intra_pred_actual_mode =
                    ((ps_ctxt->u1_chroma_array_type == 2)
                         ? gau1_chroma422_intra_angle_mapping[best_cand_idx]
                         : best_cand_idx);
            }

            ps_final_prms->u1_chroma_intra_pred_mode = 4;
        }

        /*remember the cbf flag to replicate qp for 4x4 neighbour*/
        ps_final_prms->u1_is_cu_coded |= ai4_cbf[best_intra_buf_idx];

        /*accumulate ssd over all TU of intra CU*/
        ps_final_prms->u4_cu_sad += au4_tu_sad[best_intra_buf_idx];

        /* update the bytes */
        ps_final_prms->as_tu_enc_loop[ctr].i4_luma_coeff_offset = ecd_data_bytes_cons;
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].i2_luma_bytes_consumed =
            ai4_curr_bytes[best_intra_buf_idx];
        /* update the zero_row and col info for the final mode */
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].u4_luma_zero_col =
            ai4_zero_col[best_intra_buf_idx];
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].u4_luma_zero_row =
            ai4_zero_row[best_intra_buf_idx];

        ps_final_prms->as_tu_enc_loop[ctr].i4_luma_coeff_offset = ecd_data_bytes_cons;

        /* update the total bytes cons */
        ecd_data_bytes_cons += ai4_curr_bytes[best_intra_buf_idx];
        pu1_ecd_data += ai4_curr_bytes[best_intra_buf_idx];

        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_y_cbf = ai4_cbf[best_intra_buf_idx];
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_chroma_intra_mode_idx = chrm_present_flag;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b7_qp = ps_ctxt->i4_cu_qp;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_first_tu_in_cu = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_transquant_bypass = 0;
        GETRANGE(tx_size, trans_size);
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_size = tx_size - 3;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_x = cu_pos_x;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_y = cu_pos_y;

        /* repiclate the nbr 4x4 structure for all 4x4 blocks current TU */
        ps_nbr_4x4->b1_skip_flag = 0;
        ps_nbr_4x4->b1_intra_flag = 1;
        ps_nbr_4x4->b1_pred_l0_flag = 0;
        ps_nbr_4x4->b1_pred_l1_flag = 0;

        if(is_sub_pu_in_hq == 0)
        {
            ps_nbr_4x4->b6_luma_intra_mode = pu1_curr_mode[best_cand_idx];
        }
        else
        {
            ps_nbr_4x4->b6_luma_intra_mode = best_cand_idx;
        }

        ps_nbr_4x4->b1_y_cbf = ai4_cbf[best_intra_buf_idx];

        /* since tu size can be less than cusize, replication is done with strd */
        {
            WORD32 i, j;
            nbr_4x4_t *ps_tmp_4x4;

            ps_tmp_4x4 = ps_nbr_4x4;

            for(i = 0; i < num_4x4_in_tu; i++)
            {
                for(j = 0; j < num_4x4_in_tu; j++)
                {
                    ps_tmp_4x4[j] = *ps_nbr_4x4;
                }
                /* row level update*/
                ps_tmp_4x4 += num_4x4_in_cu;
            }
        }

        if(TU_EQ_SUBCU == func_proc_mode)
        {
            pu1_luma_mode += ((MAX_INTRA_CU_CANDIDATES * 4) + 2 + 1);
        }

        if((num_cu_parts > 1) && (ctr < 3))
        {
            /* set the neighbour map to 1 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x,
                cu_pos_y,
                trans_size >> 2,
                1);

            /* block level updates block number (1 & 3 )*/
            pv_curr_src = (UWORD8 *)pv_curr_src + trans_size;
            pv_pred_org = (UWORD8 *)pv_pred_org + trans_size;
            pi2_deq_data += trans_size;

            switch(ctr)
            {
            case 0:
            {
                pu1_left = pu1_recon + trans_size - 1;
                pu1_top += trans_size;
                pu1_top_left = pu1_top - 1;
                left_strd = i4_recon_stride;

                break;
            }
            case 1:
            {
                ASSERT(
                    (ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0] == 0) ||
                    (ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0] == 1));

                /* Since the 'lumaRefSubstitution' function expects both Top and */
                /* TopRight recon pixels to be present in the same buffer */
                if(ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0] !=
                   ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1])
                {
                    UWORD8 *pu1_src =
                        ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs
                             [ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1]]) +
                        trans_size;
                    UWORD8 *pu1_dst =
                        ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs
                             [ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0]]) +
                        trans_size;

                    ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                        pu1_dst, i4_recon_stride, pu1_src, i4_recon_stride, trans_size, trans_size);

                    ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1] =
                        ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0];
                }

                pu1_left = (UWORD8 *)pv_cu_left + trans_size * cu_left_stride;
                pu1_top = ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs
                               [ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0]]) +
                          (trans_size - 1) * i4_recon_stride;
                pu1_top_left = pu1_left - cu_left_stride;
                left_strd = cu_left_stride;

                break;
            }
            case 2:
            {
                ASSERT(
                    (ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1] == 0) ||
                    (ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1] == 1));

                pu1_left = pu1_recon + trans_size - 1;
                pu1_top = ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs
                               [ps_recon_datastore->au1_bufId_with_winning_LumaRecon[1]]) +
                          (trans_size - 1) * i4_recon_stride + trans_size;
                pu1_top_left = pu1_top - 1;
                left_strd = i4_recon_stride;

                break;
            }
            }

            pu1_csbf_buf += num_4x4_in_tu;
            cu_pos_x += num_4x4_in_tu;
            ps_nbr_4x4 += num_4x4_in_tu;
            ps_top_nbr_4x4 += num_4x4_in_tu;
            ps_tmp_lt_4x4 = ps_nbr_4x4 - 1;

            pu1_intra_pred_mode++;

            /* after 2 blocks increment the pointers to bottom blocks */
            if(1 == ctr)
            {
                pv_curr_src = (UWORD8 *)pv_curr_src - (trans_size << 1);
                pv_curr_src = (UWORD8 *)pv_curr_src + (trans_size * src_strd);

                pv_pred_org = (UWORD8 *)pv_pred_org - (trans_size << 1);
                pv_pred_org = (UWORD8 *)pv_pred_org + (trans_size * pred_strd_org);
                pi2_deq_data -= (trans_size << 1);
                pi2_deq_data += (trans_size * deq_data_strd);

                pu1_csbf_buf -= (num_4x4_in_tu << 1);
                pu1_csbf_buf += (num_4x4_in_tu * csbf_strd);

                ps_nbr_4x4 -= (num_4x4_in_tu << 1);
                ps_nbr_4x4 += (num_4x4_in_tu * num_4x4_in_cu);
                ps_top_nbr_4x4 = ps_nbr_4x4 - num_4x4_in_cu;
                ps_tmp_lt_4x4 = ps_left_nbr_4x4 + (num_4x4_in_tu * nbr_4x4_left_strd);

                /* decrement pos x to start */
                cu_pos_x -= (num_4x4_in_tu << 1);
                cu_pos_y += num_4x4_in_tu;
            }
        }

#if RDOPT_ENABLE
        /* compute the RDOPT cost for the current TU */
        ai8_cand_rdopt_cost[best_intra_buf_idx] += COMPUTE_RATE_COST_CLIP30(
            ai4_tu_bits[best_intra_buf_idx], ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);
#endif

        /* accumulate the costs */
        total_rdopt_cost += ai8_cand_rdopt_cost[best_intra_buf_idx];

        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
        {
            /* Early exit : If the current running cost exceeds
            the prev. best mode cost, break */
            if(total_rdopt_cost > prev_best_rdopt_cost)
            {
                return (total_rdopt_cost);
            }
        }

        /* if transfrom size is 4x4 then only first luma 4x4 will have chroma*/
        chrm_present_flag = (4 != trans_size) ? 1 : INTRA_PRED_CHROMA_IDX_NONE;

        pu4_nbr_flags++;
    }
    /* Modify the cost function for this CU. */
    /* loop in for 8x8 blocks */
    if(ps_ctxt->u1_enable_psyRDOPT)
    {
        UWORD8 *pu1_recon_cu;
        WORD32 recon_stride;
        WORD32 curr_pos_x;
        WORD32 curr_pos_y;
        WORD32 start_index;
        WORD32 num_horz_cu_in_ctb;
        WORD32 cu_size;
        WORD32 had_block_size;

        /* tODO: sreenivasa ctb size has to be used appropriately */
        had_block_size = 8;
        cu_size = ps_cu_analyse->u1_cu_size; /* todo */
        num_horz_cu_in_ctb = 64 / had_block_size;

        curr_pos_x = ps_cu_analyse->b3_cu_pos_x << 3; /* pel units */
        curr_pos_y = ps_cu_analyse->b3_cu_pos_y << 3; /* pel units */
        recon_stride = ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;
        pu1_recon_cu =
            ((UWORD8 *)ps_final_prms->s_recon_datastore
                 .apv_luma_recon_bufs[ps_recon_datastore->au1_bufId_with_winning_LumaRecon[0]]);
        /* + \  curr_pos_x + curr_pos_y * recon_stride; */

        /* start index to index the source satd of curr cu int he current ctb*/
        start_index =
            (curr_pos_x / had_block_size) + (curr_pos_y / had_block_size) * num_horz_cu_in_ctb;

        {
            total_rdopt_cost += ihevce_psy_rd_cost(
                ps_ctxt->ai4_source_satd_8x8,
                pu1_recon_cu,
                recon_stride,
                1,  //
                cu_size,
                0,  // pic type
                0,  //layer id
                ps_ctxt->i4_satd_lamda,  // lambda
                start_index,
                ps_ctxt->u1_is_input_data_hbd,
                ps_ctxt->u4_psy_strength,
                &ps_ctxt->s_cmn_opt_func

            );  // 8 bit
        }
    }

#if !FORCE_INTRA_TU_DEPTH_TO_0  //RATIONALISE_NUM_RDO_MODES_IN_PQ_AND_HQ
    if(TU_EQ_SUBCU == func_proc_mode)
    {
        UWORD8 au1_tu_eq_cu_div2_modes[4];
        UWORD8 au1_freq_of_mode[4];

        WORD32 i4_num_clusters = ihevce_find_num_clusters_of_identical_points_1D(
            ps_final_prms->au1_intra_pred_mode, au1_tu_eq_cu_div2_modes, au1_freq_of_mode, 4);

        if(1 == i4_num_clusters)
        {
            ps_final_prms->u2_num_pus_in_cu = 1;
            ps_final_prms->u1_part_mode = SIZE_2Nx2N;
        }
    }
#endif

    /* store the num TUs*/
    ps_final_prms->u2_num_tus_in_cu = u2_num_tus_in_cu;

    /* update the bytes consumed */
    ps_final_prms->i4_num_bytes_ecd_data = ecd_data_bytes_cons;

    /* store the current cu size to final prms */
    ps_final_prms->u1_cu_size = ps_cu_analyse->u1_cu_size;

    /* cu bits will be having luma residual bits till this point    */
    /* if zero_cbf eval is disabled then cu bits will be zero       */
    ps_final_prms->u4_cu_luma_res_bits = cu_bits;

    /* ------------- Chroma processing -------------- */
    /* Chroma rdopt eval for each luma candidate only for HIGH QUALITY/MEDIUM SPEDD preset*/
    if(1 == ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt)
    {
        LWORD64 chrm_rdopt_cost;
        WORD32 chrm_rdopt_tu_bits;

        /* Store the current RDOPT cost to enable early exit in chrom_prcs */
        ps_ctxt->as_cu_prms[curr_buf_idx].i8_curr_rdopt_cost = total_rdopt_cost;

        chrm_rdopt_cost = ihevce_chroma_cu_prcs_rdopt(
            ps_ctxt,
            curr_buf_idx,
            func_proc_mode,
            ps_chrm_cu_buf_prms->pu1_curr_src,
            ps_chrm_cu_buf_prms->i4_chrm_src_stride,
            ps_chrm_cu_buf_prms->pu1_cu_left,
            ps_chrm_cu_buf_prms->pu1_cu_top,
            ps_chrm_cu_buf_prms->pu1_cu_top_left,
            ps_chrm_cu_buf_prms->i4_cu_left_stride,
            cu_pos_x_8pelunits,
            cu_pos_y_8pelunits,
            &chrm_rdopt_tu_bits,
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy);

#if WEIGH_CHROMA_COST
        chrm_rdopt_cost = (LWORD64)(
            (chrm_rdopt_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
             (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
            CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif

#if CHROMA_RDOPT_ENABLE
        total_rdopt_cost += chrm_rdopt_cost;
#endif
        cu_bits += chrm_rdopt_tu_bits;

        /* cu bits for chroma residual if chroma rdopt is on       */
        /* if zero_cbf eval is disabled then cu bits will be zero  */
        ps_final_prms->u4_cu_chroma_res_bits = chrm_rdopt_tu_bits;

        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
        {
            /* Early exit : If the current running cost exceeds
            the prev. best mode cost, break */
            if(total_rdopt_cost > prev_best_rdopt_cost)
            {
                return (total_rdopt_cost);
            }
        }
    }
    else
    {}

    /* RDOPT copy States :  Best after all luma TUs to current */
    COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                .s_cabac_ctxt.au1_ctxt_models[0] +
            IHEVC_CAB_COEFFX_PREFIX,
        &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
        IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

    /* get the neighbour availability flags for current cu  */
    ihevce_get_only_nbr_flag(
        &s_nbr,
        ps_ctxt->pu1_ctb_nbr_map,
        ps_ctxt->i4_nbr_map_strd,
        (cu_pos_x_8pelunits << 1),
        (cu_pos_y_8pelunits << 1),
        (trans_size << 1),
        (trans_size << 1));

    /* call the entropy rdo encode to get the bit estimate for current cu */
    /*if ZERO_CBF eval is enabled then this function will return only CU header bits */
    {
        /*cbf_bits will account for both texture and cbf bits when zero cbf eval flag is 0*/
        WORD32 cbf_bits, header_bits;

        header_bits = ihevce_entropy_rdo_encode_cu(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_final_prms,
            cu_pos_x_8pelunits,
            cu_pos_y_8pelunits,
            ps_cu_analyse->u1_cu_size,
            s_nbr.u1_top_avail,
            s_nbr.u1_left_avail,
            &ps_final_prms->pu1_cu_coeffs[0],
            &cbf_bits);

        cu_bits += header_bits;

        /* cbf bits are excluded from header bits, instead considered as texture bits */
        /* incase if zero cbf eval is disabled then texture bits gets added here */
        ps_final_prms->u4_cu_hdr_bits = (header_bits - cbf_bits);
        ps_final_prms->u4_cu_cbf_bits = cbf_bits;

#if RDOPT_ENABLE
        /* add the cost of coding the cu bits */
        total_rdopt_cost +=
            COMPUTE_RATE_COST_CLIP30(header_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);
#endif
    }
    return (total_rdopt_cost);
}
/*!
******************************************************************************
* \if Function name : ihevce_inter_rdopt_cu_ntu \endif
*
* \brief
*    Inter Coding unit funtion whic perfomr the TQ IT IQ recon for luma
*
* \param[in] ps_ctxt       enc_loop module ctxt pointer
* \param[in] ps_inter_cand pointer to inter candidate structure
* \param[in] pu1_src       pointer to source data buffer
* \param[in] cu_size       Current CU size
* \param[in] cu_pos_x      cu position x w.r.t to ctb
* \param[in] cu_pos_y      cu position y w.r.t to ctb
* \param[in] src_strd      source buffer stride
* \param[in] curr_buf_idx  buffer index for current output storage
* \param[in] ps_chrm_cu_buf_prms pointer to chroma buffer pointers structure
*
* \return
*    Rdopt cost
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_inter_rdopt_cu_ntu(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_src,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 curr_buf_idx,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_inter_cand_t *ps_inter_cand,
    cu_analyse_t *ps_cu_analyse,
    WORD32 i4_alpha_stim_multiplier)
{
    enc_loop_cu_final_prms_t *ps_final_prms;
    nbr_4x4_t *ps_nbr_4x4;
    tu_prms_t s_tu_prms[64 * 4];
    tu_prms_t *ps_tu_prms;

    WORD32 i4_perform_rdoq;
    WORD32 i4_perform_sbh;
    WORD32 ai4_tu_split_flags[4];
    WORD32 ai4_tu_early_cbf[4];
    WORD32 num_split_flags = 1;
    WORD32 i;
    UWORD8 u1_tu_size;
    UWORD8 *pu1_pred;
    UWORD8 *pu1_ecd_data;
    WORD16 *pi2_deq_data;
    UWORD8 *pu1_csbf_buf;
    UWORD8 *pu1_tu_sz_sft;
    UWORD8 *pu1_tu_posx;
    UWORD8 *pu1_tu_posy;
    LWORD64 total_rdopt_cost;
    WORD32 ctr;
    WORD32 chrm_ctr;
    WORD32 num_tu_in_cu = 0;
    WORD32 pred_stride;
    WORD32 recon_stride;
    WORD32 trans_size = ps_cu_analyse->u1_cu_size;
    WORD32 csbf_strd;
    WORD32 chrm_present_flag;
    WORD32 ecd_data_bytes_cons;
    WORD32 num_4x4_in_cu;
    WORD32 num_4x4_in_tu;
    WORD32 recon_func_mode;
    WORD32 cu_bits;
    UWORD8 u1_compute_spatial_ssd;

    /* min_trans_size is initialized to some huge number than usual TU sizes */
    WORD32 i4_min_trans_size = 256;
    /* Get the RDOPT cost of the best CU mode for early_exit */
    LWORD64 prev_best_rdopt_cost = ps_ctxt->as_cu_prms[!curr_buf_idx].i8_best_rdopt_cost;
    WORD32 src_strd = ps_cu_prms->i4_luma_src_stride;

    /* model for no residue syntax qt root cbf flag */
    UWORD8 u1_qtroot_cbf_cabac_model = ps_ctxt->au1_rdopt_init_ctxt_models[IHEVC_CAB_NORES_IDX];

    /* backup copy of cabac states for restoration if zero cu reside rdo wins later */
    UWORD8 au1_rdopt_init_ctxt_models[IHEVC_CAB_CTXT_END];

    /* for skip cases tables are not reqquired */
    UWORD8 u1_skip_tu_sz_sft = 0;
    UWORD8 u1_skip_tu_posx = 0;
    UWORD8 u1_skip_tu_posy = 0;
    UWORD8 u1_is_cu_noisy = ps_cu_prms->u1_is_cu_noisy;

    /* get the pointers based on curbuf idx */
    ps_final_prms = &ps_ctxt->as_cu_prms[curr_buf_idx];
    ps_nbr_4x4 = &ps_ctxt->as_cu_nbr[curr_buf_idx][0];
    pu1_ecd_data = &ps_final_prms->pu1_cu_coeffs[0];
    pi2_deq_data = &ps_final_prms->pi2_cu_deq_coeffs[0];
    csbf_strd = ps_ctxt->i4_cu_csbf_strd;
    pu1_csbf_buf = &ps_ctxt->au1_cu_csbf[0];

    pred_stride = ps_inter_cand->i4_pred_data_stride;
    recon_stride = cu_size;
    pu1_pred = ps_inter_cand->pu1_pred_data;
    chrm_ctr = 0;
    ecd_data_bytes_cons = 0;
    total_rdopt_cost = 0;
    num_4x4_in_cu = cu_size >> 2;
    recon_func_mode = PRED_MODE_INTER;
    cu_bits = 0;

    /* get the 4x4 level postion of current cu */
    cu_pos_x = cu_pos_x << 1;
    cu_pos_y = cu_pos_y << 1;

    /* default value for cu coded flag */
    ps_final_prms->u1_is_cu_coded = 0;

    /*init of ssd of CU accuumulated over all TU*/
    ps_final_prms->u4_cu_sad = 0;

    /* populate the coeffs scan idx */
    ps_ctxt->i4_scan_idx = SCAN_DIAG_UPRIGHT;

#if ENABLE_INTER_ZCU_COST
    /* reset cu not coded cost */
    ps_ctxt->i8_cu_not_coded_cost = 0;

    /* backup copy of cabac states for restoration if zero cu reside rdo wins later */
    memcpy(au1_rdopt_init_ctxt_models, &ps_ctxt->au1_rdopt_init_ctxt_models[0], IHEVC_CAB_CTXT_END);
#endif

    if(ps_cu_analyse->u1_cu_size == 64)
    {
        num_split_flags = 4;
        u1_tu_size = 32;
    }
    else
    {
        num_split_flags = 1;
        u1_tu_size = ps_cu_analyse->u1_cu_size;
    }

    /* ckeck for skip mode */
    if(1 == ps_final_prms->u1_skip_flag)
    {
        if(64 == cu_size)
        {
            /* TU = CU/2 is set but no trnaform is evaluated  */
            num_tu_in_cu = 4;
            pu1_tu_sz_sft = &gau1_inter_tu_shft_amt[0];
            pu1_tu_posx = &gau1_inter_tu_posx_scl_amt[0];
            pu1_tu_posy = &gau1_inter_tu_posy_scl_amt[0];
        }
        else
        {
            /* TU = CU is set but no trnaform is evaluated  */
            num_tu_in_cu = 1;
            pu1_tu_sz_sft = &u1_skip_tu_sz_sft;
            pu1_tu_posx = &u1_skip_tu_posx;
            pu1_tu_posy = &u1_skip_tu_posy;
        }

        recon_func_mode = PRED_MODE_SKIP;
    }
    /* check for PU part mode being AMP or No AMP */
    else if(ps_final_prms->u1_part_mode < SIZE_2NxnU)
    {
        if((SIZE_2Nx2N == ps_final_prms->u1_part_mode) && (cu_size < 64))
        {
            /* TU= CU is evaluated 2Nx2N inter case */
            num_tu_in_cu = 1;
            pu1_tu_sz_sft = &u1_skip_tu_sz_sft;
            pu1_tu_posx = &u1_skip_tu_posx;
            pu1_tu_posy = &u1_skip_tu_posy;
        }
        else
        {
            /* currently TU= CU/2 is evaluated for all inter case */
            num_tu_in_cu = 4;
            pu1_tu_sz_sft = &gau1_inter_tu_shft_amt[0];
            pu1_tu_posx = &gau1_inter_tu_posx_scl_amt[0];
            pu1_tu_posy = &gau1_inter_tu_posy_scl_amt[0];
        }
    }
    else
    {
        /* for AMP cases one level of TU recurssion is done */
        /* based on oreintation of the partitions           */
        num_tu_in_cu = 10;
        pu1_tu_sz_sft = &gau1_inter_tu_shft_amt_amp[ps_final_prms->u1_part_mode - 4][0];
        pu1_tu_posx = &gau1_inter_tu_posx_scl_amt_amp[ps_final_prms->u1_part_mode - 4][0];
        pu1_tu_posy = &gau1_inter_tu_posy_scl_amt_amp[ps_final_prms->u1_part_mode - 4][0];
    }

    ps_tu_prms = &s_tu_prms[0];
    num_tu_in_cu = 0;

    for(i = 0; i < num_split_flags; i++)
    {
        WORD32 i4_x_off = 0, i4_y_off = 0;

        if(i == 1 || i == 3)
        {
            i4_x_off = 32;
        }

        if(i == 2 || i == 3)
        {
            i4_y_off = 32;
        }

        if(1 == ps_final_prms->u1_skip_flag)
        {
            ai4_tu_split_flags[0] = 0;
            ps_inter_cand->ai4_tu_split_flag[i] = 0;

            ai4_tu_early_cbf[0] = 0;
        }
        else
        {
            ai4_tu_split_flags[0] = ps_inter_cand->ai4_tu_split_flag[i];
            ai4_tu_early_cbf[0] = ps_inter_cand->ai4_tu_early_cbf[i];
        }

        ps_tu_prms->u1_tu_size = u1_tu_size;

        ps_tu_prms = (tu_prms_t *)ihevce_tu_tree_update(
            ps_tu_prms,
            &num_tu_in_cu,
            0,
            ai4_tu_split_flags[0],
            ai4_tu_early_cbf[0],
            i4_x_off,
            i4_y_off);
    }

    /* loop for all tu blocks in current cu */
    ps_tu_prms = &s_tu_prms[0];
    for(ctr = 0; ctr < num_tu_in_cu; ctr++)
    {
        trans_size = ps_tu_prms->u1_tu_size;

        if(i4_min_trans_size > trans_size)
        {
            i4_min_trans_size = trans_size;
        }
        ps_tu_prms++;
    }

    if(ps_ctxt->i1_cu_qp_delta_enable)
    {
        ihevce_update_cu_level_qp_lamda(ps_ctxt, ps_cu_analyse, i4_min_trans_size, 0);
    }

    if(u1_is_cu_noisy && !ps_ctxt->u1_enable_psyRDOPT)
    {
        ps_ctxt->i8_cl_ssd_lambda_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_qf * (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) /
             100.0f);
        ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_chroma_qf *
             (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);
    }

    u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_QP_WHERE_SPATIAL_SSD_ENABLED) &&
                             (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
                             CONVERT_SSDS_TO_SPATIAL_DOMAIN;

    if(u1_is_cu_noisy || ps_ctxt->u1_enable_psyRDOPT)
    {
        u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_HEVC_QP) &&
                                 CONVERT_SSDS_TO_SPATIAL_DOMAIN;
    }

    if(!u1_compute_spatial_ssd)
    {
        ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 0;
        ps_final_prms->s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
    }
    else
    {
        ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 1;
    }

    ps_tu_prms = &s_tu_prms[0];

    ASSERT(num_tu_in_cu <= 256);

    /* RDOPT copy States :  TU init (best until prev TU) to current */
    memcpy(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
             .s_cabac_ctxt.au1_ctxt_models[0],
        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
        IHEVC_CAB_COEFFX_PREFIX);

    for(ctr = 0; ctr < num_tu_in_cu; ctr++)
    {
        WORD32 curr_bytes;
        WORD32 tx_size;
        WORD32 cbf, zero_col, zero_row;
        LWORD64 rdopt_cost;
        UWORD8 u1_is_recon_available;

        WORD32 curr_pos_x;
        WORD32 curr_pos_y;
        nbr_4x4_t *ps_cur_nbr_4x4;
        UWORD8 *pu1_cur_pred;
        UWORD8 *pu1_cur_src;
        UWORD8 *pu1_cur_recon;
        WORD16 *pi2_cur_deq_data;
        UWORD32 u4_tu_sad;
        WORD32 tu_bits;

        WORD32 i4_recon_stride = ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;

        trans_size = ps_tu_prms->u1_tu_size;
        /* get the current pos x and pos y in pixels */
        curr_pos_x = ps_tu_prms->u1_x_off;  //((cu_size >> 2) * pu1_tu_posx[ctr]);
        curr_pos_y = ps_tu_prms->u1_y_off;  //((cu_size >> 2) * pu1_tu_posy[ctr]);

        num_4x4_in_tu = trans_size >> 2;

#if FORCE_8x8_TFR
        if(cu_size == 64)
        {
            curr_pos_x = ((cu_size >> 3) * pu1_tu_posx[ctr]);
            curr_pos_y = ((cu_size >> 3) * pu1_tu_posy[ctr]);
        }
#endif

        /* increment the pointers to start of current TU  */
        pu1_cur_src = ((UWORD8 *)pv_src + curr_pos_x);
        pu1_cur_src += (curr_pos_y * src_strd);
        pu1_cur_pred = (pu1_pred + curr_pos_x);
        pu1_cur_pred += (curr_pos_y * pred_stride);
        pi2_cur_deq_data = pi2_deq_data + curr_pos_x;
        pi2_cur_deq_data += (curr_pos_y * cu_size);
        pu1_cur_recon = ((UWORD8 *)ps_final_prms->s_recon_datastore.apv_luma_recon_bufs[0]) +
                        curr_pos_x + curr_pos_y * i4_recon_stride;

        ps_cur_nbr_4x4 = (ps_nbr_4x4 + (curr_pos_x >> 2));
        ps_cur_nbr_4x4 += ((curr_pos_y >> 2) * num_4x4_in_cu);

        /* RDOPT copy States :  TU init (best until prev TU) to current */
        COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                    .s_cabac_ctxt.au1_ctxt_models[0] +
                IHEVC_CAB_COEFFX_PREFIX,
            &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
            IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

        i4_perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq;
        i4_perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh;

        /*2 Multi- dimensinal array based on trans size  of rounding factor to be added here */
        /* arrays are for rounding factor corr. to 0-1 decision and 1-2 decision */
        /* Currently the complete array will contain only single value*/
        /*The rounding factor is calculated with the formula
        Deadzone val = (((R1 - R0) * (2^(-8/3)) * lamMod) + 1)/2
        rounding factor = (1 - DeadZone Val)

        Assumption: Cabac states of All the sub-blocks in the TU are considered independent
        */
        if((ps_ctxt->i4_quant_rounding_level == TU_LEVEL_QUANT_ROUNDING) && (ctr != 0))
        {
            double i4_lamda_modifier;

            if((BSLICE == ps_ctxt->i1_slice_type) && (ps_ctxt->i4_temporal_layer_id))
            {
                i4_lamda_modifier = ps_ctxt->i4_lamda_modifier *
                                    CLIP3((((double)(ps_ctxt->i4_cu_qp - 12)) / 6.0), 2.00, 4.00);
            }
            else
            {
                i4_lamda_modifier = ps_ctxt->i4_lamda_modifier;
            }
            if(ps_ctxt->i4_use_const_lamda_modifier)
            {
                if(ISLICE == ps_ctxt->i1_slice_type)
                {
                    i4_lamda_modifier = ps_ctxt->f_i_pic_lamda_modifier;
                }
                else
                {
                    i4_lamda_modifier = CONST_LAMDA_MOD_VAL;
                }
            }
            ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3] =
                &ps_ctxt->i4_quant_round_tu[0][0];
            ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3] =
                &ps_ctxt->i4_quant_round_tu[1][0];

            memset(
                ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                0,
                trans_size * trans_size * sizeof(WORD32));
            memset(
                ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
                0,
                trans_size * trans_size * sizeof(WORD32));

            ihevce_quant_rounding_factor_gen(
                trans_size,
                1,
                &ps_ctxt->s_rdopt_entropy_ctxt,
                ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3],
                ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3],
                i4_lamda_modifier,
                1);
        }
        else
        {
            ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3] =
                ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[trans_size >> 3];
            ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3] =
                ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[trans_size >> 3];
        }

        /* call T Q IT IQ and recon function */
        cbf = ihevce_t_q_iq_ssd_scan_fxn(
            ps_ctxt,
            pu1_cur_pred,
            pred_stride,
            pu1_cur_src,
            src_strd,
            pi2_cur_deq_data,
            cu_size,
            pu1_cur_recon,
            i4_recon_stride,
            pu1_ecd_data,
            pu1_csbf_buf,
            csbf_strd,
            trans_size,
            recon_func_mode,
            &rdopt_cost,
            &curr_bytes,
            &tu_bits,
            &u4_tu_sad,
            &zero_col,
            &zero_row,
            &u1_is_recon_available,
            i4_perform_rdoq,
            i4_perform_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
            ps_ctxt->u1_use_early_cbf_data ? ps_tu_prms->i4_early_cbf : 1);

#if COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL && !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
        {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
            rdopt_cost = ihevce_inject_stim_into_distortion(
                pu1_cur_src,
                src_strd,
                pu1_cur_pred,
                pred_stride,
                rdopt_cost,
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                NULL_PLANE);
#else
            if(u1_compute_spatial_ssd && u1_is_recon_available)
            {
                rdopt_cost = ihevce_inject_stim_into_distortion(
                    pu1_cur_src,
                    src_strd,
                    pu1_cur_recon,
                    i4_recon_stride,
                    rdopt_cost,
                    i4_alpha_stim_multiplier,
                    trans_size,
                    0,
                    NULL_PLANE);
            }
            else
            {
                rdopt_cost = ihevce_inject_stim_into_distortion(
                    pu1_cur_src,
                    src_strd,
                    pu1_cur_pred,
                    pred_stride,
                    rdopt_cost,
                    i4_alpha_stim_multiplier,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    NULL_PLANE);
            }
#endif
        }
#endif

        if(u1_compute_spatial_ssd && u1_is_recon_available)
        {
            ps_final_prms->s_recon_datastore.au1_bufId_with_winning_LumaRecon[ctr] = 0;
        }
        else
        {
            ps_final_prms->s_recon_datastore.au1_bufId_with_winning_LumaRecon[ctr] = UCHAR_MAX;
        }

        /* accumulate the TU sad into cu sad */
        ps_final_prms->u4_cu_sad += u4_tu_sad;

        /* accumulate the TU bits into cu bits */
        cu_bits += tu_bits;

        /* inter cu is coded if any of the tu is coded in it */
        ps_final_prms->u1_is_cu_coded |= cbf;

        /* call the entropy function to get the bits */
        /* add that to rd opt cost(SSD)              */

        /* update the bytes */
        ps_final_prms->as_tu_enc_loop[ctr].i4_luma_coeff_offset = ecd_data_bytes_cons;
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].i2_luma_bytes_consumed = curr_bytes;
        /* update the zero_row and col info for the final mode */
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].u4_luma_zero_col = zero_col;
        ps_final_prms->as_tu_enc_loop_temp_prms[ctr].u4_luma_zero_row = zero_row;

        /* update the bytes */
        ps_final_prms->as_tu_enc_loop[ctr].i4_luma_coeff_offset = ecd_data_bytes_cons;

        /* update the total bytes cons */
        ecd_data_bytes_cons += curr_bytes;
        pu1_ecd_data += curr_bytes;

        /* RDOPT copy States :  New updated after curr TU to TU init */
        if(0 != cbf)
        {
            /* update to new state only if CBF is non zero */
            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                        .s_cabac_ctxt.au1_ctxt_models[0] +
                    IHEVC_CAB_COEFFX_PREFIX,
                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
        }

        /* by default chroma present is set to 1*/
        chrm_present_flag = 1;
        if(4 == trans_size)
        {
            /* if tusize is 4x4 then only first luma 4x4 will have chroma*/
            if(0 != chrm_ctr)
            {
                chrm_present_flag = INTRA_PRED_CHROMA_IDX_NONE;
            }

            /* increment the chrm ctr unconditionally */
            chrm_ctr++;

            /* after ctr reached 4 reset it */
            if(4 == chrm_ctr)
            {
                chrm_ctr = 0;
            }
        }

        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_y_cbf = cbf;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_chroma_intra_mode_idx = chrm_present_flag;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b7_qp = ps_ctxt->i4_cu_qp;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_first_tu_in_cu = 0;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_transquant_bypass = 0;
        GETRANGE(tx_size, trans_size);
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_size = tx_size - 3;
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_x = cu_pos_x + (curr_pos_x >> 2);
        ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_y = cu_pos_y + (curr_pos_y >> 2);

        /* repiclate the nbr 4x4 structure for all 4x4 blocks current TU */
        ps_cur_nbr_4x4->b1_y_cbf = cbf;
        /*copy the cu qp. This will be overwritten by qp calculated based on skip flag at final stage of cu mode decide*/
        ps_cur_nbr_4x4->b8_qp = ps_ctxt->i4_cu_qp;

        /* Qp and cbf are stored for the all 4x4 in TU */
        {
            WORD32 i, j;
            nbr_4x4_t *ps_tmp_4x4;
            ps_tmp_4x4 = ps_cur_nbr_4x4;

            for(i = 0; i < num_4x4_in_tu; i++)
            {
                for(j = 0; j < num_4x4_in_tu; j++)
                {
                    ps_tmp_4x4[j].b8_qp = ps_ctxt->i4_cu_qp;
                    ps_tmp_4x4[j].b1_y_cbf = cbf;
                }
                /* row level update*/
                ps_tmp_4x4 += num_4x4_in_cu;
            }
        }

#if RDOPT_ENABLE
        /* compute the rdopt cost */
        rdopt_cost +=
            COMPUTE_RATE_COST_CLIP30(tu_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);
#endif
        /* accumulate the costs */
        total_rdopt_cost += rdopt_cost;

        ps_tu_prms++;

        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
        {
            /* Early exit : If the current running cost exceeds
            the prev. best mode cost, break */
            if(total_rdopt_cost > prev_best_rdopt_cost)
            {
                return (total_rdopt_cost);
            }
        }
    }

    /* Modify the cost function for this CU. */
    /* loop in for 8x8 blocks */
    if(ps_ctxt->u1_enable_psyRDOPT)
    {
        UWORD8 *pu1_recon_cu;
        WORD32 recon_stride;
        WORD32 curr_pos_x;
        WORD32 curr_pos_y;
        WORD32 start_index;
        WORD32 num_horz_cu_in_ctb;
        WORD32 had_block_size;

        /* tODO: sreenivasa ctb size has to be used appropriately */
        had_block_size = 8;
        num_horz_cu_in_ctb = 64 / had_block_size;

        curr_pos_x = cu_pos_x << 2; /* pel units */
        curr_pos_y = cu_pos_y << 2; /* pel units */
        recon_stride = ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;
        pu1_recon_cu = ((UWORD8 *)ps_final_prms->s_recon_datastore
                            .apv_luma_recon_bufs[0]);  // already pointing to the current CU recon
        //+ \curr_pos_x + curr_pos_y * recon_stride;

        /* start index to index the source satd of curr cu int he current ctb*/
        start_index =
            (curr_pos_x / had_block_size) + (curr_pos_y / had_block_size) * num_horz_cu_in_ctb;

        {
            total_rdopt_cost += ihevce_psy_rd_cost(
                ps_ctxt->ai4_source_satd_8x8,
                pu1_recon_cu,
                recon_stride,
                1,  //howz stride
                cu_size,
                0,  // pic type
                0,  //layer id
                ps_ctxt->i4_satd_lamda,  // lambda
                start_index,
                ps_ctxt->u1_is_input_data_hbd,
                ps_ctxt->u4_psy_strength,
                &ps_ctxt->s_cmn_opt_func);  // 8 bit
        }
    }

    /* store the num TUs*/
    ps_final_prms->u2_num_tus_in_cu = num_tu_in_cu;

    /* update the bytes consumed */
    ps_final_prms->i4_num_bytes_ecd_data = ecd_data_bytes_cons;

    /* store the current cu size to final prms */
    ps_final_prms->u1_cu_size = cu_size;

    /* cu bits will be having luma residual bits till this point    */
    /* if zero_cbf eval is disabled then cu bits will be zero       */
    ps_final_prms->u4_cu_luma_res_bits = cu_bits;

    /* ------------- Chroma processing -------------- */
    /* Chroma rdopt eval for each luma candidate only for HIGH QUALITY/MEDIUM SPEDD preset*/
    if(1 == ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt)
    {
        LWORD64 chrm_rdopt_cost;
        WORD32 chrm_rdopt_tu_bits;

        /* Store the current RDOPT cost to enable early exit in chrom_prcs */
        ps_ctxt->as_cu_prms[curr_buf_idx].i8_curr_rdopt_cost = total_rdopt_cost;

        chrm_rdopt_cost = ihevce_chroma_cu_prcs_rdopt(
            ps_ctxt,
            curr_buf_idx,
            0, /* TU mode : Don't care in Inter patrh */
            ps_chrm_cu_buf_prms->pu1_curr_src,
            ps_chrm_cu_buf_prms->i4_chrm_src_stride,
            ps_chrm_cu_buf_prms->pu1_cu_left,
            ps_chrm_cu_buf_prms->pu1_cu_top,
            ps_chrm_cu_buf_prms->pu1_cu_top_left,
            ps_chrm_cu_buf_prms->i4_cu_left_stride,
            (cu_pos_x >> 1),
            (cu_pos_y >> 1),
            &chrm_rdopt_tu_bits,
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy);

#if WEIGH_CHROMA_COST
        chrm_rdopt_cost = (LWORD64)(
            (chrm_rdopt_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
             (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
            CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif

#if CHROMA_RDOPT_ENABLE
        total_rdopt_cost += chrm_rdopt_cost;
#endif
        cu_bits += chrm_rdopt_tu_bits;

        /* during chroma evaluation if skip decision was over written     */
        /* then the current skip candidate is set to a non skip candidate */
        ps_inter_cand->b1_skip_flag = ps_final_prms->u1_skip_flag;

        /* cu bits for chroma residual if chroma rdopt is on       */
        /* if zero_cbf eval is disabled then cu bits will be zero  */
        ps_final_prms->u4_cu_chroma_res_bits = chrm_rdopt_tu_bits;

        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
        {
            /* Early exit : If the current running cost exceeds
            the prev. best mode cost, break */
            if(total_rdopt_cost > prev_best_rdopt_cost)
            {
                return (total_rdopt_cost);
            }
        }
    }
    else
    {}

#if SHRINK_INTER_TUTREE
    /* ------------- Quadtree TU split  optimization ------------  */
    if(ps_final_prms->u1_is_cu_coded)
    {
        ps_final_prms->u2_num_tus_in_cu = ihevce_shrink_inter_tu_tree(
            &ps_final_prms->as_tu_enc_loop[0],
            &ps_final_prms->as_tu_enc_loop_temp_prms[0],
            &ps_final_prms->s_recon_datastore,
            num_tu_in_cu,
            (ps_ctxt->u1_chroma_array_type == 2));
    }
#endif

    /* RDOPT copy States :  Best after all luma TUs (and chroma,if enabled)to current */
    COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                .s_cabac_ctxt.au1_ctxt_models[0] +
            IHEVC_CAB_COEFFX_PREFIX,
        &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
        IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

    /* -------- Bit estimate for RD opt -------------- */
    {
        nbr_avail_flags_t s_nbr;
        /*cbf_bits will account for both texture and cbf bits when zero cbf eval flag is 0*/
        WORD32 cbf_bits, header_bits;

        /* get the neighbour availability flags for current cu  */
        ihevce_get_only_nbr_flag(
            &s_nbr,
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            cu_pos_x,
            cu_pos_y,
            (cu_size >> 2),
            (cu_size >> 2));

        /* call the entropy rdo encode to get the bit estimate for current cu */
        header_bits = ihevce_entropy_rdo_encode_cu(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_final_prms,
            (cu_pos_x >> 1), /*  back to 8x8 pel units   */
            (cu_pos_y >> 1), /*  back to 8x8 pel units   */
            cu_size,
            ps_ctxt->u1_disable_intra_eval ? !DISABLE_TOP_SYNC && s_nbr.u1_top_avail
                                           : s_nbr.u1_top_avail,
            s_nbr.u1_left_avail,
            &ps_final_prms->pu1_cu_coeffs[0],
            &cbf_bits);

        cu_bits += header_bits;

        /* cbf bits are excluded from header bits, instead considered as texture bits */
        /* incase if zero cbf eval is disabled then texture bits gets added here */
        ps_final_prms->u4_cu_hdr_bits = (header_bits - cbf_bits);
        ps_final_prms->u4_cu_cbf_bits = cbf_bits;

#if RDOPT_ENABLE
        /* add the cost of coding the header bits */
        total_rdopt_cost +=
            COMPUTE_RATE_COST_CLIP30(header_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

#if ENABLE_INTER_ZCU_COST
        /* If cu is coded, Evaluate not coded cost and check if it improves over coded cost */
        if(ps_final_prms->u1_is_cu_coded && (ZCBF_ENABLE == ps_ctxt->i4_zcbf_rdo_level))
        {
            LWORD64 i8_cu_not_coded_cost = ps_ctxt->i8_cu_not_coded_cost;

            WORD32 is_2nx2n_mergecu = (SIZE_2Nx2N == ps_final_prms->u1_part_mode) &&
                                      (1 == ps_final_prms->as_pu_enc_loop[0].b1_merge_flag);

            cab_ctxt_t *ps_cab_ctxt =
                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx].s_cabac_ctxt;

            /* Read header bits generatated after ihevce_entropy_rdo_encode_cu() call  */
            UWORD32 u4_cu_hdr_bits_q12 = ps_cab_ctxt->u4_header_bits_estimated_q12;

            /* account for coding qt_root_cbf = 0 */
            /* First subtract cost for coding as 1 (part of header bits) and then add cost for coding as 0 */
            u4_cu_hdr_bits_q12 += gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 0];
            if(u4_cu_hdr_bits_q12 < gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 1])
                u4_cu_hdr_bits_q12 = 0;
            else
                u4_cu_hdr_bits_q12 -= gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 1];

            /* add the cost of coding the header bits */
            i8_cu_not_coded_cost += COMPUTE_RATE_COST_CLIP30(
                u4_cu_hdr_bits_q12 /* ps_final_prms->u4_cu_hdr_bits */,
                ps_ctxt->i8_cl_ssd_lambda_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

            if(ps_ctxt->u1_enable_psyRDOPT)
            {
                i8_cu_not_coded_cost = total_rdopt_cost + 1;
            }

            /* Evaluate qtroot cbf rdo; exclude 2Nx2N Merge as skip cu is explicitly evaluated */
            if((i8_cu_not_coded_cost <= total_rdopt_cost) && (!is_2nx2n_mergecu))
            {
                WORD32 tx_size;

                /* force cu as not coded and update the cost */
                ps_final_prms->u1_is_cu_coded = 0;
                ps_final_prms->s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
                ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 0;

                total_rdopt_cost = i8_cu_not_coded_cost;

                /* reset num TUs to 1 unless cu size id 64 */
                ps_final_prms->u2_num_tus_in_cu = (64 == cu_size) ? 4 : 1;
                trans_size = (64 == cu_size) ? 32 : cu_size;
                GETRANGE(tx_size, trans_size);

                /* reset the bytes consumed */
                ps_final_prms->i4_num_bytes_ecd_data = 0;

                /* reset texture related bits and roll back header bits*/
                ps_final_prms->u4_cu_cbf_bits = 0;
                ps_final_prms->u4_cu_luma_res_bits = 0;
                ps_final_prms->u4_cu_chroma_res_bits = 0;
                ps_final_prms->u4_cu_hdr_bits =
                    (u4_cu_hdr_bits_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >> CABAC_FRAC_BITS_Q;

                /* update cabac model with qtroot cbf = 0 decision */
                ps_cab_ctxt->au1_ctxt_models[IHEVC_CAB_NORES_IDX] =
                    gau1_ihevc_next_state[u1_qtroot_cbf_cabac_model << 1];

                /* restore untouched cabac models for, tusplit, cbfs, texture etc */
                memcpy(
                    &ps_cab_ctxt->au1_ctxt_models[IHEVC_CAB_SPLIT_TFM],
                    &au1_rdopt_init_ctxt_models[IHEVC_CAB_SPLIT_TFM],
                    (IHEVC_CAB_CTXT_END - IHEVC_CAB_SPLIT_TFM));

                /* mark all tus as not coded for final eval */
                for(ctr = 0; ctr < ps_final_prms->u2_num_tus_in_cu; ctr++)
                {
                    WORD32 curr_pos_x = (ctr & 0x1) ? (trans_size >> 2) : 0;
                    WORD32 curr_pos_y = (ctr & 0x2) ? (trans_size >> 2) : 0;

                    nbr_4x4_t *ps_cur_nbr_4x4 =
                        ps_nbr_4x4 + curr_pos_x + (curr_pos_y * num_4x4_in_cu);

                    num_4x4_in_tu = trans_size >> 2;

                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].i2_luma_bytes_consumed = 0;
                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].ai2_cb_bytes_consumed[0] = 0;
                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].ai2_cr_bytes_consumed[0] = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_y_cbf = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf_subtu1 = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf_subtu1 = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_size = tx_size - 3;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_x = cu_pos_x + curr_pos_x;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_y = cu_pos_y + curr_pos_y;

                    /* reset cbf for the all 4x4 in TU */
                    {
                        WORD32 i, j;
                        nbr_4x4_t *ps_tmp_4x4;
                        ps_tmp_4x4 = ps_cur_nbr_4x4;

                        for(i = 0; i < num_4x4_in_tu; i++)
                        {
                            for(j = 0; j < num_4x4_in_tu; j++)
                            {
                                ps_tmp_4x4[j].b1_y_cbf = 0;
                            }
                            /* row level update*/
                            ps_tmp_4x4 += num_4x4_in_cu;
                        }
                    }
                }
            }
        }
#endif /* ENABLE_INTER_ZCU_COST */

#endif /* RDOPT_ENABLE */
    }

    return (total_rdopt_cost);
}

#if ENABLE_RDO_BASED_TU_RECURSION
LWORD64 ihevce_inter_tu_tree_selector_and_rdopt_cost_computer(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    void *pv_src,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 curr_buf_idx,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_inter_cand_t *ps_inter_cand,
    cu_analyse_t *ps_cu_analyse,
    WORD32 i4_alpha_stim_multiplier)
{
    tu_tree_node_t as_tu_nodes[256 + 64 + 16 + 4 + 1];
    buffer_data_for_tu_t s_buffer_data_for_tu;
    enc_loop_cu_final_prms_t *ps_final_prms;
    nbr_4x4_t *ps_nbr_4x4;

    WORD32 num_split_flags = 1;
    UWORD8 u1_tu_size;
    UWORD8 *pu1_pred;
    UWORD8 *pu1_ecd_data;
    WORD16 *pi2_deq_data;
    UWORD8 *pu1_csbf_buf;
    UWORD8 *pu1_tu_sz_sft;
    UWORD8 *pu1_tu_posx;
    UWORD8 *pu1_tu_posy;
    LWORD64 total_rdopt_cost;
    WORD32 ctr;
    WORD32 chrm_ctr;
    WORD32 pred_stride;
    WORD32 recon_stride;
    WORD32 trans_size = ps_cu_analyse->u1_cu_size;
    WORD32 csbf_strd;
    WORD32 ecd_data_bytes_cons;
    WORD32 num_4x4_in_cu;
    WORD32 num_4x4_in_tu;
    WORD32 recon_func_mode;
    WORD32 cu_bits;
    UWORD8 u1_compute_spatial_ssd;
    /* backup copy of cabac states for restoration if zero cu reside rdo wins later */
    UWORD8 au1_rdopt_init_ctxt_models[IHEVC_CAB_CTXT_END];

    WORD32 i4_min_trans_size = 256;
    LWORD64 prev_best_rdopt_cost = ps_ctxt->as_cu_prms[!curr_buf_idx].i8_best_rdopt_cost;
    WORD32 src_strd = ps_cu_prms->i4_luma_src_stride;
    /* model for no residue syntax qt root cbf flag */
    UWORD8 u1_qtroot_cbf_cabac_model = ps_ctxt->au1_rdopt_init_ctxt_models[IHEVC_CAB_NORES_IDX];
    UWORD8 u1_skip_tu_sz_sft = 0;
    UWORD8 u1_skip_tu_posx = 0;
    UWORD8 u1_skip_tu_posy = 0;
    UWORD8 u1_is_cu_noisy = ps_cu_prms->u1_is_cu_noisy;

    ps_final_prms = &ps_ctxt->as_cu_prms[curr_buf_idx];
    ps_nbr_4x4 = &ps_ctxt->as_cu_nbr[curr_buf_idx][0];
    pu1_ecd_data = &ps_final_prms->pu1_cu_coeffs[0];
    pi2_deq_data = &ps_final_prms->pi2_cu_deq_coeffs[0];
    csbf_strd = ps_ctxt->i4_cu_csbf_strd;
    pu1_csbf_buf = &ps_ctxt->au1_cu_csbf[0];
    pred_stride = ps_inter_cand->i4_pred_data_stride;
    recon_stride = cu_size;
    pu1_pred = ps_inter_cand->pu1_pred_data;
    chrm_ctr = 0;
    ecd_data_bytes_cons = 0;
    total_rdopt_cost = 0;
    num_4x4_in_cu = cu_size >> 2;
    recon_func_mode = PRED_MODE_INTER;
    cu_bits = 0;

    /* get the 4x4 level postion of current cu */
    cu_pos_x = cu_pos_x << 1;
    cu_pos_y = cu_pos_y << 1;

    ps_final_prms->u1_is_cu_coded = 0;
    ps_final_prms->u4_cu_sad = 0;

    /* populate the coeffs scan idx */
    ps_ctxt->i4_scan_idx = SCAN_DIAG_UPRIGHT;

#if ENABLE_INTER_ZCU_COST
    /* reset cu not coded cost */
    ps_ctxt->i8_cu_not_coded_cost = 0;

    /* backup copy of cabac states for restoration if zero cu reside rdo wins later */
    memcpy(au1_rdopt_init_ctxt_models, &ps_ctxt->au1_rdopt_init_ctxt_models[0], IHEVC_CAB_CTXT_END);
#endif

    if(ps_cu_analyse->u1_cu_size == 64)
    {
        num_split_flags = 4;
        u1_tu_size = 32;
    }
    else
    {
        num_split_flags = 1;
        u1_tu_size = ps_cu_analyse->u1_cu_size;
    }

    if(1 == ps_final_prms->u1_skip_flag)
    {
        if(64 == cu_size)
        {
            /* TU = CU/2 is set but no trnaform is evaluated  */
            pu1_tu_sz_sft = &gau1_inter_tu_shft_amt[0];
            pu1_tu_posx = &gau1_inter_tu_posx_scl_amt[0];
            pu1_tu_posy = &gau1_inter_tu_posy_scl_amt[0];
        }
        else
        {
            /* TU = CU is set but no trnaform is evaluated  */
            pu1_tu_sz_sft = &u1_skip_tu_sz_sft;
            pu1_tu_posx = &u1_skip_tu_posx;
            pu1_tu_posy = &u1_skip_tu_posy;
        }

        recon_func_mode = PRED_MODE_SKIP;
    }
    /* check for PU part mode being AMP or No AMP */
    else if(ps_final_prms->u1_part_mode < SIZE_2NxnU)
    {
        if((SIZE_2Nx2N == ps_final_prms->u1_part_mode) && (cu_size < 64))
        {
            /* TU= CU is evaluated 2Nx2N inter case */
            pu1_tu_sz_sft = &u1_skip_tu_sz_sft;
            pu1_tu_posx = &u1_skip_tu_posx;
            pu1_tu_posy = &u1_skip_tu_posy;
        }
        else
        {
            /* currently TU= CU/2 is evaluated for all inter case */
            pu1_tu_sz_sft = &gau1_inter_tu_shft_amt[0];
            pu1_tu_posx = &gau1_inter_tu_posx_scl_amt[0];
            pu1_tu_posy = &gau1_inter_tu_posy_scl_amt[0];
        }
    }
    else
    {
        /* for AMP cases one level of TU recurssion is done */
        /* based on oreintation of the partitions           */
        pu1_tu_sz_sft = &gau1_inter_tu_shft_amt_amp[ps_final_prms->u1_part_mode - 4][0];
        pu1_tu_posx = &gau1_inter_tu_posx_scl_amt_amp[ps_final_prms->u1_part_mode - 4][0];
        pu1_tu_posy = &gau1_inter_tu_posy_scl_amt_amp[ps_final_prms->u1_part_mode - 4][0];
    }

    i4_min_trans_size = 4;

    if(ps_ctxt->i1_cu_qp_delta_enable)
    {
        ihevce_update_cu_level_qp_lamda(ps_ctxt, ps_cu_analyse, i4_min_trans_size, 0);
    }

    if(u1_is_cu_noisy && !ps_ctxt->u1_enable_psyRDOPT)
    {
        ps_ctxt->i8_cl_ssd_lambda_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_qf * (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) /
             100.0f);
        ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
            ((float)ps_ctxt->i8_cl_ssd_lambda_chroma_qf *
             (100.0f - RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f);
    }

    u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_QP_WHERE_SPATIAL_SSD_ENABLED) &&
                             (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
                             CONVERT_SSDS_TO_SPATIAL_DOMAIN;

    if(u1_is_cu_noisy || ps_ctxt->u1_enable_psyRDOPT)
    {
        u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_HEVC_QP) &&
                                 CONVERT_SSDS_TO_SPATIAL_DOMAIN;
    }

    if(!u1_compute_spatial_ssd)
    {
        ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 0;
        ps_final_prms->s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
    }
    else
    {
        ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 1;

        if(INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0))
        {
            ps_final_prms->s_recon_datastore.au1_is_chromaRecon_available[0] = 1;
        }
    }

    /* RDOPT copy States :  TU init (best until prev TU) to current */
    memcpy(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
             .s_cabac_ctxt.au1_ctxt_models[0],
        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
        IHEVC_CAB_COEFFX_PREFIX);

    ihevce_tu_tree_init(
        as_tu_nodes,
        cu_size,
        (cu_size == 64) ? !ps_inter_cand->b1_skip_flag : 0,
        ps_inter_cand->b1_skip_flag ? 0 : ps_ctxt->u1_max_inter_tr_depth,
        INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0),
        ps_ctxt->u1_chroma_array_type == 2);

    if(!ps_inter_cand->b1_skip_flag && (ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P3))
    {
        ihevce_tuSplitArray_to_tuTree_mapper(
            as_tu_nodes,
            ps_inter_cand->ai4_tu_split_flag,
            cu_size,
            cu_size,
            MAX(MIN_TU_SIZE, (cu_size >> ps_ctxt->u1_max_inter_tr_depth)),
            MIN(MAX_TU_SIZE, cu_size),
            ps_inter_cand->b1_skip_flag);
    }

    ASSERT(ihevce_tu_tree_coverage_in_cu(as_tu_nodes) == cu_size * cu_size);

#if ENABLE_INTER_ZCU_COST
    ps_ctxt->i8_cu_not_coded_cost = 0;
#endif

    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.pv_src = pv_src;
    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.pv_pred = pu1_pred;
    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.pv_recon =
        ps_final_prms->s_recon_datastore.apv_luma_recon_bufs[0];
    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.i4_src_stride = src_strd;
    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.i4_pred_stride = pred_stride;
    s_buffer_data_for_tu.s_src_pred_rec_buf_luma.i4_recon_stride =
        ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.pv_src = ps_chrm_cu_buf_prms->pu1_curr_src;
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.pv_pred =
        ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[CU_ME_INTRA_PRED_CHROMA_IDX] +
        curr_buf_idx * ((MAX_CTB_SIZE * MAX_CTB_SIZE >> 1) + ((ps_ctxt->u1_chroma_array_type == 2) *
                                                              (MAX_CTB_SIZE * MAX_CTB_SIZE >> 1)));
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.pv_recon =
        ps_final_prms->s_recon_datastore.apv_chroma_recon_bufs[0];
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.i4_src_stride =
        ps_chrm_cu_buf_prms->i4_chrm_src_stride;
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.i4_pred_stride =
        ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[CU_ME_INTRA_PRED_CHROMA_IDX];
    s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.i4_recon_stride =
        ps_final_prms->s_recon_datastore.i4_chromaRecon_stride;
    s_buffer_data_for_tu.ps_nbr_data_buf = ps_nbr_4x4;
    s_buffer_data_for_tu.pi2_deq_data = pi2_deq_data;
    s_buffer_data_for_tu.pi2_deq_data_chroma =
        pi2_deq_data + ps_final_prms->i4_chrm_deq_coeff_strt_idx;
    s_buffer_data_for_tu.i4_nbr_data_buf_stride = num_4x4_in_cu;
    s_buffer_data_for_tu.i4_deq_data_stride = cu_size;
    s_buffer_data_for_tu.i4_deq_data_stride_chroma = cu_size;
    s_buffer_data_for_tu.ppu1_ecd = &pu1_ecd_data;

    if(INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0))
    {
        UWORD8 i;

        UWORD8 *pu1_pred = (UWORD8 *)s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.pv_pred;

        for(i = 0; i < (!!ps_inter_cand->b3_part_size) + 1; i++)
        {
            pu_t *ps_pu;

            WORD32 inter_pu_wd;
            WORD32 inter_pu_ht;

            ps_pu = ps_inter_cand->as_inter_pu + i;

            inter_pu_wd = (ps_pu->b4_wd + 1) << 2; /* cb and cr pixel interleaved */
            inter_pu_ht = ((ps_pu->b4_ht + 1) << 2) >> 1;
            inter_pu_ht <<= (ps_ctxt->u1_chroma_array_type == 2);
            ihevce_chroma_inter_pred_pu(
                &ps_ctxt->s_mc_ctxt,
                ps_pu,
                pu1_pred,
                s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.i4_pred_stride);
            if(!!ps_inter_cand->b3_part_size)
            {
                /* 2Nx__ partion case */
                if(inter_pu_wd == cu_size)
                {
                    pu1_pred +=
                        (inter_pu_ht *
                         s_buffer_data_for_tu.s_src_pred_rec_buf_chroma.i4_pred_stride);
                }

                /* __x2N partion case */
                if(inter_pu_ht == (cu_size >> !(ps_ctxt->u1_chroma_array_type == 2)))
                {
                    pu1_pred += inter_pu_wd;
                }
            }
        }
    }

#if !ENABLE_TOP_DOWN_TU_RECURSION
    total_rdopt_cost = ihevce_tu_tree_selector(
        ps_ctxt,
        as_tu_nodes,
        &s_buffer_data_for_tu,
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
             .s_cabac_ctxt.au1_ctxt_models[0],
        recon_func_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        i4_alpha_stim_multiplier,
        u1_is_cu_noisy,
#endif
        0,
        ps_ctxt->u1_max_inter_tr_depth,
        ps_inter_cand->b3_part_size,
        u1_compute_spatial_ssd);
#else
    total_rdopt_cost = ihevce_topDown_tu_tree_selector(
        ps_ctxt,
        as_tu_nodes,
        &s_buffer_data_for_tu,
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
             .s_cabac_ctxt.au1_ctxt_models[0],
        recon_func_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        i4_alpha_stim_multiplier,
        u1_is_cu_noisy,
#endif
        0,
        ps_ctxt->u1_max_inter_tr_depth,
        ps_inter_cand->b3_part_size,
        INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0),
        u1_compute_spatial_ssd);
#endif

    ps_final_prms->u2_num_tus_in_cu = 0;
    ps_final_prms->u4_cu_luma_res_bits = 0;
    ps_final_prms->u4_cu_sad = 0;
    total_rdopt_cost = 0;
    ecd_data_bytes_cons = 0;
    cu_bits = 0;
#if ENABLE_INTER_ZCU_COST
    ps_ctxt->i8_cu_not_coded_cost = 0;
#endif
    ps_final_prms->u1_is_cu_coded = 0;
    ps_final_prms->u1_cu_size = cu_size;

    ihevce_tu_selector_debriefer(
        as_tu_nodes,
        ps_final_prms,
        &total_rdopt_cost,
#if ENABLE_INTER_ZCU_COST
        &ps_ctxt->i8_cu_not_coded_cost,
#endif
        &ecd_data_bytes_cons,
        &cu_bits,
        &ps_final_prms->u2_num_tus_in_cu,
        ps_ctxt->i4_cu_qp,
        cu_pos_x * 4,
        cu_pos_y * 4,
        INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0),
        (ps_ctxt->u1_chroma_array_type == 2),
        POS_TL);

    if(!(INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0)))
    {
        ps_final_prms->i4_chrm_cu_coeff_strt_idx = ecd_data_bytes_cons;
    }

    /* Modify the cost function for this CU. */
    /* loop in for 8x8 blocks */
    if(ps_ctxt->u1_enable_psyRDOPT)
    {
        UWORD8 *pu1_recon_cu;
        WORD32 recon_stride;
        WORD32 curr_pos_x;
        WORD32 curr_pos_y;
        WORD32 start_index;
        WORD32 num_horz_cu_in_ctb;
        WORD32 had_block_size;

        /* tODO: sreenivasa ctb size has to be used appropriately */
        had_block_size = 8;
        num_horz_cu_in_ctb = 64 / had_block_size;

        curr_pos_x = cu_pos_x << 2; /* pel units */
        curr_pos_y = cu_pos_y << 2; /* pel units */
        recon_stride = ps_final_prms->s_recon_datastore.i4_lumaRecon_stride;
        pu1_recon_cu = ((UWORD8 *)ps_final_prms->s_recon_datastore
                            .apv_luma_recon_bufs[0]);  // already pointing to the current CU recon
        //+ \curr_pos_x + curr_pos_y * recon_stride;

        /* start index to index the source satd of curr cu int he current ctb*/
        start_index =
            (curr_pos_x / had_block_size) + (curr_pos_y / had_block_size) * num_horz_cu_in_ctb;

        {
            total_rdopt_cost += ihevce_psy_rd_cost(
                ps_ctxt->ai4_source_satd_8x8,
                pu1_recon_cu,
                recon_stride,
                1,  //howz stride
                cu_size,
                0,  // pic type
                0,  //layer id
                ps_ctxt->i4_satd_lamda,  // lambda
                start_index,
                ps_ctxt->u1_is_input_data_hbd,
                ps_ctxt->u4_psy_strength,
                &ps_ctxt->s_cmn_opt_func);  // 8 bit
        }
    }

    ps_final_prms->u1_chroma_intra_pred_mode = 4;

    /* update the bytes consumed */
    ps_final_prms->i4_num_bytes_ecd_data = ecd_data_bytes_cons;

    /* store the current cu size to final prms */
    ps_final_prms->u1_cu_size = cu_size;
    /* ------------- Chroma processing -------------- */
    /* Chroma rdopt eval for each luma candidate only for HIGH QUALITY/MEDIUM SPEDD preset*/
    if(ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt &&
       !(INCLUDE_CHROMA_DURING_TU_RECURSION && (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P0)))
    {
        LWORD64 chrm_rdopt_cost;
        WORD32 chrm_rdopt_tu_bits;

        /* Store the current RDOPT cost to enable early exit in chrom_prcs */
        ps_ctxt->as_cu_prms[curr_buf_idx].i8_curr_rdopt_cost = total_rdopt_cost;

        chrm_rdopt_cost = ihevce_chroma_cu_prcs_rdopt(
            ps_ctxt,
            curr_buf_idx,
            0, /* TU mode : Don't care in Inter patrh */
            ps_chrm_cu_buf_prms->pu1_curr_src,
            ps_chrm_cu_buf_prms->i4_chrm_src_stride,
            ps_chrm_cu_buf_prms->pu1_cu_left,
            ps_chrm_cu_buf_prms->pu1_cu_top,
            ps_chrm_cu_buf_prms->pu1_cu_top_left,
            ps_chrm_cu_buf_prms->i4_cu_left_stride,
            (cu_pos_x >> 1),
            (cu_pos_y >> 1),
            &chrm_rdopt_tu_bits,
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy);

#if WEIGH_CHROMA_COST
        chrm_rdopt_cost = (LWORD64)(
            (chrm_rdopt_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
             (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
            CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif

#if CHROMA_RDOPT_ENABLE
        total_rdopt_cost += chrm_rdopt_cost;
#endif
        cu_bits += chrm_rdopt_tu_bits;

        /* during chroma evaluation if skip decision was over written     */
        /* then the current skip candidate is set to a non skip candidate */
        ps_inter_cand->b1_skip_flag = ps_final_prms->u1_skip_flag;

        /* cu bits for chroma residual if chroma rdopt is on       */
        /* if zero_cbf eval is disabled then cu bits will be zero  */
        ps_final_prms->u4_cu_chroma_res_bits = chrm_rdopt_tu_bits;

        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
        {
            /* Early exit : If the current running cost exceeds
            the prev. best mode cost, break */
            if(total_rdopt_cost > prev_best_rdopt_cost)
            {
                return (total_rdopt_cost);
            }
        }
    }
    else
    {}

#if SHRINK_INTER_TUTREE
    /* ------------- Quadtree TU split  optimization ------------  */
    if(ps_final_prms->u1_is_cu_coded)
    {
        ps_final_prms->u2_num_tus_in_cu = ihevce_shrink_inter_tu_tree(
            &ps_final_prms->as_tu_enc_loop[0],
            &ps_final_prms->as_tu_enc_loop_temp_prms[0],
            &ps_final_prms->s_recon_datastore,
            ps_final_prms->u2_num_tus_in_cu,
            (ps_ctxt->u1_chroma_array_type == 2));
    }
#endif

    /* RDOPT copy States :  Best after all luma TUs (and chroma,if enabled)to current */
    COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx]
                .s_cabac_ctxt.au1_ctxt_models[0] +
            IHEVC_CAB_COEFFX_PREFIX,
        &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
        IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

    /* -------- Bit estimate for RD opt -------------- */
    {
        nbr_avail_flags_t s_nbr;
        /*cbf_bits will account for both texture and cbf bits when zero cbf eval flag is 0*/
        WORD32 cbf_bits, header_bits;

        /* get the neighbour availability flags for current cu  */
        ihevce_get_only_nbr_flag(
            &s_nbr,
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            cu_pos_x,
            cu_pos_y,
            (cu_size >> 2),
            (cu_size >> 2));

        /* call the entropy rdo encode to get the bit estimate for current cu */
        header_bits = ihevce_entropy_rdo_encode_cu(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_final_prms,
            (cu_pos_x >> 1), /*  back to 8x8 pel units   */
            (cu_pos_y >> 1), /*  back to 8x8 pel units   */
            cu_size,
            ps_ctxt->u1_disable_intra_eval ? !DISABLE_TOP_SYNC && s_nbr.u1_top_avail
                                           : s_nbr.u1_top_avail,
            s_nbr.u1_left_avail,
            &ps_final_prms->pu1_cu_coeffs[0],
            &cbf_bits);

        cu_bits += header_bits;

        /* cbf bits are excluded from header bits, instead considered as texture bits */
        /* incase if zero cbf eval is disabled then texture bits gets added here */
        ps_final_prms->u4_cu_hdr_bits = (header_bits - cbf_bits);
        ps_final_prms->u4_cu_cbf_bits = cbf_bits;

#if RDOPT_ENABLE
        /* add the cost of coding the header bits */
        total_rdopt_cost +=
            COMPUTE_RATE_COST_CLIP30(header_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

#if ENABLE_INTER_ZCU_COST
        /* If cu is coded, Evaluate not coded cost and check if it improves over coded cost */
        if(ps_final_prms->u1_is_cu_coded && (ZCBF_ENABLE == ps_ctxt->i4_zcbf_rdo_level))
        {
            LWORD64 i8_cu_not_coded_cost = ps_ctxt->i8_cu_not_coded_cost;

            WORD32 is_2nx2n_mergecu = (SIZE_2Nx2N == ps_final_prms->u1_part_mode) &&
                                      (1 == ps_final_prms->as_pu_enc_loop[0].b1_merge_flag);

            cab_ctxt_t *ps_cab_ctxt =
                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[curr_buf_idx].s_cabac_ctxt;

            /* Read header bits generatated after ihevce_entropy_rdo_encode_cu() call  */
            UWORD32 u4_cu_hdr_bits_q12 = ps_cab_ctxt->u4_header_bits_estimated_q12;

            /* account for coding qt_root_cbf = 0 */
            /* First subtract cost for coding as 1 (part of header bits) and then add cost for coding as 0 */
            u4_cu_hdr_bits_q12 += gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 0];
            if(u4_cu_hdr_bits_q12 < gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 1])
                u4_cu_hdr_bits_q12 = 0;
            else
                u4_cu_hdr_bits_q12 -= gau2_ihevce_cabac_bin_to_bits[u1_qtroot_cbf_cabac_model ^ 1];

            /* add the cost of coding the header bits */
            i8_cu_not_coded_cost += COMPUTE_RATE_COST_CLIP30(
                u4_cu_hdr_bits_q12 /* ps_final_prms->u4_cu_hdr_bits */,
                ps_ctxt->i8_cl_ssd_lambda_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

            if(ps_ctxt->u1_enable_psyRDOPT)
            {
                i8_cu_not_coded_cost = total_rdopt_cost + 1;
            }

            /* Evaluate qtroot cbf rdo; exclude 2Nx2N Merge as skip cu is explicitly evaluated */
            if((i8_cu_not_coded_cost <= total_rdopt_cost) && (!is_2nx2n_mergecu))
            {
                WORD32 tx_size;

                /* force cu as not coded and update the cost */
                ps_final_prms->u1_is_cu_coded = 0;
                ps_final_prms->s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
                ps_final_prms->s_recon_datastore.u1_is_lumaRecon_available = 0;

                total_rdopt_cost = i8_cu_not_coded_cost;

                /* reset num TUs to 1 unless cu size id 64 */
                ps_final_prms->u2_num_tus_in_cu = (64 == cu_size) ? 4 : 1;
                trans_size = (64 == cu_size) ? 32 : cu_size;
                GETRANGE(tx_size, trans_size);

                /* reset the bytes consumed */
                ps_final_prms->i4_num_bytes_ecd_data = 0;

                /* reset texture related bits and roll back header bits*/
                ps_final_prms->u4_cu_cbf_bits = 0;
                ps_final_prms->u4_cu_luma_res_bits = 0;
                ps_final_prms->u4_cu_chroma_res_bits = 0;
                ps_final_prms->u4_cu_hdr_bits =
                    (u4_cu_hdr_bits_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >> CABAC_FRAC_BITS_Q;

                /* update cabac model with qtroot cbf = 0 decision */
                ps_cab_ctxt->au1_ctxt_models[IHEVC_CAB_NORES_IDX] =
                    gau1_ihevc_next_state[u1_qtroot_cbf_cabac_model << 1];

                /* restore untouched cabac models for, tusplit, cbfs, texture etc */
                memcpy(
                    &ps_cab_ctxt->au1_ctxt_models[IHEVC_CAB_SPLIT_TFM],
                    &au1_rdopt_init_ctxt_models[IHEVC_CAB_SPLIT_TFM],
                    (IHEVC_CAB_CTXT_END - IHEVC_CAB_SPLIT_TFM));

                /* mark all tus as not coded for final eval */
                for(ctr = 0; ctr < ps_final_prms->u2_num_tus_in_cu; ctr++)
                {
                    WORD32 curr_pos_x = (ctr & 0x1) ? (trans_size >> 2) : 0;
                    WORD32 curr_pos_y = (ctr & 0x2) ? (trans_size >> 2) : 0;

                    nbr_4x4_t *ps_cur_nbr_4x4 =
                        ps_nbr_4x4 + curr_pos_x + (curr_pos_y * num_4x4_in_cu);

                    num_4x4_in_tu = trans_size >> 2;

                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].i2_luma_bytes_consumed = 0;
                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].ai2_cb_bytes_consumed[0] = 0;
                    ps_final_prms->as_tu_enc_loop_temp_prms[ctr].ai2_cr_bytes_consumed[0] = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_y_cbf = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cr_cbf_subtu1 = 0;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b1_cb_cbf_subtu1 = 0;

                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b3_size = tx_size - 3;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_x = cu_pos_x + curr_pos_x;
                    ps_final_prms->as_tu_enc_loop[ctr].s_tu.b4_pos_y = cu_pos_y + curr_pos_y;

                    /* reset cbf for the all 4x4 in TU */
                    {
                        WORD32 i, j;
                        nbr_4x4_t *ps_tmp_4x4;
                        ps_tmp_4x4 = ps_cur_nbr_4x4;

                        for(i = 0; i < num_4x4_in_tu; i++)
                        {
                            for(j = 0; j < num_4x4_in_tu; j++)
                            {
                                ps_tmp_4x4[j].b1_y_cbf = 0;
                            }
                            /* row level update*/
                            ps_tmp_4x4 += num_4x4_in_cu;
                        }
                    }
                }
            }
        }
#endif /* ENABLE_INTER_ZCU_COST */

#endif /* RDOPT_ENABLE */
    }

    return (total_rdopt_cost);
}
#endif

/*!
******************************************************************************
* \if Function name : ihevce_inter_rdopt_cu_mc_mvp \endif
*
* \brief
*    Inter Coding unit funtion which performs MC and MVP calc for RD opt mode
*
* \param[in] ps_ctxt       enc_loop module ctxt pointer
* \param[in] ps_inter_cand pointer to inter candidate structure
* \param[in] cu_size         Current CU size
* \param[in] cu_pos_x        cu position x w.r.t to ctb
* \param[in] cu_pos_y        cu position y w.r.t to ctb
* \param[in] ps_left_nbr_4x4 Left neighbour 4x4 structure pointer
* \param[in] ps_top_nbr_4x4  top neighbour 4x4 structure pointer
* \param[in] ps_topleft_nbr_4x4  top left neighbour 4x4 structure pointer
* \param[in] nbr_4x4_left_strd  left neighbour 4x4 buffer stride
* \param[in] curr_buf_idx Current Buffer index
*
* \return
*    Rdopt cost
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_inter_rdopt_cu_mc_mvp(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    cu_inter_cand_t *ps_inter_cand,
    WORD32 cu_size,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    nbr_4x4_t *ps_left_nbr_4x4,
    nbr_4x4_t *ps_top_nbr_4x4,
    nbr_4x4_t *ps_topleft_nbr_4x4,
    WORD32 nbr_4x4_left_strd,
    WORD32 curr_buf_idx)
{
    /* local variables */
    enc_loop_cu_final_prms_t *ps_final_prms;
    nbr_avail_flags_t s_nbr;
    nbr_4x4_t *ps_nbr_4x4;

    UWORD8 au1_is_top_used[2][MAX_MVP_LIST_CAND];
    UWORD8 *pu1_pred;
    WORD32 rdopt_cost;
    WORD32 ctr;
    WORD32 num_cu_part;
    WORD32 inter_pu_wd;
    WORD32 inter_pu_ht;
    WORD32 pred_stride;

    /* get the pointers based on curbuf idx */
    ps_nbr_4x4 = &ps_ctxt->as_cu_nbr[curr_buf_idx][0];
    ps_final_prms = &ps_ctxt->as_cu_prms[curr_buf_idx];
    pu1_pred = ps_inter_cand->pu1_pred_data;

    pred_stride = ps_inter_cand->i4_pred_data_stride;

    /* store the partition mode in final prms */
    ps_final_prms->u1_part_mode = ps_inter_cand->b3_part_size;

    /* since encoder does not support NXN part type */
    /* num parts can be either 1 or 2 only          */
    ASSERT(SIZE_NxN != ps_inter_cand->b3_part_size);

    num_cu_part = (SIZE_2Nx2N != ps_inter_cand->b3_part_size) + 1;

    /* get the 4x4 level position of current cu */
    cu_pos_x = cu_pos_x << 1;
    cu_pos_y = cu_pos_y << 1;

    /* populate cu level params */
    ps_final_prms->u1_intra_flag = PRED_MODE_INTER;
    ps_final_prms->u2_num_pus_in_cu = num_cu_part;

    /* run a loop over all the partitons in cu */
    for(ctr = 0; ctr < num_cu_part; ctr++)
    {
        pu_mv_t as_pred_mv[MAX_MVP_LIST_CAND];
        pu_t *ps_pu;
        WORD32 skip_or_merge_flag;
        UWORD8 u1_use_mvp_from_top_row;

        ps_pu = &ps_inter_cand->as_inter_pu[ctr];

        /* IF AMP then each partitions can have diff wd ht */
        inter_pu_wd = (ps_pu->b4_wd + 1) << 2;
        inter_pu_ht = (ps_pu->b4_ht + 1) << 2;

        /* populate reference pic buf id for bs compute */

        /* L0 */
        if(-1 != ps_pu->mv.i1_l0_ref_idx)
        {
            ps_pu->mv.i1_l0_ref_pic_buf_id =
                ps_ctxt->s_mv_pred_ctxt.ps_ref_list[0][ps_pu->mv.i1_l0_ref_idx]->i4_buf_id;
        }

        /* L1 */
        if(-1 != ps_pu->mv.i1_l1_ref_idx)
        {
            ps_pu->mv.i1_l1_ref_pic_buf_id =
                ps_ctxt->s_mv_pred_ctxt.ps_ref_list[1][ps_pu->mv.i1_l1_ref_idx]->i4_buf_id;
        }

        /* SKIP or merge check for every part */
        skip_or_merge_flag = ps_inter_cand->b1_skip_flag | ps_pu->b1_merge_flag;

        /* ----------- MV Prediction ----------------- */
        if(0 == skip_or_merge_flag)
        {
            /* get the neighbour availability flags */
            ihevce_get_only_nbr_flag(
                &s_nbr,
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x,
                cu_pos_y,
                inter_pu_wd >> 2,
                inter_pu_ht >> 2);

            if(ps_ctxt->u1_disable_intra_eval && DISABLE_TOP_SYNC && (ps_pu->b4_pos_y == 0))
            {
                u1_use_mvp_from_top_row = 0;
            }
            else
            {
                u1_use_mvp_from_top_row = 1;
            }

            if(!u1_use_mvp_from_top_row)
            {
                if(s_nbr.u1_top_avail || s_nbr.u1_top_lt_avail || s_nbr.u1_top_rt_avail)
                {
                    if(!s_nbr.u1_left_avail && !s_nbr.u1_bot_lt_avail)
                    {
                        WORD32 curr_cu_pos_in_row, cu_top_right_offset, cu_top_right_dep_pos;

                        /* Ensure Top Right Sync */
                        if(!ps_ctxt->u1_use_top_at_ctb_boundary)
                        {
                            curr_cu_pos_in_row =
                                ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_x + (cu_pos_x << 2);

                            if(ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_y == 0)
                            {
                                /* No wait for 1st row */
                                cu_top_right_offset = -(MAX_CTB_SIZE);
                                {
                                    ihevce_tile_params_t *ps_col_tile_params =
                                        ((ihevce_tile_params_t *)ps_ctxt->pv_tile_params_base +
                                         ps_ctxt->i4_tile_col_idx);

                                    /* No wait for 1st row */
                                    cu_top_right_offset =
                                        -(ps_col_tile_params->i4_first_sample_x + (MAX_CTB_SIZE));
                                }
                                cu_top_right_dep_pos = 0;
                            }
                            else
                            {
                                cu_top_right_offset = (cu_size) + 4;
                                cu_top_right_dep_pos =
                                    (ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_y >> 6) - 1;
                            }

                            ihevce_dmgr_chk_row_row_sync(
                                ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right,
                                curr_cu_pos_in_row,
                                cu_top_right_offset,
                                cu_top_right_dep_pos,
                                ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                                ps_ctxt->thrd_id);
                        }

                        u1_use_mvp_from_top_row = 1;
                    }
                    else
                    {
                        s_nbr.u1_top_avail = 0;
                        s_nbr.u1_top_lt_avail = 0;
                        s_nbr.u1_top_rt_avail = 0;
                    }
                }
                else
                {
                    u1_use_mvp_from_top_row = 1;
                }
            }
            /* Call the MV prediction module to get MVP */
            ihevce_mv_pred(
                &ps_ctxt->s_mv_pred_ctxt,
                ps_top_nbr_4x4,
                ps_left_nbr_4x4,
                ps_topleft_nbr_4x4,
                nbr_4x4_left_strd,
                &s_nbr,
                NULL, /* colocated MV */
                ps_pu,
                &as_pred_mv[0],
                au1_is_top_used);
        }

        /* store the nbr 4x4 structure */
        ps_nbr_4x4->b1_skip_flag = ps_inter_cand->b1_skip_flag;
        ps_nbr_4x4->b1_intra_flag = 0;
        ps_nbr_4x4->b1_pred_l0_flag = 0;
        ps_nbr_4x4->b1_pred_l1_flag = 0;

        /* DC is default mode for inter cu, required for intra mode signalling */
        ps_nbr_4x4->b6_luma_intra_mode = 1;

        /* copy the motion vectors to neighbour structure */
        ps_nbr_4x4->mv = ps_pu->mv;

        /* copy the PU to final out pu */
        ps_final_prms->as_pu_enc_loop[ctr] = *ps_pu;

        /* copy the PU to chroma */
        ps_final_prms->as_pu_chrm_proc[ctr] = *ps_pu;

        /* store the skip flag to final prms */
        ps_final_prms->u1_skip_flag = ps_inter_cand->b1_skip_flag;

        /* MVP index & MVD calc is gated on skip/merge flag */
        if(0 == skip_or_merge_flag)
        {
            /* calculate the MVDs and popluate the MVP idx for L0 */
            if((PRED_BI == ps_pu->b2_pred_mode) || (PRED_L0 == ps_pu->b2_pred_mode))
            {
                WORD32 idx0_cost, idx1_cost;

                /* calculate the ABS mvd for cand 0 */
                idx0_cost = abs(ps_pu->mv.s_l0_mv.i2_mvx - as_pred_mv[0].s_l0_mv.i2_mvx);
                idx0_cost += abs(ps_pu->mv.s_l0_mv.i2_mvy - as_pred_mv[0].s_l0_mv.i2_mvy);

                /* calculate the ABS mvd for cand 1 */
                if(u1_use_mvp_from_top_row)
                {
                    idx1_cost = abs(ps_pu->mv.s_l0_mv.i2_mvx - as_pred_mv[1].s_l0_mv.i2_mvx);
                    idx1_cost += abs(ps_pu->mv.s_l0_mv.i2_mvy - as_pred_mv[1].s_l0_mv.i2_mvy);
                }
                else
                {
                    idx1_cost = INT_MAX;
                }

                /* based on the least cost choose the mvp idx */
                if(idx0_cost <= idx1_cost)
                {
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l0_mv.i2_mvx -=
                        as_pred_mv[0].s_l0_mv.i2_mvx;
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l0_mv.i2_mvy -=
                        as_pred_mv[0].s_l0_mv.i2_mvy;

                    ps_final_prms->as_pu_enc_loop[ctr].b1_l0_mvp_idx = 0;
                }
                else
                {
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l0_mv.i2_mvx -=
                        as_pred_mv[1].s_l0_mv.i2_mvx;
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l0_mv.i2_mvy -=
                        as_pred_mv[1].s_l0_mv.i2_mvy;

                    ps_final_prms->as_pu_enc_loop[ctr].b1_l0_mvp_idx = 1;
                }

                /* set the pred l0 flag for neighbour storage */
                ps_nbr_4x4->b1_pred_l0_flag = 1;
            }
            /* calculate the MVDs and popluate the MVP idx for L1 */
            if((PRED_BI == ps_pu->b2_pred_mode) || (PRED_L1 == ps_pu->b2_pred_mode))
            {
                WORD32 idx0_cost, idx1_cost;

                /* calculate the ABS mvd for cand 0 */
                idx0_cost = abs(ps_pu->mv.s_l1_mv.i2_mvx - as_pred_mv[0].s_l1_mv.i2_mvx);
                idx0_cost += abs(ps_pu->mv.s_l1_mv.i2_mvy - as_pred_mv[0].s_l1_mv.i2_mvy);

                /* calculate the ABS mvd for cand 1 */
                if(u1_use_mvp_from_top_row)
                {
                    idx1_cost = abs(ps_pu->mv.s_l1_mv.i2_mvx - as_pred_mv[1].s_l1_mv.i2_mvx);
                    idx1_cost += abs(ps_pu->mv.s_l1_mv.i2_mvy - as_pred_mv[1].s_l1_mv.i2_mvy);
                }
                else
                {
                    idx1_cost = INT_MAX;
                }

                /* based on the least cost choose the mvp idx */
                if(idx0_cost <= idx1_cost)
                {
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l1_mv.i2_mvx -=
                        as_pred_mv[0].s_l1_mv.i2_mvx;
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l1_mv.i2_mvy -=
                        as_pred_mv[0].s_l1_mv.i2_mvy;

                    ps_final_prms->as_pu_enc_loop[ctr].b1_l1_mvp_idx = 0;
                }
                else
                {
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l1_mv.i2_mvx -=
                        as_pred_mv[1].s_l1_mv.i2_mvx;
                    ps_final_prms->as_pu_enc_loop[ctr].mv.s_l1_mv.i2_mvy -=
                        as_pred_mv[1].s_l1_mv.i2_mvy;

                    ps_final_prms->as_pu_enc_loop[ctr].b1_l1_mvp_idx = 1;
                }

                /* set the pred l1 flag for neighbour storage */
                ps_nbr_4x4->b1_pred_l1_flag = 1;
            }

            /* set the merge flag to 0 */
            ps_final_prms->as_pu_enc_loop[ctr].b1_merge_flag = 0;
            ps_final_prms->as_pu_enc_loop[ctr].b3_merge_idx = 0;
        }
        else
        {
            /* copy the merge index from candidate */
            ps_final_prms->as_pu_enc_loop[ctr].b1_merge_flag = ps_pu->b1_merge_flag;

            ps_final_prms->as_pu_enc_loop[ctr].b3_merge_idx = ps_pu->b3_merge_idx;

            if((PRED_BI == ps_pu->b2_pred_mode) || (PRED_L0 == ps_pu->b2_pred_mode))
            {
                /* set the pred l0 flag for neighbour storage */
                ps_nbr_4x4->b1_pred_l0_flag = 1;
            }

            /* calculate the MVDs and popluate the MVP idx for L1 */
            if((PRED_BI == ps_pu->b2_pred_mode) || (PRED_L1 == ps_pu->b2_pred_mode))
            {
                /* set the pred l1 flag for neighbour storage */
                ps_nbr_4x4->b1_pred_l1_flag = 1;
            }
        }

        /* RD opt cost computation is part of cu_ntu func hence here it is set to 0 */
        rdopt_cost = 0;

        /* copy the MV to colocated Mv structure */
        ps_final_prms->as_col_pu_enc_loop[ctr].s_l0_mv = ps_pu->mv.s_l0_mv;
        ps_final_prms->as_col_pu_enc_loop[ctr].s_l1_mv = ps_pu->mv.s_l1_mv;
        ps_final_prms->as_col_pu_enc_loop[ctr].i1_l0_ref_idx = ps_pu->mv.i1_l0_ref_idx;
        ps_final_prms->as_col_pu_enc_loop[ctr].i1_l1_ref_idx = ps_pu->mv.i1_l1_ref_idx;
        ps_final_prms->as_col_pu_enc_loop[ctr].b2_pred_mode = ps_pu->b2_pred_mode;
        ps_final_prms->as_col_pu_enc_loop[ctr].b1_intra_flag = 0;

        /* replicate neighbour 4x4 strcuture for entire partition */
        {
            WORD32 i, j;
            nbr_4x4_t *ps_tmp_4x4;

            ps_tmp_4x4 = ps_nbr_4x4;

            for(i = 0; i < (inter_pu_ht >> 2); i++)
            {
                for(j = 0; j < (inter_pu_wd >> 2); j++)
                {
                    ps_tmp_4x4[j] = *ps_nbr_4x4;
                }
                /* row level update*/
                ps_tmp_4x4 += (cu_size >> 2);
            }
        }
        /* set the neighbour map to 1 */
        ihevce_set_inter_nbr_map(
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            cu_pos_x,
            cu_pos_y,
            (inter_pu_wd >> 2),
            (inter_pu_ht >> 2),
            1);
        /* ----------- Motion Compensation for Luma ----------- */
#if !ENABLE_MIXED_INTER_MODE_EVAL
        {
            IV_API_CALL_STATUS_T valid_mv_cand;

            /*If the inter candidate is neither merge cand nor skip cand
            then calculate the mc.*/
            if(0 == skip_or_merge_flag || (ps_ctxt->u1_high_speed_cu_dec_on))
            {
                valid_mv_cand =
                    ihevce_luma_inter_pred_pu(&ps_ctxt->s_mc_ctxt, ps_pu, pu1_pred, pred_stride, 0);

                /* assert if the MC is given a valid mv candidate */
                ASSERT(valid_mv_cand == IV_SUCCESS);
            }
        }
#endif
        if((2 == num_cu_part) && (0 == ctr))
        {
            /* 2Nx__ partion case */
            if(inter_pu_wd == cu_size)
            {
                cu_pos_y += (inter_pu_ht >> 2);
                pu1_pred += (inter_pu_ht * pred_stride);
                ps_nbr_4x4 += (inter_pu_ht >> 2) * (cu_size >> 2);
                ps_left_nbr_4x4 += (inter_pu_ht >> 2) * nbr_4x4_left_strd;
                ps_top_nbr_4x4 = ps_nbr_4x4 - (cu_size >> 2);
                ps_topleft_nbr_4x4 = ps_left_nbr_4x4 - nbr_4x4_left_strd;
            }

            /* __x2N partion case */
            if(inter_pu_ht == cu_size)
            {
                cu_pos_x += (inter_pu_wd >> 2);
                pu1_pred += inter_pu_wd;
                ps_nbr_4x4 += (inter_pu_wd >> 2);
                ps_left_nbr_4x4 = ps_nbr_4x4 - 1;
                ps_top_nbr_4x4 += (inter_pu_wd >> 2);
                ps_topleft_nbr_4x4 = ps_top_nbr_4x4 - 1;
                nbr_4x4_left_strd = (cu_size >> 2);
            }
        }
    }

    return (rdopt_cost);
}

/*!
******************************************************************************
* \if Function name : ihevce_intra_chroma_pred_mode_selector \endif
*
* \brief
*    Coding unit processing function for chroma special modes (Non-Luma modes)
*
* \param[in] ps_ctxt       enc_loop module ctxt pointer
* \param[in] ps_chrm_cu_buf_prms    ctxt having chroma related prms
* \param[in] ps_cu_analyse      pointer to cu analyse
* \param[in] rd_opt_curr_idx    index in the array of RDopt params
* \param[in] tu_mode            TU_EQ_CU or other case
*
* \return
*    Stores the best SATD mode, it's RDOPT cost, CABAC state, TU bits
*
* \author
*  Ittiam
*
*****************************************************************************
*/
UWORD8 ihevce_distortion_based_intra_chroma_mode_selector(
    cu_analyse_t *ps_cu_analyse,
    ihevc_intra_pred_chroma_ref_substitution_ft *pf_ref_substitution,
    pf_intra_pred *ppf_chroma_ip,
    pf_res_trans_luma_had_chroma *ppf_resd_trns_had,
    UWORD8 *pu1_src,
    WORD32 i4_src_stride,
    UWORD8 *pu1_pred,
    WORD32 i4_pred_stride,
    UWORD8 *pu1_ctb_nbr_map,
    WORD32 i4_nbr_map_strd,
    UWORD8 *pu1_ref_sub_out,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
    UWORD8 u1_trans_size,
    UWORD8 u1_trans_idx,
    UWORD8 u1_num_tus_in_cu,
    UWORD8 u1_num_4x4_luma_blks_in_tu,
    UWORD8 u1_enable_psyRDOPT,
    UWORD8 u1_is_422)
{
    UWORD8 u1_chrm_mode;
    UWORD8 ctr;
    WORD32 i4_subtu_idx;

    WORD32 i = 0;
    UWORD8 u1_chrm_modes[4] = { 0, 1, 10, 26 };
    WORD32 i4_satd_had[4] = { 0 };
    WORD32 i4_best_satd_had = INT_MAX;
    UWORD8 u1_cu_pos_x = (ps_cu_analyse->b3_cu_pos_x << 1);
    UWORD8 u1_cu_pos_y = (ps_cu_analyse->b3_cu_pos_y << 1);
    WORD32 i4_num_sub_tus = u1_is_422 + 1;
    UWORD8 u1_best_chrm_mode = 0;

    /* Get the best satd among all possible modes */
    for(i = 0; i < 4; i++)
    {
        WORD32 left_strd = i4_src_stride;

        u1_chrm_mode = (u1_is_422 == 1) ? gau1_chroma422_intra_angle_mapping[u1_chrm_modes[i]]
                                        : u1_chrm_modes[i];

        /* loop based on num tus in a cu */
        for(ctr = 0; ctr < u1_num_tus_in_cu; ctr++)
        {
            WORD32 luma_nbr_flags;
            WORD32 chrm_pred_func_idx;

            WORD32 i4_trans_size_m2 = u1_trans_size << 1;
            UWORD8 *pu1_tu_src = pu1_src + ((ctr & 1) * i4_trans_size_m2) +
                                 (((ctr > 1) * u1_trans_size * i4_src_stride) << u1_is_422);
            UWORD8 *pu1_tu_pred = pu1_pred + ((ctr & 1) * i4_trans_size_m2) +
                                  (((ctr > 1) * u1_trans_size * i4_pred_stride) << u1_is_422);
            WORD32 i4_curr_tu_pos_x = u1_cu_pos_x + ((ctr & 1) * u1_num_4x4_luma_blks_in_tu);
            WORD32 i4_curr_tu_pos_y = u1_cu_pos_y + ((ctr > 1) * u1_num_4x4_luma_blks_in_tu);

            luma_nbr_flags = ihevce_get_nbr_intra_mxn_tu(
                pu1_ctb_nbr_map,
                i4_nbr_map_strd,
                i4_curr_tu_pos_x,
                i4_curr_tu_pos_y,
                u1_num_4x4_luma_blks_in_tu,
                u1_num_4x4_luma_blks_in_tu);

            for(i4_subtu_idx = 0; i4_subtu_idx < i4_num_sub_tus; i4_subtu_idx++)
            {
                WORD32 nbr_flags;

                UWORD8 *pu1_cur_src =
                    pu1_tu_src + ((i4_subtu_idx == 1) * u1_trans_size * i4_src_stride);
                UWORD8 *pu1_cur_pred =
                    pu1_tu_pred + ((i4_subtu_idx == 1) * u1_trans_size * i4_pred_stride);
                UWORD8 *pu1_left = pu1_cur_src - 2;
                UWORD8 *pu1_top = pu1_cur_src - i4_src_stride;
                UWORD8 *pu1_top_left = pu1_top - 2;

                nbr_flags = ihevce_get_intra_chroma_tu_nbr(
                    luma_nbr_flags, i4_subtu_idx, u1_trans_size, u1_is_422);

                /* call the chroma reference array substitution */
                pf_ref_substitution(
                    pu1_top_left,
                    pu1_top,
                    pu1_left,
                    left_strd,
                    u1_trans_size,
                    nbr_flags,
                    pu1_ref_sub_out,
                    1);

                /* use the look up to get the function idx */
                chrm_pred_func_idx = g_i4_ip_funcs[u1_chrm_mode];

                /* call the intra prediction function */
                ppf_chroma_ip[chrm_pred_func_idx](
                    pu1_ref_sub_out, 1, pu1_cur_pred, i4_pred_stride, u1_trans_size, u1_chrm_mode);

                if(!u1_is_cu_noisy || !i4_alpha_stim_multiplier)
                {
                    /* compute Hadamard-transform satd : Cb */
                    i4_satd_had[i] += ppf_resd_trns_had[u1_trans_idx - 1](
                        pu1_cur_src, i4_src_stride, pu1_cur_pred, i4_pred_stride, NULL, 0);

                    /* compute Hadamard-transform satd : Cr */
                    i4_satd_had[i] += ppf_resd_trns_had[u1_trans_idx - 1](
                        pu1_cur_src + 1, i4_src_stride, pu1_cur_pred + 1, i4_pred_stride, NULL, 0);
                }
                else
                {
                    WORD32 i4_satd;

                    /* compute Hadamard-transform satd : Cb */
                    i4_satd = ppf_resd_trns_had[u1_trans_idx - 1](
                        pu1_cur_src, i4_src_stride, pu1_cur_pred, i4_pred_stride, NULL, 0);

                    i4_satd = ihevce_inject_stim_into_distortion(
                        pu1_cur_src,
                        i4_src_stride,
                        pu1_cur_pred,
                        i4_pred_stride,
                        i4_satd,
                        i4_alpha_stim_multiplier,
                        u1_trans_size,
                        0,
                        u1_enable_psyRDOPT,
                        U_PLANE);

                    i4_satd_had[i] += i4_satd;

                    /* compute Hadamard-transform satd : Cr */
                    i4_satd = ppf_resd_trns_had[u1_trans_idx - 1](
                        pu1_cur_src + 1, i4_src_stride, pu1_cur_pred + 1, i4_pred_stride, NULL, 0);

                    i4_satd = ihevce_inject_stim_into_distortion(
                        pu1_cur_src,
                        i4_src_stride,
                        pu1_cur_pred,
                        i4_pred_stride,
                        i4_satd,
                        i4_alpha_stim_multiplier,
                        u1_trans_size,
                        0,
                        u1_enable_psyRDOPT,
                        V_PLANE);

                    i4_satd_had[i] += i4_satd;
                }
            }

            /* set the neighbour map to 1 */
            ihevce_set_nbr_map(
                pu1_ctb_nbr_map,
                i4_nbr_map_strd,
                i4_curr_tu_pos_x,
                i4_curr_tu_pos_y,
                u1_num_4x4_luma_blks_in_tu,
                1);
        }

        /* set the neighbour map to 0 */
        ihevce_set_nbr_map(
            pu1_ctb_nbr_map,
            i4_nbr_map_strd,
            (ps_cu_analyse->b3_cu_pos_x << 1),
            (ps_cu_analyse->b3_cu_pos_y << 1),
            (ps_cu_analyse->u1_cu_size >> 2),
            0);

        /* Get the least SATD and corresponding mode */
        if(i4_best_satd_had > i4_satd_had[i])
        {
            i4_best_satd_had = i4_satd_had[i];
            u1_best_chrm_mode = u1_chrm_mode;
        }
    }

    return u1_best_chrm_mode;
}

void ihevce_intra_chroma_pred_mode_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms,
    cu_analyse_t *ps_cu_analyse,
    WORD32 rd_opt_curr_idx,
    WORD32 tu_mode,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy)
{
    chroma_intra_satd_ctxt_t *ps_chr_intra_satd_ctxt;

    ihevc_intra_pred_chroma_ref_substitution_ft *ihevc_intra_pred_chroma_ref_substitution_fptr;

    UWORD8 *pu1_pred;
    WORD32 trans_size;
    WORD32 num_tus_in_cu;
    WORD32 pred_strd;
    WORD32 ctr;
    WORD32 i4_subtu_idx;
    WORD32 i4_num_sub_tus;
    WORD32 trans_idx;
    WORD32 scan_idx;
    WORD32 num_4x4_luma_in_tu;
    WORD32 cu_pos_x;
    WORD32 cu_pos_y;

    recon_datastore_t *aps_recon_datastore[2] = { &ps_ctxt->as_cu_prms[0].s_recon_datastore,
                                                  &ps_ctxt->as_cu_prms[1].s_recon_datastore };

    LWORD64 chrm_cod_cost = 0;
    WORD32 chrm_tu_bits = 0;
    WORD32 best_chrm_mode = DM_CHROMA_IDX;
    UWORD8 *pu1_chrm_src = ps_chrm_cu_buf_prms->pu1_curr_src;
    WORD32 chrm_src_stride = ps_chrm_cu_buf_prms->i4_chrm_src_stride;
    UWORD8 *pu1_cu_left = ps_chrm_cu_buf_prms->pu1_cu_left;
    UWORD8 *pu1_cu_top = ps_chrm_cu_buf_prms->pu1_cu_top;
    UWORD8 *pu1_cu_top_left = ps_chrm_cu_buf_prms->pu1_cu_top_left;
    WORD32 cu_left_stride = ps_chrm_cu_buf_prms->i4_cu_left_stride;
    WORD32 cu_size = ps_cu_analyse->u1_cu_size;
    WORD32 i4_perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq;
    WORD32 i4_perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);

    ihevc_intra_pred_chroma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_chroma_ref_substitution_fptr;
    i4_num_sub_tus = (u1_is_422 == 1) + 1;

#if DISABLE_RDOQ_INTRA
    i4_perform_rdoq = 0;
#endif

    if(TU_EQ_CU == tu_mode)
    {
        num_tus_in_cu = 1;
        trans_size = cu_size >> 1;
        num_4x4_luma_in_tu = trans_size >> 1; /*at luma level*/
        ps_chr_intra_satd_ctxt = &ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[tu_mode];
    }
    else
    {
        num_tus_in_cu = 4;
        trans_size = cu_size >> 2;
        num_4x4_luma_in_tu = trans_size >> 1; /*at luma level*/

        /* For 8x8 CU only one TU */
        if(MIN_TU_SIZE > trans_size)
        {
            trans_size = MIN_TU_SIZE;
            num_tus_in_cu = 1;
            /* chroma nbr avail. is derived based on luma.
            for 4x4 chrm use 8x8 luma's size */
            num_4x4_luma_in_tu = num_4x4_luma_in_tu << 1;
        }

        ps_chr_intra_satd_ctxt = &ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[tu_mode];
    }

    /* Can't be TU_EQ_SUBCU case */
    ASSERT(TU_EQ_SUBCU != tu_mode);

    /* translate the transform size to index */
    trans_idx = trans_size >> 2;

    pu1_pred = (UWORD8 *)ps_chr_intra_satd_ctxt->pv_pred_data;

    pred_strd = ps_chr_intra_satd_ctxt->i4_pred_stride;

    /* for 16x16 cases */
    if(16 == trans_size)
    {
        trans_idx = 3;
    }

    best_chrm_mode = ihevce_distortion_based_intra_chroma_mode_selector(
        ps_cu_analyse,
        ihevc_intra_pred_chroma_ref_substitution_fptr,
        ps_ctxt->apf_chrm_ip,
        ps_ctxt->apf_chrm_resd_trns_had,
        pu1_chrm_src,
        chrm_src_stride,
        pu1_pred,
        pred_strd,
        ps_ctxt->pu1_ctb_nbr_map,
        ps_ctxt->i4_nbr_map_strd,
        (UWORD8 *)ps_ctxt->pv_ref_sub_out,
        i4_alpha_stim_multiplier,
        u1_is_cu_noisy,
        trans_size,
        trans_idx,
        num_tus_in_cu,
        num_4x4_luma_in_tu,
        ps_ctxt->u1_enable_psyRDOPT,
        u1_is_422);

    /* Store the best chroma mode */
    ps_chr_intra_satd_ctxt->u1_best_cr_mode = best_chrm_mode;

    /* evaluate RDOPT cost for the Best mode */
    {
        WORD32 i4_subtu_pos_x;
        WORD32 i4_subtu_pos_y;
        UWORD8 u1_compute_spatial_ssd;

        WORD32 ai4_total_bytes_offset_cb[2] = { 0, 0 };
        WORD32 ai4_total_bytes_offset_cr[2] = { 0, 0 };
        /* State for prefix bin of chroma intra pred mode before CU encode */
        UWORD8 u1_chroma_intra_mode_prefix_state =
            ps_ctxt->au1_rdopt_init_ctxt_models[IHEVC_CAB_CHROMA_PRED_MODE];
        WORD32 luma_trans_size = trans_size << 1;
        WORD32 calc_recon = 0;
        UWORD8 *pu1_left = pu1_cu_left;
        UWORD8 *pu1_top = pu1_cu_top;
        UWORD8 *pu1_top_left = pu1_cu_top_left;
        WORD32 left_strd = cu_left_stride;

        if(ps_ctxt->i1_cu_qp_delta_enable)
        {
            ihevce_update_cu_level_qp_lamda(ps_ctxt, ps_cu_analyse, luma_trans_size, 1);
        }

        u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_QP_WHERE_SPATIAL_SSD_ENABLED) &&
                                 (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
                                 CONVERT_SSDS_TO_SPATIAL_DOMAIN;

        if(u1_is_cu_noisy || ps_ctxt->u1_enable_psyRDOPT)
        {
            u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_HEVC_QP) &&
                                     CONVERT_SSDS_TO_SPATIAL_DOMAIN;
        }

        /* get the 4x4 level postion of current cu */
        cu_pos_x = (ps_cu_analyse->b3_cu_pos_x << 1);
        cu_pos_y = (ps_cu_analyse->b3_cu_pos_y << 1);

        calc_recon = !u1_compute_spatial_ssd && ((4 == num_tus_in_cu) || (u1_is_422 == 1));

        if(calc_recon || u1_compute_spatial_ssd)
        {
            aps_recon_datastore[0]->au1_is_chromaRecon_available[1 + (num_tus_in_cu > 1)] = 1;
            aps_recon_datastore[1]->au1_is_chromaRecon_available[1 + (num_tus_in_cu > 1)] = 1;
        }
        else
        {
            aps_recon_datastore[0]->au1_is_chromaRecon_available[1 + (num_tus_in_cu > 1)] = 0;
            aps_recon_datastore[1]->au1_is_chromaRecon_available[1 + (num_tus_in_cu > 1)] = 0;
        }

        /* loop based on num tus in a cu */
        for(ctr = 0; ctr < num_tus_in_cu; ctr++)
        {
            WORD16 *pi2_cur_deq_data_cb;
            WORD16 *pi2_cur_deq_data_cr;

            WORD32 deq_data_strd = ps_chr_intra_satd_ctxt->i4_iq_buff_stride;
            WORD32 luma_nbr_flags = 0;

            luma_nbr_flags = ihevce_get_nbr_intra_mxn_tu(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ctr & 1) * (luma_trans_size >> 2) + cu_pos_x,
                (ctr > 1) * (luma_trans_size >> 2) + cu_pos_y,
                (luma_trans_size >> 2),
                (luma_trans_size >> 2));

            for(i4_subtu_idx = 0; i4_subtu_idx < i4_num_sub_tus; i4_subtu_idx++)
            {
                WORD32 cbf, num_bytes;
                LWORD64 trans_ssd_u, trans_ssd_v;
                UWORD8 u1_is_recon_available;

                WORD32 trans_size_m2 = trans_size << 1;
                UWORD8 *pu1_cur_src = pu1_chrm_src + ((ctr & 1) * trans_size_m2) +
                                      (((ctr > 1) * trans_size * chrm_src_stride) << u1_is_422) +
                                      (i4_subtu_idx * trans_size * chrm_src_stride);
                UWORD8 *pu1_cur_pred = pu1_pred + ((ctr & 1) * trans_size_m2) +
                                       (((ctr > 1) * trans_size * pred_strd) << u1_is_422) +
                                       (i4_subtu_idx * trans_size * pred_strd);
                WORD32 i4_recon_stride = aps_recon_datastore[0]->i4_chromaRecon_stride;
                UWORD8 *pu1_cur_recon = ((UWORD8 *)aps_recon_datastore[0]
                                             ->apv_chroma_recon_bufs[1 + (num_tus_in_cu > 1)]) +
                                        ((ctr & 1) * trans_size_m2) +
                                        (((ctr > 1) * trans_size * i4_recon_stride) << u1_is_422) +
                                        (i4_subtu_idx * trans_size * i4_recon_stride);

                /* Use Chroma coeff/iq buf of the cur. intra cand. Not rememb.
                chroma coeff/iq for high quality intra SATD special modes. Will
                be over written by coeff of luma mode in chroma_rdopt call */
                UWORD8 *pu1_ecd_data_cb =
                    &ps_chr_intra_satd_ctxt->au1_scan_coeff_cb[i4_subtu_idx][0];
                UWORD8 *pu1_ecd_data_cr =
                    &ps_chr_intra_satd_ctxt->au1_scan_coeff_cr[i4_subtu_idx][0];

                WORD32 chrm_pred_func_idx = 0;
                LWORD64 curr_cb_cod_cost = 0;
                LWORD64 curr_cr_cod_cost = 0;
                WORD32 nbr_flags = 0;

                i4_subtu_pos_x = (((ctr & 1) * trans_size_m2) >> 2);
                i4_subtu_pos_y = (((ctr > 1) * trans_size) >> (!u1_is_422 + 1)) +
                                 ((i4_subtu_idx * trans_size) >> 2);
                pi2_cur_deq_data_cb = &ps_chr_intra_satd_ctxt->ai2_iq_data_cb[0] +
                                      ((ctr & 1) * trans_size) +
                                      (((ctr > 1) * trans_size * deq_data_strd) << u1_is_422) +
                                      (i4_subtu_idx * trans_size * deq_data_strd);
                pi2_cur_deq_data_cr = &ps_chr_intra_satd_ctxt->ai2_iq_data_cr[0] +
                                      ((ctr & 1) * trans_size) +
                                      (((ctr > 1) * trans_size * deq_data_strd) << u1_is_422) +
                                      (i4_subtu_idx * trans_size * deq_data_strd);

                /* left cu boundary */
                if(0 == i4_subtu_pos_x)
                {
                    left_strd = cu_left_stride;
                    pu1_left = pu1_cu_left + (i4_subtu_pos_y << 2) * left_strd;
                }
                else
                {
                    pu1_left = pu1_cur_recon - 2;
                    left_strd = i4_recon_stride;
                }

                /* top cu boundary */
                if(0 == i4_subtu_pos_y)
                {
                    pu1_top = pu1_cu_top + (i4_subtu_pos_x << 2);
                }
                else
                {
                    pu1_top = pu1_cur_recon - i4_recon_stride;
                }

                /* by default top left is set to cu top left */
                pu1_top_left = pu1_cu_top_left;

                /* top left based on position */
                if((0 != i4_subtu_pos_y) && (0 == i4_subtu_pos_x))
                {
                    pu1_top_left = pu1_left - left_strd;
                }
                else if(0 != i4_subtu_pos_x)
                {
                    pu1_top_left = pu1_top - 2;
                }

                /* populate the coeffs scan idx */
                scan_idx = SCAN_DIAG_UPRIGHT;

                /* RDOPT copy States :  TU init (best until prev TU) to current */
                COPY_CABAC_STATES(
                    &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                         .s_cabac_ctxt.au1_ctxt_models[0],
                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                    IHEVC_CAB_CTXT_END);

                /* for 4x4 transforms based on intra pred mode scan is choosen*/
                if(4 == trans_size)
                {
                    /* for modes from 22 upto 30 horizontal scan is used */
                    if((best_chrm_mode > 21) && (best_chrm_mode < 31))
                    {
                        scan_idx = SCAN_HORZ;
                    }
                    /* for modes from 6 upto 14 horizontal scan is used */
                    else if((best_chrm_mode > 5) && (best_chrm_mode < 15))
                    {
                        scan_idx = SCAN_VERT;
                    }
                }

                nbr_flags = ihevce_get_intra_chroma_tu_nbr(
                    luma_nbr_flags, i4_subtu_idx, trans_size, u1_is_422);

                /* call the chroma reference array substitution */
                ihevc_intra_pred_chroma_ref_substitution_fptr(
                    pu1_top_left,
                    pu1_top,
                    pu1_left,
                    left_strd,
                    trans_size,
                    nbr_flags,
                    (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                    1);

                /* use the look up to get the function idx */
                chrm_pred_func_idx = g_i4_ip_funcs[best_chrm_mode];

                /* call the intra prediction function */
                ps_ctxt->apf_chrm_ip[chrm_pred_func_idx](
                    (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                    1,
                    pu1_cur_pred,
                    pred_strd,
                    trans_size,
                    best_chrm_mode);

                /* UPLANE RDOPT Loop */
                {
                    WORD32 tu_bits;

                    cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                        ps_ctxt,
                        pu1_cur_pred,
                        pred_strd,
                        pu1_cur_src,
                        chrm_src_stride,
                        pi2_cur_deq_data_cb,
                        deq_data_strd,
                        pu1_cur_recon,
                        i4_recon_stride,
                        pu1_ecd_data_cb + ai4_total_bytes_offset_cb[i4_subtu_idx],
                        ps_ctxt->au1_cu_csbf,
                        ps_ctxt->i4_cu_csbf_strd,
                        trans_size,
                        scan_idx,
                        1,
                        &num_bytes,
                        &tu_bits,
                        &ps_chr_intra_satd_ctxt->ai4_zero_col_cb[i4_subtu_idx][ctr],
                        &ps_chr_intra_satd_ctxt->ai4_zero_row_cb[i4_subtu_idx][ctr],
                        &u1_is_recon_available,
                        i4_perform_sbh,
                        i4_perform_rdoq,
                        &trans_ssd_u,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                        i4_alpha_stim_multiplier,
                        u1_is_cu_noisy,
#endif
                        0,
                        u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                        U_PLANE);

#if !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS && COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL
                    if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                    {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
                        trans_ssd_u = ihevce_inject_stim_into_distortion(
                            pu1_cur_src,
                            chrm_src_stride,
                            pu1_cur_pred,
                            pred_strd,
                            trans_ssd_u,
                            i4_alpha_stim_multiplier,
                            trans_size,
                            0,
                            ps_ctxt->u1_enable_psyRDOPT,
                            U_PLANE);
#else
                        if(u1_compute_spatial_ssd && u1_is_recon_available)
                        {
                            trans_ssd_u = ihevce_inject_stim_into_distortion(
                                pu1_cur_src,
                                chrm_src_stride,
                                pu1_cur_recon,
                                i4_recon_stride,
                                trans_ssd_u,
                                i4_alpha_stim_multiplier,
                                trans_size,
                                0,
                                ps_ctxt->u1_enable_psyRDOPT,
                                U_PLANE);
                        }
                        else
                        {
                            trans_ssd_u = ihevce_inject_stim_into_distortion(
                                pu1_cur_src,
                                chrm_src_stride,
                                pu1_cur_pred,
                                pred_strd,
                                trans_ssd_u,
                                i4_alpha_stim_multiplier,
                                trans_size,
                                0,
                                ps_ctxt->u1_enable_psyRDOPT,
                                U_PLANE);
                        }
#endif
                    }
#endif

                    /* RDOPT copy States :  New updated after curr TU to TU init */
                    if(0 != cbf)
                    {
                        memcpy(
                            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                                 .s_cabac_ctxt.au1_ctxt_models[0],
                            IHEVC_CAB_CTXT_END);
                    }
                    /* RDOPT copy States :  Restoring back the Cb init state to Cr */
                    else
                    {
                        memcpy(
                            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                                 .s_cabac_ctxt.au1_ctxt_models[0],
                            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                            IHEVC_CAB_CTXT_END);
                    }

                    if(calc_recon || (!u1_is_recon_available && u1_compute_spatial_ssd))
                    {
                        ihevce_chroma_it_recon_fxn(
                            ps_ctxt,
                            pi2_cur_deq_data_cb,
                            deq_data_strd,
                            pu1_cur_pred,
                            pred_strd,
                            pu1_cur_recon,
                            i4_recon_stride,
                            (pu1_ecd_data_cb + ai4_total_bytes_offset_cb[i4_subtu_idx]),
                            trans_size,
                            cbf,
                            ps_chr_intra_satd_ctxt->ai4_zero_col_cb[i4_subtu_idx][ctr],
                            ps_chr_intra_satd_ctxt->ai4_zero_row_cb[i4_subtu_idx][ctr],
                            U_PLANE);
                    }

                    ps_chr_intra_satd_ctxt->au1_cbf_cb[i4_subtu_idx][ctr] = cbf;
                    curr_cb_cod_cost =
                        trans_ssd_u +
                        COMPUTE_RATE_COST_CLIP30(
                            tu_bits, ps_ctxt->i8_cl_ssd_lambda_chroma_qf, LAMBDA_Q_SHIFT);
                    chrm_tu_bits += tu_bits;
                    ai4_total_bytes_offset_cb[i4_subtu_idx] += num_bytes;
                    ps_chr_intra_satd_ctxt->ai4_num_bytes_scan_coeff_cb_per_tu[i4_subtu_idx][ctr] =
                        num_bytes;
                }

                /* VPLANE RDOPT Loop */
                {
                    WORD32 tu_bits;

                    cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                        ps_ctxt,
                        pu1_cur_pred,
                        pred_strd,
                        pu1_cur_src,
                        chrm_src_stride,
                        pi2_cur_deq_data_cr,
                        deq_data_strd,
                        pu1_cur_recon,
                        i4_recon_stride,
                        pu1_ecd_data_cr + ai4_total_bytes_offset_cr[i4_subtu_idx],
                        ps_ctxt->au1_cu_csbf,
                        ps_ctxt->i4_cu_csbf_strd,
                        trans_size,
                        scan_idx,
                        1,
                        &num_bytes,
                        &tu_bits,
                        &ps_chr_intra_satd_ctxt->ai4_zero_col_cr[i4_subtu_idx][ctr],
                        &ps_chr_intra_satd_ctxt->ai4_zero_row_cr[i4_subtu_idx][ctr],
                        &u1_is_recon_available,
                        i4_perform_sbh,
                        i4_perform_rdoq,
                        &trans_ssd_v,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                        i4_alpha_stim_multiplier,
                        u1_is_cu_noisy,
#endif
                        0,
                        u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                        V_PLANE);

#if !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS && COMPUTE_NOISE_TERM_AT_THE_TU_LEVEL
                    if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                    {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
                        trans_ssd_v = ihevce_inject_stim_into_distortion(
                            pu1_cur_src,
                            chrm_src_stride,
                            pu1_cur_pred,
                            pred_strd,
                            trans_ssd_v,
                            i4_alpha_stim_multiplier,
                            trans_size,
                            0,
                            ps_ctxt->u1_enable_psyRDOPT,
                            V_PLANE);
#else
                        if(u1_compute_spatial_ssd && u1_is_recon_available)
                        {
                            trans_ssd_v = ihevce_inject_stim_into_distortion(
                                pu1_cur_src,
                                chrm_src_stride,
                                pu1_cur_recon,
                                i4_recon_stride,
                                trans_ssd_v,
                                i4_alpha_stim_multiplier,
                                trans_size,
                                0,
                                ps_ctxt->u1_enable_psyRDOPT,
                                V_PLANE);
                        }
                        else
                        {
                            trans_ssd_v = ihevce_inject_stim_into_distortion(
                                pu1_cur_src,
                                chrm_src_stride,
                                pu1_cur_pred,
                                pred_strd,
                                trans_ssd_v,
                                i4_alpha_stim_multiplier,
                                trans_size,
                                0,
                                ps_ctxt->u1_enable_psyRDOPT,
                                V_PLANE);
                        }
#endif
                    }
#endif

                    /* RDOPT copy States :  New updated after curr TU to TU init */
                    if(0 != cbf)
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                                 .s_cabac_ctxt.au1_ctxt_models[0],
                            IHEVC_CAB_CTXT_END);
                    }
                    /* RDOPT copy States :  Restoring back the Cb init state to Cr */
                    else
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                                 .s_cabac_ctxt.au1_ctxt_models[0],
                            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                            IHEVC_CAB_CTXT_END);
                    }

                    if(calc_recon || (!u1_is_recon_available && u1_compute_spatial_ssd))
                    {
                        ihevce_chroma_it_recon_fxn(
                            ps_ctxt,
                            pi2_cur_deq_data_cr,
                            deq_data_strd,
                            pu1_cur_pred,
                            pred_strd,
                            pu1_cur_recon,
                            i4_recon_stride,
                            (pu1_ecd_data_cr + ai4_total_bytes_offset_cr[i4_subtu_idx]),
                            trans_size,
                            cbf,
                            ps_chr_intra_satd_ctxt->ai4_zero_col_cr[i4_subtu_idx][ctr],
                            ps_chr_intra_satd_ctxt->ai4_zero_row_cr[i4_subtu_idx][ctr],
                            V_PLANE);
                    }

                    ps_chr_intra_satd_ctxt->au1_cbf_cr[i4_subtu_idx][ctr] = cbf;
                    curr_cr_cod_cost =
                        trans_ssd_v +
                        COMPUTE_RATE_COST_CLIP30(
                            tu_bits, ps_ctxt->i8_cl_ssd_lambda_chroma_qf, LAMBDA_Q_SHIFT);
                    chrm_tu_bits += tu_bits;
                    ai4_total_bytes_offset_cr[i4_subtu_idx] += num_bytes;
                    ps_chr_intra_satd_ctxt->ai4_num_bytes_scan_coeff_cr_per_tu[i4_subtu_idx][ctr] =
                        num_bytes;
                }

                chrm_cod_cost += curr_cb_cod_cost;
                chrm_cod_cost += curr_cr_cod_cost;
            }

            /* set the neighbour map to 1 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ctr & 1) * (luma_trans_size >> 2) + cu_pos_x,
                (ctr > 1) * (luma_trans_size >> 2) + cu_pos_y,
                (luma_trans_size >> 2),
                1);
        }

        /* set the neighbour map to 0 */
        ihevce_set_nbr_map(
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            (ps_cu_analyse->b3_cu_pos_x << 1),
            (ps_cu_analyse->b3_cu_pos_y << 1),
            (ps_cu_analyse->u1_cu_size >> 2),
            0);

        /* Account for coding b3_chroma_intra_pred_mode prefix and suffix bins */
        /* This is done by adding the bits for signalling chroma mode (0-3)    */
        /* and subtracting the bits for chroma mode same as luma mode (4)      */
#if CHROMA_RDOPT_ENABLE
        {
            /* Estimate bits to encode prefix bin as 1 for b3_chroma_intra_pred_mode */
            WORD32 bits_frac_1 =
                gau2_ihevce_cabac_bin_to_bits[u1_chroma_intra_mode_prefix_state ^ 1];

            WORD32 bits_for_mode_0to3 = (2 << CABAC_FRAC_BITS_Q) + bits_frac_1;

            /* Estimate bits to encode prefix bin as 0 for b3_chroma_intra_pred_mode */
            WORD32 bits_for_mode4 =
                gau2_ihevce_cabac_bin_to_bits[u1_chroma_intra_mode_prefix_state ^ 0];

            /* accumulate into final rd cost for chroma */
            ps_chr_intra_satd_ctxt->i8_cost_to_encode_chroma_mode = COMPUTE_RATE_COST_CLIP30(
                (bits_for_mode_0to3 - bits_for_mode4),
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

            chrm_cod_cost += ps_chr_intra_satd_ctxt->i8_cost_to_encode_chroma_mode;
        }
#endif

        if(ps_ctxt->u1_enable_psyRDOPT)
        {
            UWORD8 *pu1_recon_cu;
            WORD32 recon_stride;
            WORD32 curr_pos_x;
            WORD32 curr_pos_y;
            WORD32 start_index;
            WORD32 num_horz_cu_in_ctb;
            WORD32 had_block_size;

            /* tODO: sreenivasa ctb size has to be used appropriately */
            had_block_size = 8;
            num_horz_cu_in_ctb = 2 * 64 / had_block_size;
            curr_pos_x = ps_cu_analyse->b3_cu_pos_x << 3; /* pel units */
            curr_pos_y = ps_cu_analyse->b3_cu_pos_x << 3; /* pel units */
            recon_stride = aps_recon_datastore[0]->i4_chromaRecon_stride;
            pu1_recon_cu =
                aps_recon_datastore[0]->apv_chroma_recon_bufs[1 + (num_tus_in_cu > 1)];  //

            /* start index to index the source satd of curr cu int he current ctb*/
            start_index = 2 * (curr_pos_x / had_block_size) +
                          (curr_pos_y / had_block_size) * num_horz_cu_in_ctb;

            {
                chrm_cod_cost += ihevce_psy_rd_cost_croma(
                    ps_ctxt->ai4_source_chroma_satd,
                    pu1_recon_cu,
                    recon_stride,
                    1,  //
                    cu_size,
                    0,  // pic type
                    0,  //layer id
                    ps_ctxt->i4_satd_lamda,  // lambda
                    start_index,
                    ps_ctxt->u1_is_input_data_hbd,  // 8 bit
                    ps_ctxt->u1_chroma_array_type,
                    &ps_ctxt->s_cmn_opt_func

                );  // chroma subsampling 420
            }
        }

        ps_chr_intra_satd_ctxt->i8_chroma_best_rdopt = chrm_cod_cost;
        ps_chr_intra_satd_ctxt->i4_chrm_tu_bits = chrm_tu_bits;

        memcpy(
            &ps_chr_intra_satd_ctxt->au1_chrm_satd_updated_ctxt_models[0],
            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
            IHEVC_CAB_CTXT_END);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_chroma_cu_prcs_rdopt \endif
*
* \brief
*    Coding unit processing function for chroma
*
* \param[in] ps_ctxt    enc_loop module ctxt pointer
* \param[in] rd_opt_curr_idx index in the array of RDopt params
* \param[in] func_proc_mode TU_EQ_CU or other case
* \param[in] pu1_chrm_src  pointer to source data buffer
* \param[in] chrm_src_stride   source buffer stride
* \param[in] pu1_cu_left pointer to left recon data buffer
* \param[in] pu1_cu_top  pointer to top recon data buffer
* \param[in] pu1_cu_top_left pointer to top left recon data buffer
* \param[in] left_stride left recon buffer stride
* \param[out] cu_pos_x position x of current CU in CTB
* \param[out] cu_pos_y position y of current CU in CTB
* \param[out] pi4_chrm_tu_bits pointer to store the totla chroma bits
*
* \return
*    Chroma coding cost (cb adn Cr included)
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_chroma_cu_prcs_rdopt(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    WORD32 rd_opt_curr_idx,
    WORD32 func_proc_mode,
    UWORD8 *pu1_chrm_src,
    WORD32 chrm_src_stride,
    UWORD8 *pu1_cu_left,
    UWORD8 *pu1_cu_top,
    UWORD8 *pu1_cu_top_left,
    WORD32 cu_left_stride,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 *pi4_chrm_tu_bits,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy)
{
    tu_enc_loop_out_t *ps_tu;
    tu_enc_loop_temp_prms_t *ps_tu_temp_prms;

    ihevc_intra_pred_chroma_ref_substitution_ft *ihevc_intra_pred_chroma_ref_substitution_fptr;

    UWORD8 *pu1_pred;
    UWORD8 *pu1_recon;
    WORD32 i4_recon_stride;
    WORD32 cu_size, trans_size = 0;
    WORD32 pred_strd;
    WORD32 ctr, i4_subtu_idx;
    WORD32 scan_idx;
    WORD32 u1_is_cu_coded_old;
    WORD32 init_bytes_offset;

    enc_loop_cu_final_prms_t *ps_best_cu_prms = &ps_ctxt->as_cu_prms[rd_opt_curr_idx];
    recon_datastore_t *ps_recon_datastore = &ps_best_cu_prms->s_recon_datastore;

    WORD32 total_bytes_offset = 0;
    LWORD64 chrm_cod_cost = 0;
    WORD32 chrm_tu_bits = 0;
    WORD32 chrm_pred_mode = DM_CHROMA_IDX, luma_pred_mode = 35;
    LWORD64 i8_ssd_cb = 0;
    WORD32 i4_bits_cb = 0;
    LWORD64 i8_ssd_cr = 0;
    WORD32 i4_bits_cr = 0;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);
    UWORD8 u1_num_tus =
        /* NumChromaTU's = 1, if TUSize = 4 and CUSize = 8 */
        (!ps_best_cu_prms->as_tu_enc_loop[0].s_tu.b3_size && ps_best_cu_prms->u1_intra_flag)
            ? 1
            : ps_best_cu_prms->u2_num_tus_in_cu;
    UWORD8 u1_num_subtus_in_tu = u1_is_422 + 1;
    UWORD8 u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_QP_WHERE_SPATIAL_SSD_ENABLED) &&
                                    (ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3) &&
                                    CONVERT_SSDS_TO_SPATIAL_DOMAIN;
    /* Get the RDOPT cost of the best CU mode for early_exit */
    LWORD64 prev_best_rdopt_cost = ps_ctxt->as_cu_prms[!rd_opt_curr_idx].i8_best_rdopt_cost;
    /* Get the current running RDOPT (Luma RDOPT) for early_exit */
    LWORD64 curr_rdopt_cost = ps_ctxt->as_cu_prms[rd_opt_curr_idx].i8_curr_rdopt_cost;
    WORD32 i4_perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq;
    WORD32 i4_perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh;

    ihevc_intra_pred_chroma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_chroma_ref_substitution_fptr;

    if(u1_is_cu_noisy || ps_ctxt->u1_enable_psyRDOPT)
    {
        u1_compute_spatial_ssd = (ps_ctxt->i4_cu_qp <= MAX_HEVC_QP) &&
                                 CONVERT_SSDS_TO_SPATIAL_DOMAIN;
    }

    /* Store the init bytes offset from luma */
    init_bytes_offset = ps_best_cu_prms->i4_num_bytes_ecd_data;

    /* Unused pred buffer in merge_skip_pred_data_t structure is used as
    Chroma pred storage buf. for final_recon function.
    The buffer is split into two and used as a ping-pong buffer */
    pu1_pred = ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[CU_ME_INTRA_PRED_CHROMA_IDX] +
               rd_opt_curr_idx * ((MAX_CTB_SIZE * MAX_CTB_SIZE >> 1) +
                                  (u1_is_422 * (MAX_CTB_SIZE * MAX_CTB_SIZE >> 1)));

    pred_strd = ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[CU_ME_INTRA_PRED_CHROMA_IDX];

    pu1_recon = (UWORD8 *)ps_recon_datastore->apv_chroma_recon_bufs[0];
    i4_recon_stride = ps_recon_datastore->i4_chromaRecon_stride;
    cu_size = ps_best_cu_prms->u1_cu_size;
    chrm_tu_bits = 0;

    /* get the first TU pointer */
    ps_tu = &ps_best_cu_prms->as_tu_enc_loop[0];
    /* get the first TU enc_loop temp prms pointer */
    ps_tu_temp_prms = &ps_best_cu_prms->as_tu_enc_loop_temp_prms[0];

    if(PRED_MODE_INTRA == ps_best_cu_prms->u1_intra_flag)
    {
        /* Mode signalled by intra prediction for luma */
        luma_pred_mode = ps_best_cu_prms->au1_intra_pred_mode[0];

#if DISABLE_RDOQ_INTRA
        i4_perform_rdoq = 0;
#endif
    }

    else
    {
        UWORD8 *pu1_pred_org = pu1_pred;

        /* ------ Motion Compensation for Chroma -------- */
        for(ctr = 0; ctr < ps_best_cu_prms->u2_num_pus_in_cu; ctr++)
        {
            pu_t *ps_pu;
            WORD32 inter_pu_wd;
            WORD32 inter_pu_ht;

            ps_pu = &ps_best_cu_prms->as_pu_chrm_proc[ctr];

            inter_pu_wd = (ps_pu->b4_wd + 1) << 2; /* cb and cr pixel interleaved */
            inter_pu_ht = ((ps_pu->b4_ht + 1) << 2) >> 1;
            inter_pu_ht <<= u1_is_422;

            ihevce_chroma_inter_pred_pu(&ps_ctxt->s_mc_ctxt, ps_pu, pu1_pred, pred_strd);

            if(2 == ps_best_cu_prms->u2_num_pus_in_cu)
            {
                /* 2Nx__ partion case */
                if(inter_pu_wd == cu_size)
                {
                    pu1_pred += (inter_pu_ht * pred_strd);
                }

                /* __x2N partion case */
                if(inter_pu_ht == (cu_size >> (u1_is_422 == 0)))
                {
                    pu1_pred += inter_pu_wd;
                }
            }
        }

        /* restore the pred pointer to start for transform loop */
        pu1_pred = pu1_pred_org;
    }

    /* Used to store back only the luma based info. if SATD based chorma
    mode also comes */
    u1_is_cu_coded_old = ps_best_cu_prms->u1_is_cu_coded;

    /* evaluate chroma candidates (same as luma) and
    if INTRA & HIGH_QUALITY compare with best SATD mode */
    {
        WORD32 calc_recon = 0, deq_data_strd;
        WORD16 *pi2_deq_data;
        UWORD8 *pu1_ecd_data;
        UWORD8 u1_is_mode_eq_chroma_satd_mode = 0;

        pi2_deq_data = &ps_best_cu_prms->pi2_cu_deq_coeffs[0];
        pi2_deq_data += ps_best_cu_prms->i4_chrm_deq_coeff_strt_idx;
        deq_data_strd = cu_size;
        /* update ecd buffer for storing coeff. */
        pu1_ecd_data = &ps_best_cu_prms->pu1_cu_coeffs[0];
        pu1_ecd_data += init_bytes_offset;
        /* store chroma starting index */
        ps_best_cu_prms->i4_chrm_cu_coeff_strt_idx = init_bytes_offset;

        /* get the first TU pointer */
        ps_tu = &ps_best_cu_prms->as_tu_enc_loop[0];
        ps_tu_temp_prms = &ps_best_cu_prms->as_tu_enc_loop_temp_prms[0];

        /* Reset total_bytes_offset for each candidate */
        chrm_pred_mode = (u1_is_422 == 1) ? gau1_chroma422_intra_angle_mapping[luma_pred_mode]
                                          : luma_pred_mode;

        total_bytes_offset = 0;

        if(TU_EQ_SUBCU == func_proc_mode)
        {
            func_proc_mode = TU_EQ_CU_DIV2;
        }

        /* For cu_size=8 case, chroma cost will be same for TU_EQ_CU and
        TU_EQ_CU_DIV2 and  TU_EQ_SUBCU case */
        if(8 == cu_size)
        {
            func_proc_mode = TU_EQ_CU;
        }

        /* loop based on num tus in a cu */
        if(!ps_best_cu_prms->u1_intra_flag || !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd ||
           (ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd &&
            (chrm_pred_mode !=
             ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[func_proc_mode].u1_best_cr_mode)))
        {
            /* loop based on num tus in a cu */
            for(ctr = 0; ctr < u1_num_tus; ctr++)
            {
                WORD32 num_bytes = 0;
                LWORD64 curr_cb_cod_cost = 0;
                LWORD64 curr_cr_cod_cost = 0;
                WORD32 chrm_pred_func_idx = 0;
                UWORD8 u1_is_early_exit_condition_satisfied = 0;

                /* Default cb and cr offset initializatio for b3_chroma_intra_mode_idx=7   */
                /* FIX for TU tree shrinkage caused by ecd data copies in final mode recon */
                ps_tu->s_tu.b1_cb_cbf = ps_tu->s_tu.b1_cr_cbf = 0;
                ps_tu->s_tu.b1_cb_cbf_subtu1 = ps_tu->s_tu.b1_cr_cbf_subtu1 = 0;
                ps_tu->ai4_cb_coeff_offset[0] = total_bytes_offset + init_bytes_offset;
                ps_tu->ai4_cr_coeff_offset[0] = total_bytes_offset + init_bytes_offset;
                ps_tu->ai4_cb_coeff_offset[1] = total_bytes_offset + init_bytes_offset;
                ps_tu->ai4_cr_coeff_offset[1] = total_bytes_offset + init_bytes_offset;
                ps_tu_temp_prms->ai2_cb_bytes_consumed[0] = 0;
                ps_tu_temp_prms->ai2_cr_bytes_consumed[0] = 0;
                ps_tu_temp_prms->ai2_cb_bytes_consumed[1] = 0;
                ps_tu_temp_prms->ai2_cr_bytes_consumed[1] = 0;

                /* TU level inits */
                /* check if chroma present flag is set */
                if(1 == ps_tu->s_tu.b3_chroma_intra_mode_idx)
                {
                    /* RDOPT copy States :  TU init (best until prev TU) to current */
                    COPY_CABAC_STATES(
                        &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_curr_idx]
                             .s_cabac_ctxt.au1_ctxt_models[0],
                        &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                        IHEVC_CAB_CTXT_END);

                    /* get the current transform size */
                    trans_size = ps_tu->s_tu.b3_size;
                    trans_size = (1 << (trans_size + 1)); /* in chroma units */

                    /* since 2x2 transform is not allowed for chroma*/
                    if(2 == trans_size)
                    {
                        trans_size = 4;
                    }
                }

                for(i4_subtu_idx = 0; i4_subtu_idx < u1_num_subtus_in_tu; i4_subtu_idx++)
                {
                    WORD32 cbf;
                    UWORD8 u1_is_recon_available;

                    WORD32 nbr_flags = 0;
                    WORD32 zero_cols = 0;
                    WORD32 zero_rows = 0;

                    /* check if chroma present flag is set */
                    if(1 == ps_tu->s_tu.b3_chroma_intra_mode_idx)
                    {
                        UWORD8 *pu1_cur_pred;
                        UWORD8 *pu1_cur_recon;
                        UWORD8 *pu1_cur_src;
                        WORD16 *pi2_cur_deq_data;
                        WORD32 curr_pos_x, curr_pos_y;
                        LWORD64 trans_ssd_u, trans_ssd_v;

                        /* get the current sub-tu posx and posy w.r.t to cu */
                        curr_pos_x = (ps_tu->s_tu.b4_pos_x << 2) - (cu_pos_x << 3);
                        curr_pos_y = (ps_tu->s_tu.b4_pos_y << 2) - (cu_pos_y << 3) +
                                     (i4_subtu_idx * trans_size);

                        /* 420sp case only vertical height will be half */
                        if(u1_is_422 == 0)
                        {
                            curr_pos_y >>= 1;
                        }

                        /* increment the pointers to start of current Sub-TU */
                        pu1_cur_recon = (pu1_recon + curr_pos_x);
                        pu1_cur_recon += (curr_pos_y * i4_recon_stride);
                        pu1_cur_src = (pu1_chrm_src + curr_pos_x);
                        pu1_cur_src += (curr_pos_y * chrm_src_stride);
                        pu1_cur_pred = (pu1_pred + curr_pos_x);
                        pu1_cur_pred += (curr_pos_y * pred_strd);
                        pi2_cur_deq_data = pi2_deq_data + curr_pos_x;
                        pi2_cur_deq_data += (curr_pos_y * deq_data_strd);

                        /* populate the coeffs scan idx */
                        scan_idx = SCAN_DIAG_UPRIGHT;

                        /* perform intra prediction only for Intra case */
                        if(PRED_MODE_INTRA == ps_best_cu_prms->u1_intra_flag)
                        {
                            UWORD8 *pu1_top_left;
                            UWORD8 *pu1_top;
                            UWORD8 *pu1_left;
                            WORD32 left_strd;

                            calc_recon = !u1_compute_spatial_ssd &&
                                         ((4 == u1_num_tus) || (u1_is_422 == 1)) &&
                                         (((u1_num_tus == 1) && (0 == i4_subtu_idx)) ||
                                          ((ctr == 3) && (0 == i4_subtu_idx) && (u1_is_422 == 1)) ||
                                          ((u1_num_tus == 4) && (ctr < 3)));

                            /* left cu boundary */
                            if(0 == curr_pos_x)
                            {
                                pu1_left = pu1_cu_left + curr_pos_y * cu_left_stride;
                                left_strd = cu_left_stride;
                            }
                            else
                            {
                                pu1_left = pu1_cur_recon - 2;
                                left_strd = i4_recon_stride;
                            }

                            /* top cu boundary */
                            if(0 == curr_pos_y)
                            {
                                pu1_top = pu1_cu_top + curr_pos_x;
                            }
                            else
                            {
                                pu1_top = pu1_cur_recon - i4_recon_stride;
                            }

                            /* by default top left is set to cu top left */
                            pu1_top_left = pu1_cu_top_left;

                            /* top left based on position */
                            if((0 != curr_pos_y) && (0 == curr_pos_x))
                            {
                                pu1_top_left = pu1_left - cu_left_stride;
                            }
                            else if(0 != curr_pos_x)
                            {
                                pu1_top_left = pu1_top - 2;
                            }

                            /* for 4x4 transforms based on intra pred mode scan is choosen*/
                            if(4 == trans_size)
                            {
                                /* for modes from 22 upto 30 horizontal scan is used */
                                if((chrm_pred_mode > 21) && (chrm_pred_mode < 31))
                                {
                                    scan_idx = SCAN_HORZ;
                                }
                                /* for modes from 6 upto 14 horizontal scan is used */
                                else if((chrm_pred_mode > 5) && (chrm_pred_mode < 15))
                                {
                                    scan_idx = SCAN_VERT;
                                }
                            }

                            nbr_flags = ihevce_get_intra_chroma_tu_nbr(
                                ps_best_cu_prms->au4_nbr_flags[ctr],
                                i4_subtu_idx,
                                trans_size,
                                u1_is_422);

                            /* call the chroma reference array substitution */
                            ihevc_intra_pred_chroma_ref_substitution_fptr(
                                pu1_top_left,
                                pu1_top,
                                pu1_left,
                                left_strd,
                                trans_size,
                                nbr_flags,
                                (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                                1);

                            /* use the look up to get the function idx */
                            chrm_pred_func_idx = g_i4_ip_funcs[chrm_pred_mode];

                            /* call the intra prediction function */
                            ps_ctxt->apf_chrm_ip[chrm_pred_func_idx](
                                (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                                1,
                                pu1_cur_pred,
                                pred_strd,
                                trans_size,
                                chrm_pred_mode);
                        }

                        if(!ctr && !i4_subtu_idx && (u1_compute_spatial_ssd || calc_recon))
                        {
                            ps_recon_datastore->au1_is_chromaRecon_available[0] =
                                !ps_best_cu_prms->u1_skip_flag;
                        }
                        else if(!ctr && !i4_subtu_idx)
                        {
                            ps_recon_datastore->au1_is_chromaRecon_available[0] = 0;
                        }
                        /************************************************************/
                        /* recon loop is done for all cases including skip cu       */
                        /* This is because skipping chroma reisdual based on luma   */
                        /* skip decision can lead to chroma artifacts               */
                        /************************************************************/
                        /************************************************************/
                        /*In the high quality and medium speed modes, wherein chroma*/
                        /*and luma costs are included in the total cost calculation */
                        /*the cost is just a ssd cost, and not that obtained through*/
                        /*iq_it path                                                */
                        /************************************************************/
                        if(ps_best_cu_prms->u1_skip_flag == 0)
                        {
                            WORD32 tu_bits;

                            cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                                ps_ctxt,
                                pu1_cur_pred,
                                pred_strd,
                                pu1_cur_src,
                                chrm_src_stride,
                                pi2_cur_deq_data,
                                deq_data_strd,
                                pu1_cur_recon,
                                i4_recon_stride,
                                pu1_ecd_data + total_bytes_offset,
                                ps_ctxt->au1_cu_csbf,
                                ps_ctxt->i4_cu_csbf_strd,
                                trans_size,
                                scan_idx,
                                PRED_MODE_INTRA == ps_best_cu_prms->u1_intra_flag,
                                &num_bytes,
                                &tu_bits,
                                &zero_cols,
                                &zero_rows,
                                &u1_is_recon_available,
                                i4_perform_sbh,
                                i4_perform_rdoq,
                                &trans_ssd_u,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                                i4_alpha_stim_multiplier,
                                u1_is_cu_noisy,
#endif
                                ps_best_cu_prms->u1_skip_flag,
                                u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                                U_PLANE);

                            if(u1_compute_spatial_ssd && u1_is_recon_available)
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }
                            else
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                            }

#if !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                            {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
                                trans_ssd_u = ihevce_inject_stim_into_distortion(
                                    pu1_cur_src,
                                    chrm_src_stride,
                                    pu1_cur_pred,
                                    pred_strd,
                                    trans_ssd_u,
                                    i4_alpha_stim_multiplier,
                                    trans_size,
                                    0,
                                    ps_ctxt->u1_enable_psyRDOPT,
                                    U_PLANE);
#else
                                if(u1_compute_spatial_ssd && u1_is_recon_available)
                                {
                                    trans_ssd_u = ihevce_inject_stim_into_distortion(
                                        pu1_cur_src,
                                        chrm_src_stride,
                                        pu1_cur_recon,
                                        i4_recon_stride,
                                        trans_ssd_u,
                                        i4_alpha_stim_multiplier,
                                        trans_size,
                                        0,
                                        ps_ctxt->u1_enable_psyRDOPT,
                                        U_PLANE);
                                }
                                else
                                {
                                    trans_ssd_u = ihevce_inject_stim_into_distortion(
                                        pu1_cur_src,
                                        chrm_src_stride,
                                        pu1_cur_pred,
                                        pred_strd,
                                        trans_ssd_u,
                                        i4_alpha_stim_multiplier,
                                        trans_size,
                                        0,
                                        ps_ctxt->u1_enable_psyRDOPT,
                                        U_PLANE);
                                }
#endif
                            }
#endif

                            curr_cb_cod_cost =
                                trans_ssd_u +
                                COMPUTE_RATE_COST_CLIP30(
                                    tu_bits, ps_ctxt->i8_cl_ssd_lambda_chroma_qf, LAMBDA_Q_SHIFT);

                            chrm_tu_bits += tu_bits;
                            i4_bits_cb += tu_bits;

                            /* RDOPT copy States :  New updated after curr TU to TU init */
                            if(0 != cbf)
                            {
                                COPY_CABAC_STATES(
                                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                                    &ps_ctxt->s_rdopt_entropy_ctxt
                                         .as_cu_entropy_ctxt[rd_opt_curr_idx]
                                         .s_cabac_ctxt.au1_ctxt_models[0],
                                    IHEVC_CAB_CTXT_END);
                            }
                            /* RDOPT copy States :  Restoring back the Cb init state to Cr */
                            else
                            {
                                COPY_CABAC_STATES(
                                    &ps_ctxt->s_rdopt_entropy_ctxt
                                         .as_cu_entropy_ctxt[rd_opt_curr_idx]
                                         .s_cabac_ctxt.au1_ctxt_models[0],
                                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                                    IHEVC_CAB_CTXT_END);
                            }

                            /* If Intra and TU=CU/2, need recon for next TUs */
                            if(calc_recon)
                            {
                                ihevce_chroma_it_recon_fxn(
                                    ps_ctxt,
                                    pi2_cur_deq_data,
                                    deq_data_strd,
                                    pu1_cur_pred,
                                    pred_strd,
                                    pu1_cur_recon,
                                    i4_recon_stride,
                                    (pu1_ecd_data + total_bytes_offset),
                                    trans_size,
                                    cbf,
                                    zero_cols,
                                    zero_rows,
                                    U_PLANE);

                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }
                            else
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                            }
                        }
                        else
                        {
                            /* num bytes is set to 0 */
                            num_bytes = 0;

                            /* cbf is returned as 0 */
                            cbf = 0;

                            curr_cb_cod_cost = trans_ssd_u =

                                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_ssd_calculator(
                                    pu1_cur_pred,
                                    pu1_cur_src,
                                    pred_strd,
                                    chrm_src_stride,
                                    trans_size,
                                    trans_size,
                                    U_PLANE);

                            if(u1_compute_spatial_ssd)
                            {
                                /* buffer copy fromp pred to recon */

                                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                    pu1_cur_pred,
                                    pred_strd,
                                    pu1_cur_recon,
                                    i4_recon_stride,
                                    trans_size,
                                    trans_size,
                                    U_PLANE);

                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }

                            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                            {
                                trans_ssd_u = ihevce_inject_stim_into_distortion(
                                    pu1_cur_src,
                                    chrm_src_stride,
                                    pu1_cur_pred,
                                    pred_strd,
                                    trans_ssd_u,
                                    i4_alpha_stim_multiplier,
                                    trans_size,
                                    0,
                                    ps_ctxt->u1_enable_psyRDOPT,
                                    U_PLANE);
                            }

#if ENABLE_INTER_ZCU_COST
#if !WEIGH_CHROMA_COST
                            /* cbf = 0, accumulate cu not coded cost */
                            ps_ctxt->i8_cu_not_coded_cost += curr_cb_cod_cost;
#else
                            /* cbf = 0, accumulate cu not coded cost */

                            ps_ctxt->i8_cu_not_coded_cost += (LWORD64)(
                                (curr_cb_cod_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                                 (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                                CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif
#endif
                        }

#if !WEIGH_CHROMA_COST
                        curr_rdopt_cost += curr_cb_cod_cost;
#else
                        curr_rdopt_cost +=
                            ((curr_cb_cod_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                              (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                             CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif
                        chrm_cod_cost += curr_cb_cod_cost;
                        i8_ssd_cb += trans_ssd_u;

                        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
                        {
                            /* Early exit : If the current running cost exceeds
                            the prev. best mode cost, break */
                            if(curr_rdopt_cost > prev_best_rdopt_cost)
                            {
                                u1_is_early_exit_condition_satisfied = 1;
                                break;
                            }
                        }

                        /* inter cu is coded if any of the tu is coded in it */
                        ps_best_cu_prms->u1_is_cu_coded |= cbf;

                        /* update CB related params */
                        ps_tu->ai4_cb_coeff_offset[i4_subtu_idx] =
                            total_bytes_offset + init_bytes_offset;

                        if(0 == i4_subtu_idx)
                        {
                            ps_tu->s_tu.b1_cb_cbf = cbf;
                        }
                        else
                        {
                            ps_tu->s_tu.b1_cb_cbf_subtu1 = cbf;
                        }

                        total_bytes_offset += num_bytes;

                        ps_tu_temp_prms->au4_cb_zero_col[i4_subtu_idx] = zero_cols;
                        ps_tu_temp_prms->au4_cb_zero_row[i4_subtu_idx] = zero_rows;
                        ps_tu_temp_prms->ai2_cb_bytes_consumed[i4_subtu_idx] = num_bytes;

                        /* recon loop is done for non skip cases */
                        if(ps_best_cu_prms->u1_skip_flag == 0)
                        {
                            WORD32 tu_bits;

                            cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                                ps_ctxt,
                                pu1_cur_pred,
                                pred_strd,
                                pu1_cur_src,
                                chrm_src_stride,
                                pi2_cur_deq_data + trans_size,
                                deq_data_strd,
                                pu1_cur_recon,
                                i4_recon_stride,
                                pu1_ecd_data + total_bytes_offset,
                                ps_ctxt->au1_cu_csbf,
                                ps_ctxt->i4_cu_csbf_strd,
                                trans_size,
                                scan_idx,
                                PRED_MODE_INTRA == ps_best_cu_prms->u1_intra_flag,
                                &num_bytes,
                                &tu_bits,
                                &zero_cols,
                                &zero_rows,
                                &u1_is_recon_available,
                                i4_perform_sbh,
                                i4_perform_rdoq,
                                &trans_ssd_v,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                                i4_alpha_stim_multiplier,
                                u1_is_cu_noisy,
#endif
                                ps_best_cu_prms->u1_skip_flag,
                                u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                                V_PLANE);

                            if(u1_compute_spatial_ssd && u1_is_recon_available)
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }
                            else
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                            }

#if !USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                            {
#if !USE_RECON_TO_EVALUATE_STIM_IN_RDOPT
                                trans_ssd_v = ihevce_inject_stim_into_distortion(
                                    pu1_cur_src,
                                    chrm_src_stride,
                                    pu1_cur_pred,
                                    pred_strd,
                                    trans_ssd_v,
                                    i4_alpha_stim_multiplier,
                                    trans_size,
                                    0,
                                    ps_ctxt->u1_enable_psyRDOPT,
                                    V_PLANE);
#else
                                if(u1_compute_spatial_ssd && u1_is_recon_available)
                                {
                                    trans_ssd_v = ihevce_inject_stim_into_distortion(
                                        pu1_cur_src,
                                        chrm_src_stride,
                                        pu1_cur_recon,
                                        i4_recon_stride,
                                        trans_ssd_v,
                                        i4_alpha_stim_multiplier,
                                        trans_size,
                                        0,
                                        ps_ctxt->u1_enable_psyRDOPT,
                                        V_PLANE);
                                }
                                else
                                {
                                    trans_ssd_v = ihevce_inject_stim_into_distortion(
                                        pu1_cur_src,
                                        chrm_src_stride,
                                        pu1_cur_pred,
                                        pred_strd,
                                        trans_ssd_v,
                                        i4_alpha_stim_multiplier,
                                        trans_size,
                                        0,
                                        ps_ctxt->u1_enable_psyRDOPT,
                                        V_PLANE);
                                }
#endif
                            }
#endif

                            curr_cr_cod_cost =
                                trans_ssd_v +
                                COMPUTE_RATE_COST_CLIP30(
                                    tu_bits, ps_ctxt->i8_cl_ssd_lambda_chroma_qf, LAMBDA_Q_SHIFT);
                            chrm_tu_bits += tu_bits;
                            i4_bits_cr += tu_bits;

                            /* RDOPT copy States :  New updated after curr TU to TU init */
                            if(0 != cbf)
                            {
                                COPY_CABAC_STATES(
                                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                                    &ps_ctxt->s_rdopt_entropy_ctxt
                                         .as_cu_entropy_ctxt[rd_opt_curr_idx]
                                         .s_cabac_ctxt.au1_ctxt_models[0],
                                    IHEVC_CAB_CTXT_END);
                            }
                            /* RDOPT copy States :  Restoring back the Cb init state to Cr */
                            else
                            {
                                COPY_CABAC_STATES(
                                    &ps_ctxt->s_rdopt_entropy_ctxt
                                         .as_cu_entropy_ctxt[rd_opt_curr_idx]
                                         .s_cabac_ctxt.au1_ctxt_models[0],
                                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                                    IHEVC_CAB_CTXT_END);
                            }

                            /* If Intra and TU=CU/2, need recon for next TUs */
                            if(calc_recon)
                            {
                                ihevce_chroma_it_recon_fxn(
                                    ps_ctxt,
                                    (pi2_cur_deq_data + trans_size),
                                    deq_data_strd,
                                    pu1_cur_pred,
                                    pred_strd,
                                    pu1_cur_recon,
                                    i4_recon_stride,
                                    (pu1_ecd_data + total_bytes_offset),
                                    trans_size,
                                    cbf,
                                    zero_cols,
                                    zero_rows,
                                    V_PLANE);

                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }
                            else
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                            }
                        }
                        else
                        {
                            /* num bytes is set to 0 */
                            num_bytes = 0;

                            /* cbf is returned as 0 */
                            cbf = 0;

                            curr_cr_cod_cost = trans_ssd_v =

                                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_ssd_calculator(
                                    pu1_cur_pred,
                                    pu1_cur_src,
                                    pred_strd,
                                    chrm_src_stride,
                                    trans_size,
                                    trans_size,
                                    V_PLANE);

                            if(u1_compute_spatial_ssd)
                            {
                                /* buffer copy fromp pred to recon */
                                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                    pu1_cur_pred,
                                    pred_strd,
                                    pu1_cur_recon,
                                    i4_recon_stride,
                                    trans_size,
                                    trans_size,
                                    V_PLANE);

                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = 0;
                            }

                            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
                            {
                                trans_ssd_v = ihevce_inject_stim_into_distortion(
                                    pu1_cur_src,
                                    chrm_src_stride,
                                    pu1_cur_pred,
                                    pred_strd,
                                    trans_ssd_v,
                                    i4_alpha_stim_multiplier,
                                    trans_size,
                                    0,
                                    ps_ctxt->u1_enable_psyRDOPT,
                                    V_PLANE);
                            }

#if ENABLE_INTER_ZCU_COST
#if !WEIGH_CHROMA_COST
                            /* cbf = 0, accumulate cu not coded cost */
                            ps_ctxt->i8_cu_not_coded_cost += curr_cr_cod_cost;
#else
                            /* cbf = 0, accumulate cu not coded cost */

                            ps_ctxt->i8_cu_not_coded_cost += (LWORD64)(
                                (curr_cr_cod_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                                 (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                                CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif
#endif
                        }

#if !WEIGH_CHROMA_COST
                        curr_rdopt_cost += curr_cr_cod_cost;
#else
                        curr_rdopt_cost +=
                            ((curr_cr_cod_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                              (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                             CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif

                        chrm_cod_cost += curr_cr_cod_cost;
                        i8_ssd_cr += trans_ssd_v;

                        if(ps_ctxt->i4_bitrate_instance_num || ps_ctxt->i4_num_bitrates == 1)
                        {
                            /* Early exit : If the current running cost exceeds
                            the prev. best mode cost, break */
                            if(curr_rdopt_cost > prev_best_rdopt_cost)
                            {
                                u1_is_early_exit_condition_satisfied = 1;
                                break;
                            }
                        }

                        /* inter cu is coded if any of the tu is coded in it */
                        ps_best_cu_prms->u1_is_cu_coded |= cbf;

                        /* update CR related params */
                        ps_tu->ai4_cr_coeff_offset[i4_subtu_idx] =
                            total_bytes_offset + init_bytes_offset;

                        if(0 == i4_subtu_idx)
                        {
                            ps_tu->s_tu.b1_cr_cbf = cbf;
                        }
                        else
                        {
                            ps_tu->s_tu.b1_cr_cbf_subtu1 = cbf;
                        }

                        total_bytes_offset += num_bytes;

                        ps_tu_temp_prms->au4_cr_zero_col[i4_subtu_idx] = zero_cols;
                        ps_tu_temp_prms->au4_cr_zero_row[i4_subtu_idx] = zero_rows;
                        ps_tu_temp_prms->ai2_cr_bytes_consumed[i4_subtu_idx] = num_bytes;
                    }
                    else
                    {
                        ps_recon_datastore
                            ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][i4_subtu_idx] =
                            UCHAR_MAX;
                        ps_recon_datastore
                            ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][i4_subtu_idx] =
                            UCHAR_MAX;
                    }
                }

                if(u1_is_early_exit_condition_satisfied)
                {
                    break;
                }

                /* loop increments */
                ps_tu++;
                ps_tu_temp_prms++;
            }

            /* Signal as luma mode. HIGH_QUALITY may update it */
            ps_best_cu_prms->u1_chroma_intra_pred_mode = 4;

            /* modify the cost chrm_cod_cost */
            if(ps_ctxt->u1_enable_psyRDOPT)
            {
                UWORD8 *pu1_recon_cu;
                WORD32 recon_stride;
                WORD32 curr_pos_x;
                WORD32 curr_pos_y;
                WORD32 start_index;
                WORD32 num_horz_cu_in_ctb;
                WORD32 had_block_size;
                /* tODO: sreenivasa ctb size has to be used appropriately */
                had_block_size = 8;
                num_horz_cu_in_ctb = 2 * 64 / had_block_size;

                curr_pos_x = cu_pos_x << 3; /* pel units */
                curr_pos_y = cu_pos_y << 3; /* pel units */
                recon_stride = i4_recon_stride;
                pu1_recon_cu = pu1_recon;

                /* start index to index the source satd of curr cu int he current ctb*/
                start_index = 2 * (curr_pos_x / had_block_size) +
                              (curr_pos_y / had_block_size) * num_horz_cu_in_ctb;

                {
                    chrm_cod_cost += ihevce_psy_rd_cost_croma(
                        ps_ctxt->ai4_source_chroma_satd,
                        pu1_recon,
                        recon_stride,
                        1,  //
                        cu_size,
                        0,  // pic type
                        0,  //layer id
                        ps_ctxt->i4_satd_lamda,  // lambda
                        start_index,
                        ps_ctxt->u1_is_input_data_hbd,  // 8 bit
                        ps_ctxt->u1_chroma_array_type,
                        &ps_ctxt->s_cmn_opt_func

                    );  // chroma subsampling 420
                }
            }
        }
        else
        {
            u1_is_mode_eq_chroma_satd_mode = 1;
            chrm_cod_cost = MAX_COST_64;
        }

        /* If Intra Block and preset is HIGH QUALITY, then compare with best SATD mode */
        if((PRED_MODE_INTRA == ps_best_cu_prms->u1_intra_flag) &&
           (1 == ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd))
        {
            if(64 == cu_size)
            {
                ASSERT(TU_EQ_CU != func_proc_mode);
            }

            if(ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[func_proc_mode]
                   .i8_chroma_best_rdopt < chrm_cod_cost)
            {
                UWORD8 *pu1_src;
                UWORD8 *pu1_ecd_data_src_cb;
                UWORD8 *pu1_ecd_data_src_cr;

                chroma_intra_satd_ctxt_t *ps_chr_intra_satd_ctxt =
                    &ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[func_proc_mode];

                UWORD8 *pu1_dst = &ps_ctxt->au1_rdopt_init_ctxt_models[0];
                WORD32 ai4_ecd_data_cb_offset[2] = { 0, 0 };
                WORD32 ai4_ecd_data_cr_offset[2] = { 0, 0 };

                pu1_src = &ps_chr_intra_satd_ctxt->au1_chrm_satd_updated_ctxt_models[0];
                chrm_cod_cost = ps_chr_intra_satd_ctxt->i8_chroma_best_rdopt;
                chrm_pred_mode = ps_chr_intra_satd_ctxt->u1_best_cr_mode;
                chrm_tu_bits = ps_chr_intra_satd_ctxt->i4_chrm_tu_bits;

                if(u1_is_mode_eq_chroma_satd_mode)
                {
                    chrm_cod_cost -= ps_chr_intra_satd_ctxt->i8_cost_to_encode_chroma_mode;
                }

                /*Resetting total_num_bytes_to 0*/
                total_bytes_offset = 0;

                /* Update the CABAC state corresponding to chroma only */
                /* Chroma Cbf */
                memcpy(pu1_dst + IHEVC_CAB_CBCR_IDX, pu1_src + IHEVC_CAB_CBCR_IDX, 2);
                /* Chroma transform skip */
                memcpy(pu1_dst + IHEVC_CAB_TFM_SKIP12, pu1_src + IHEVC_CAB_TFM_SKIP12, 1);
                /* Chroma last coeff x prefix */
                memcpy(
                    pu1_dst + IHEVC_CAB_COEFFX_PREFIX + 15,
                    pu1_src + IHEVC_CAB_COEFFX_PREFIX + 15,
                    3);
                /* Chroma last coeff y prefix */
                memcpy(
                    pu1_dst + IHEVC_CAB_COEFFY_PREFIX + 15,
                    pu1_src + IHEVC_CAB_COEFFY_PREFIX + 15,
                    3);
                /* Chroma csbf */
                memcpy(
                    pu1_dst + IHEVC_CAB_CODED_SUBLK_IDX + 2,
                    pu1_src + IHEVC_CAB_CODED_SUBLK_IDX + 2,
                    2);
                /* Chroma sig coeff flags */
                memcpy(
                    pu1_dst + IHEVC_CAB_COEFF_FLAG + 27, pu1_src + IHEVC_CAB_COEFF_FLAG + 27, 15);
                /* Chroma absgt1 flags */
                memcpy(
                    pu1_dst + IHEVC_CAB_COEFABS_GRTR1_FLAG + 16,
                    pu1_src + IHEVC_CAB_COEFABS_GRTR1_FLAG + 16,
                    8);
                /* Chroma absgt2 flags */
                memcpy(
                    pu1_dst + IHEVC_CAB_COEFABS_GRTR2_FLAG + 4,
                    pu1_src + IHEVC_CAB_COEFABS_GRTR2_FLAG + 4,
                    2);

                ps_tu = &ps_best_cu_prms->as_tu_enc_loop[0];
                ps_tu_temp_prms = &ps_best_cu_prms->as_tu_enc_loop_temp_prms[0];

                /* update to luma decision as we update chroma in final mode */
                ps_best_cu_prms->u1_is_cu_coded = u1_is_cu_coded_old;

                for(ctr = 0; ctr < u1_num_tus; ctr++)
                {
                    for(i4_subtu_idx = 0; i4_subtu_idx < u1_num_subtus_in_tu; i4_subtu_idx++)
                    {
                        WORD32 cbf;
                        WORD32 num_bytes;

                        pu1_ecd_data_src_cb =
                            &ps_chr_intra_satd_ctxt->au1_scan_coeff_cb[i4_subtu_idx][0];
                        pu1_ecd_data_src_cr =
                            &ps_chr_intra_satd_ctxt->au1_scan_coeff_cr[i4_subtu_idx][0];

                        /* check if chroma present flag is set */
                        if(1 == ps_tu->s_tu.b3_chroma_intra_mode_idx)
                        {
                            UWORD8 *pu1_cur_pred_dest;
                            UWORD8 *pu1_cur_pred_src;
                            WORD32 pred_src_strd;
                            WORD16 *pi2_cur_deq_data_dest;
                            WORD16 *pi2_cur_deq_data_src_cb;
                            WORD16 *pi2_cur_deq_data_src_cr;
                            WORD32 deq_src_strd;

                            WORD32 curr_pos_x, curr_pos_y;

                            trans_size = ps_tu->s_tu.b3_size;
                            trans_size = (1 << (trans_size + 1)); /* in chroma units */

                            /*Deriving stride values*/
                            pred_src_strd = ps_chr_intra_satd_ctxt->i4_pred_stride;
                            deq_src_strd = ps_chr_intra_satd_ctxt->i4_iq_buff_stride;

                            /* since 2x2 transform is not allowed for chroma*/
                            if(2 == trans_size)
                            {
                                trans_size = 4;
                            }

                            /* get the current tu posx and posy w.r.t to cu */
                            curr_pos_x = (ps_tu->s_tu.b4_pos_x << 2) - (cu_pos_x << 3);
                            curr_pos_y = (ps_tu->s_tu.b4_pos_y << 2) - (cu_pos_y << 3) +
                                         (i4_subtu_idx * trans_size);

                            /* 420sp case only vertical height will be half */
                            if(0 == u1_is_422)
                            {
                                curr_pos_y >>= 1;
                            }

                            /* increment the pointers to start of current TU  */
                            pu1_cur_pred_src =
                                ((UWORD8 *)ps_chr_intra_satd_ctxt->pv_pred_data + curr_pos_x);
                            pu1_cur_pred_src += (curr_pos_y * pred_src_strd);
                            pu1_cur_pred_dest = (pu1_pred + curr_pos_x);
                            pu1_cur_pred_dest += (curr_pos_y * pred_strd);

                            pi2_cur_deq_data_src_cb =
                                &ps_chr_intra_satd_ctxt->ai2_iq_data_cb[0] + (curr_pos_x >> 1);
                            pi2_cur_deq_data_src_cr =
                                &ps_chr_intra_satd_ctxt->ai2_iq_data_cr[0] + (curr_pos_x >> 1);
                            pi2_cur_deq_data_src_cb += (curr_pos_y * deq_src_strd);
                            pi2_cur_deq_data_src_cr += (curr_pos_y * deq_src_strd);
                            pi2_cur_deq_data_dest = pi2_deq_data + curr_pos_x;
                            pi2_cur_deq_data_dest += (curr_pos_y * deq_data_strd);

                            /*Overwriting deq data with that belonging to the winning special mode
                            (luma mode !=  chroma mode)
                            ihevce_copy_2d takes source and dest arguments as UWORD8 *. We have to
                            correspondingly manipulate to copy WORD16 data*/

                            ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                                (UWORD8 *)pi2_cur_deq_data_dest,
                                (deq_data_strd << 1),
                                (UWORD8 *)pi2_cur_deq_data_src_cb,
                                (deq_src_strd << 1),
                                (trans_size << 1),
                                trans_size);

                            ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                                (UWORD8 *)(pi2_cur_deq_data_dest + trans_size),
                                (deq_data_strd << 1),
                                (UWORD8 *)pi2_cur_deq_data_src_cr,
                                (deq_src_strd << 1),
                                (trans_size << 1),
                                trans_size);

                            /*Overwriting pred data with that belonging to the winning special mode
                            (luma mode !=  chroma mode)*/

                            ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                                pu1_cur_pred_dest,
                                pred_strd,
                                pu1_cur_pred_src,
                                pred_src_strd,
                                (trans_size << 1),
                                trans_size);

                            num_bytes = ps_chr_intra_satd_ctxt
                                            ->ai4_num_bytes_scan_coeff_cb_per_tu[i4_subtu_idx][ctr];
                            cbf = ps_chr_intra_satd_ctxt->au1_cbf_cb[i4_subtu_idx][ctr];
                            /* inter cu is coded if any of the tu is coded in it */
                            ps_best_cu_prms->u1_is_cu_coded |= cbf;

                            /* update CB related params */
                            ps_tu->ai4_cb_coeff_offset[i4_subtu_idx] =
                                total_bytes_offset + init_bytes_offset;

                            if(0 == i4_subtu_idx)
                            {
                                ps_tu->s_tu.b1_cb_cbf = cbf;
                            }
                            else
                            {
                                ps_tu->s_tu.b1_cb_cbf_subtu1 = cbf;
                            }

                            /*Overwriting the cb ecd data corresponding to the special mode*/
                            if(0 != num_bytes)
                            {
                                memcpy(
                                    (pu1_ecd_data + total_bytes_offset),
                                    pu1_ecd_data_src_cb + ai4_ecd_data_cb_offset[i4_subtu_idx],
                                    num_bytes);
                            }

                            total_bytes_offset += num_bytes;
                            ai4_ecd_data_cb_offset[i4_subtu_idx] += num_bytes;
                            ps_tu_temp_prms->ai2_cb_bytes_consumed[i4_subtu_idx] = num_bytes;

                            num_bytes = ps_chr_intra_satd_ctxt
                                            ->ai4_num_bytes_scan_coeff_cr_per_tu[i4_subtu_idx][ctr];
                            cbf = ps_chr_intra_satd_ctxt->au1_cbf_cr[i4_subtu_idx][ctr];
                            /* inter cu is coded if any of the tu is coded in it */
                            ps_best_cu_prms->u1_is_cu_coded |= cbf;

                            /*Overwriting the cr ecd data corresponding to the special mode*/
                            if(0 != num_bytes)
                            {
                                memcpy(
                                    (pu1_ecd_data + total_bytes_offset),
                                    pu1_ecd_data_src_cr + ai4_ecd_data_cr_offset[i4_subtu_idx],
                                    num_bytes);
                            }

                            /* update CR related params */
                            ps_tu->ai4_cr_coeff_offset[i4_subtu_idx] =
                                total_bytes_offset + init_bytes_offset;

                            if(0 == i4_subtu_idx)
                            {
                                ps_tu->s_tu.b1_cr_cbf = cbf;
                            }
                            else
                            {
                                ps_tu->s_tu.b1_cr_cbf_subtu1 = cbf;
                            }

                            total_bytes_offset += num_bytes;
                            ai4_ecd_data_cr_offset[i4_subtu_idx] += num_bytes;

                            /*Updating zero rows and zero cols*/
                            ps_tu_temp_prms->au4_cb_zero_col[i4_subtu_idx] =
                                ps_chr_intra_satd_ctxt->ai4_zero_col_cb[i4_subtu_idx][ctr];
                            ps_tu_temp_prms->au4_cb_zero_row[i4_subtu_idx] =
                                ps_chr_intra_satd_ctxt->ai4_zero_row_cb[i4_subtu_idx][ctr];
                            ps_tu_temp_prms->au4_cr_zero_col[i4_subtu_idx] =
                                ps_chr_intra_satd_ctxt->ai4_zero_col_cr[i4_subtu_idx][ctr];
                            ps_tu_temp_prms->au4_cr_zero_row[i4_subtu_idx] =
                                ps_chr_intra_satd_ctxt->ai4_zero_row_cr[i4_subtu_idx][ctr];

                            ps_tu_temp_prms->ai2_cr_bytes_consumed[i4_subtu_idx] = num_bytes;

                            if((u1_num_tus > 1) &&
                               ps_recon_datastore->au1_is_chromaRecon_available[2])
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = 2;
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = 2;
                            }
                            else if(
                                (1 == u1_num_tus) &&
                                ps_recon_datastore->au1_is_chromaRecon_available[1])
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = 1;
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = 1;
                            }
                            else
                            {
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                                ps_recon_datastore
                                    ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr]
                                                                        [i4_subtu_idx] = UCHAR_MAX;
                            }
                        }
                    }

                    /* loop increments */
                    ps_tu++;
                    ps_tu_temp_prms++;
                }
            }

            if(!u1_is_422)
            {
                if(chrm_pred_mode == luma_pred_mode)
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 4;
                }
                else if(chrm_pred_mode == 0)
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 0;
                }
                else if(chrm_pred_mode == 1)
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 3;
                }
                else if(chrm_pred_mode == 10)
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 2;
                }
                else if(chrm_pred_mode == 26)
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 1;
                }
                else
                {
                    ASSERT(0); /*Should not come here*/
                }
            }
            else
            {
                if(chrm_pred_mode == gau1_chroma422_intra_angle_mapping[luma_pred_mode])
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 4;
                }
                else if(chrm_pred_mode == gau1_chroma422_intra_angle_mapping[0])
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 0;
                }
                else if(chrm_pred_mode == gau1_chroma422_intra_angle_mapping[1])
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 3;
                }
                else if(chrm_pred_mode == gau1_chroma422_intra_angle_mapping[10])
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 2;
                }
                else if(chrm_pred_mode == gau1_chroma422_intra_angle_mapping[26])
                {
                    ps_best_cu_prms->u1_chroma_intra_pred_mode = 1;
                }
                else
                {
                    ASSERT(0); /*Should not come here*/
                }
            }
        }

        /* Store the actual chroma mode */
        ps_best_cu_prms->u1_chroma_intra_pred_actual_mode = chrm_pred_mode;
    }

    /* update the total bytes produced */
    ps_best_cu_prms->i4_num_bytes_ecd_data = total_bytes_offset + init_bytes_offset;

    /* store the final chrm bits accumulated */
    *pi4_chrm_tu_bits = chrm_tu_bits;

    return (chrm_cod_cost);
}

/*!
******************************************************************************
* \if Function name : ihevce_final_rdopt_mode_prcs \endif
*
* \brief
*    Final RDOPT mode process function. Performs Recon computation for the
*    final mode. Re-use or Compute pred, iq-data, coeff based on the flags.
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_prms : pointer to struct containing requisite parameters
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_final_rdopt_mode_prcs(
    ihevce_enc_loop_ctxt_t *ps_ctxt, final_mode_process_prms_t *ps_prms)
{
    enc_loop_cu_final_prms_t *ps_best_cu_prms;
    tu_enc_loop_out_t *ps_tu_enc_loop;
    tu_enc_loop_temp_prms_t *ps_tu_enc_loop_temp_prms;
    nbr_avail_flags_t s_nbr;
    recon_datastore_t *ps_recon_datastore;

    ihevc_intra_pred_luma_ref_substitution_ft *ihevc_intra_pred_luma_ref_substitution_fptr;
    ihevc_intra_pred_chroma_ref_substitution_ft *ihevc_intra_pred_chroma_ref_substitution_fptr;
    ihevc_intra_pred_ref_filtering_ft *ihevc_intra_pred_ref_filtering_fptr;

    WORD32 num_tu_in_cu;
    LWORD64 rd_opt_cost;
    WORD32 ctr;
    WORD32 i4_subtu_idx;
    WORD32 cu_size;
    WORD32 cu_pos_x, cu_pos_y;
    WORD32 chrm_present_flag = 1;
    WORD32 num_bytes, total_bytes = 0;
    WORD32 chrm_ctr = 0;
    WORD32 u1_is_cu_coded;
    UWORD8 *pu1_old_ecd_data;
    UWORD8 *pu1_chrm_old_ecd_data;
    UWORD8 *pu1_cur_pred;
    WORD16 *pi2_deq_data;
    WORD16 *pi2_chrm_deq_data;
    WORD16 *pi2_cur_deq_data;
    WORD16 *pi2_cur_deq_data_chrm;
    UWORD8 *pu1_cur_luma_recon;
    UWORD8 *pu1_cur_chroma_recon;
    UWORD8 *pu1_cur_src;
    UWORD8 *pu1_cur_src_chrm;
    UWORD8 *pu1_cur_pred_chrm;
    UWORD8 *pu1_intra_pred_mode;
    UWORD32 *pu4_nbr_flags;
    LWORD64 i8_ssd;

    cu_nbr_prms_t *ps_cu_nbr_prms = ps_prms->ps_cu_nbr_prms;
    cu_inter_cand_t *ps_best_inter_cand = ps_prms->ps_best_inter_cand;
    enc_loop_chrm_cu_buf_prms_t *ps_chrm_cu_buf_prms = ps_prms->ps_chrm_cu_buf_prms;

    WORD32 packed_pred_mode = ps_prms->packed_pred_mode;
    WORD32 rd_opt_best_idx = ps_prms->rd_opt_best_idx;
    UWORD8 *pu1_src = (UWORD8 *)ps_prms->pv_src;
    WORD32 src_strd = ps_prms->src_strd;
    UWORD8 *pu1_pred = (UWORD8 *)ps_prms->pv_pred;
    WORD32 pred_strd = ps_prms->pred_strd;
    UWORD8 *pu1_pred_chrm = (UWORD8 *)ps_prms->pv_pred_chrm;
    WORD32 pred_chrm_strd = ps_prms->pred_chrm_strd;
    UWORD8 *pu1_final_ecd_data = ps_prms->pu1_final_ecd_data;
    UWORD8 *pu1_csbf_buf = ps_prms->pu1_csbf_buf;
    WORD32 csbf_strd = ps_prms->csbf_strd;
    UWORD8 *pu1_luma_recon = (UWORD8 *)ps_prms->pv_luma_recon;
    WORD32 recon_luma_strd = ps_prms->recon_luma_strd;
    UWORD8 *pu1_chrm_recon = (UWORD8 *)ps_prms->pv_chrm_recon;
    WORD32 recon_chrma_strd = ps_prms->recon_chrma_strd;
    UWORD8 u1_cu_pos_x = ps_prms->u1_cu_pos_x;
    UWORD8 u1_cu_pos_y = ps_prms->u1_cu_pos_y;
    UWORD8 u1_cu_size = ps_prms->u1_cu_size;
    WORD8 i1_cu_qp = ps_prms->i1_cu_qp;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);
    UWORD8 u1_num_subtus = (u1_is_422 == 1) + 1;
    /* Get the Chroma pointer and parameters */
    UWORD8 *pu1_src_chrm = ps_chrm_cu_buf_prms->pu1_curr_src;
    WORD32 src_chrm_strd = ps_chrm_cu_buf_prms->i4_chrm_src_stride;
    UWORD8 u1_compute_spatial_ssd_luma = 0;
    UWORD8 u1_compute_spatial_ssd_chroma = 0;
    /* Get the pointer for function selector */
    ihevc_intra_pred_luma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_luma_ref_substitution_fptr;

    ihevc_intra_pred_ref_filtering_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_ref_filtering_fptr;

    ihevc_intra_pred_chroma_ref_substitution_fptr =
        ps_ctxt->ps_func_selector->ihevc_intra_pred_chroma_ref_substitution_fptr;

    /* Get the best CU parameters */
    ps_best_cu_prms = &ps_ctxt->as_cu_prms[rd_opt_best_idx];
    num_tu_in_cu = ps_best_cu_prms->u2_num_tus_in_cu;
    cu_size = ps_best_cu_prms->u1_cu_size;
    cu_pos_x = u1_cu_pos_x;
    cu_pos_y = u1_cu_pos_y;
    pu1_intra_pred_mode = &ps_best_cu_prms->au1_intra_pred_mode[0];
    pu4_nbr_flags = &ps_best_cu_prms->au4_nbr_flags[0];
    ps_recon_datastore = &ps_best_cu_prms->s_recon_datastore;

    /* get the first TU pointer */
    ps_tu_enc_loop = &ps_best_cu_prms->as_tu_enc_loop[0];
    /* get the first TU only enc_loop prms pointer */
    ps_tu_enc_loop_temp_prms = &ps_best_cu_prms->as_tu_enc_loop_temp_prms[0];
    /*modify quant related param in ctxt based on current cu qp*/
    if((ps_ctxt->i1_cu_qp_delta_enable))
    {
        /*recompute quant related param at every cu level*/
        ihevce_compute_quant_rel_param(ps_ctxt, i1_cu_qp);

        /* get frame level lambda params */
        ihevce_get_cl_cu_lambda_prms(
            ps_ctxt, MODULATE_LAMDA_WHEN_SPATIAL_MOD_ON ? i1_cu_qp : ps_ctxt->i4_frame_qp);
    }

    ps_best_cu_prms->i8_cu_ssd = 0;
    ps_best_cu_prms->u4_cu_open_intra_sad = 0;

    /* For skip case : Set TU_size = CU_size and make cbf = 0
    so that same TU loop can be used for all modes */
    if(PRED_MODE_SKIP == packed_pred_mode)
    {
        for(ctr = 0; ctr < num_tu_in_cu; ctr++)
        {
            ps_tu_enc_loop->s_tu.b1_y_cbf = 0;

            ps_tu_enc_loop_temp_prms->i2_luma_bytes_consumed = 0;

            ps_tu_enc_loop++;
            ps_tu_enc_loop_temp_prms++;
        }

        /* go back to the first TU pointer */
        ps_tu_enc_loop = &ps_best_cu_prms->as_tu_enc_loop[0];
        ps_tu_enc_loop_temp_prms = &ps_best_cu_prms->as_tu_enc_loop_temp_prms[0];
    }
    /**   For inter case, pred calculation is outside the loop     **/
    if(PRED_MODE_INTRA != packed_pred_mode)
    {
        /**------------- Compute pred data if required --------------**/
        if((1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data))
        {
            nbr_4x4_t *ps_topleft_nbr_4x4;
            nbr_4x4_t *ps_left_nbr_4x4;
            nbr_4x4_t *ps_top_nbr_4x4;
            WORD32 nbr_4x4_left_strd;

            ps_best_inter_cand->pu1_pred_data = pu1_pred;
            ps_best_inter_cand->i4_pred_data_stride = pred_strd;

            /* Get the CU nbr information */
            ps_topleft_nbr_4x4 = ps_cu_nbr_prms->ps_topleft_nbr_4x4;
            ps_left_nbr_4x4 = ps_cu_nbr_prms->ps_left_nbr_4x4;
            ps_top_nbr_4x4 = ps_cu_nbr_prms->ps_top_nbr_4x4;
            nbr_4x4_left_strd = ps_cu_nbr_prms->nbr_4x4_left_strd;

            /* MVP ,MVD calc and Motion compensation */
            rd_opt_cost = ((pf_inter_rdopt_cu_mc_mvp)ps_ctxt->pv_inter_rdopt_cu_mc_mvp)(
                ps_ctxt,
                ps_best_inter_cand,
                u1_cu_size,
                cu_pos_x,
                cu_pos_y,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                ps_topleft_nbr_4x4,
                nbr_4x4_left_strd,
                rd_opt_best_idx);
        }

        /** ------ Motion Compensation for Chroma -------- **/
        if(1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data)
        {
            UWORD8 *pu1_cur_pred;
            pu1_cur_pred = pu1_pred_chrm;

            /* run a loop over all the partitons in cu */
            for(ctr = 0; ctr < ps_best_cu_prms->u2_num_pus_in_cu; ctr++)
            {
                pu_t *ps_pu;
                WORD32 inter_pu_wd, inter_pu_ht;

                ps_pu = &ps_best_cu_prms->as_pu_chrm_proc[ctr];

                /* IF AMP then each partitions can have diff wd ht */
                inter_pu_wd = (ps_pu->b4_wd + 1) << 2; /* cb and cr pixel interleaved */
                inter_pu_ht = ((ps_pu->b4_ht + 1) << 2) >> 1;
                inter_pu_ht <<= u1_is_422;
                /* chroma mc func */
                ihevce_chroma_inter_pred_pu(
                    &ps_ctxt->s_mc_ctxt, ps_pu, pu1_cur_pred, pred_chrm_strd);
                if(2 == ps_best_cu_prms->u2_num_pus_in_cu)
                {
                    /* 2Nx__ partion case */
                    if(inter_pu_wd == ps_best_cu_prms->u1_cu_size)
                    {
                        pu1_cur_pred += (inter_pu_ht * pred_chrm_strd);
                    }
                    /* __x2N partion case */
                    if(inter_pu_ht == (ps_best_cu_prms->u1_cu_size >> (u1_is_422 == 0)))
                    {
                        pu1_cur_pred += inter_pu_wd;
                    }
                }
            }
        }
    }
    pi2_deq_data = &ps_best_cu_prms->pi2_cu_deq_coeffs[0];
    pi2_chrm_deq_data =
        &ps_best_cu_prms->pi2_cu_deq_coeffs[0] + ps_best_cu_prms->i4_chrm_deq_coeff_strt_idx;
    pu1_old_ecd_data = &ps_best_cu_prms->pu1_cu_coeffs[0];
    pu1_chrm_old_ecd_data =
        &ps_best_cu_prms->pu1_cu_coeffs[0] + ps_best_cu_prms->i4_chrm_cu_coeff_strt_idx;

    /* default value for cu coded flag */
    u1_is_cu_coded = 0;

    /* If we are re-computing coeff, set sad to 0 and start accumulating */
    /* else use the best cand. sad from RDOPT stage                    */
    if(1 == ps_tu_enc_loop_temp_prms->b1_eval_luma_iq_and_coeff_data)
    {
        /*init of ssd of CU accuumulated over all TU*/
        ps_best_cu_prms->u4_cu_sad = 0;

        /* reset the luma residual bits */
        ps_best_cu_prms->u4_cu_luma_res_bits = 0;
    }

    if(1 == ps_tu_enc_loop_temp_prms->b1_eval_chroma_iq_and_coeff_data)
    {
        /* reset the chroma residual bits */
        ps_best_cu_prms->u4_cu_chroma_res_bits = 0;
    }

    if((1 == ps_tu_enc_loop_temp_prms->b1_eval_luma_iq_and_coeff_data) ||
       (1 == ps_tu_enc_loop_temp_prms->b1_eval_chroma_iq_and_coeff_data))
    {
        /*Header bits have to be reevaluated if luma and chroma reevaluation is done, as
        the quantized coefficients might be changed.
        We are copying only those states which correspond to the header from the cabac state
        of the previous CU, because the header is going to be recomputed for this condition*/
        ps_ctxt->s_cu_final_recon_flags.u1_eval_header_data = 1;
        memcpy(
            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
            &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
            IHEVC_CAB_COEFFX_PREFIX);

        if((1 == ps_tu_enc_loop_temp_prms->b1_eval_luma_iq_and_coeff_data))
        {
            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                (&ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX),
                (&ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0] +
                 IHEVC_CAB_COEFFX_PREFIX),
                (IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX));
        }
        else
        {
            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                (&ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX),
                (&ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                      .s_cabac_ctxt.au1_ctxt_models[0] +
                 IHEVC_CAB_COEFFX_PREFIX),
                (IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX));
        }
        ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx = rd_opt_best_idx;
    }
    else
    {
        ps_ctxt->s_cu_final_recon_flags.u1_eval_header_data = 0;
    }

    /* Zero cbf tool is disabled for intra CUs */
    if(PRED_MODE_INTRA == packed_pred_mode)
    {
#if ENABLE_ZERO_CBF_IN_INTRA
        ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;
#else
        ps_ctxt->i4_zcbf_rdo_level = NO_ZCBF;
#endif
    }
    else
    {
#if DISABLE_ZERO_ZBF_IN_INTER
        ps_ctxt->i4_zcbf_rdo_level = NO_ZCBF;
#else
        ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;
#endif
    }

    /** Loop for all tu blocks in current cu and do reconstruction **/
    for(ctr = 0; ctr < num_tu_in_cu; ctr++)
    {
        tu_t *ps_tu;
        WORD32 trans_size, num_4x4_in_tu;
        WORD32 cbf, zero_rows, zero_cols;
        WORD32 cu_pos_x_in_4x4, cu_pos_y_in_4x4;
        WORD32 cu_pos_x_in_pix, cu_pos_y_in_pix;
        WORD32 luma_pred_mode, chroma_pred_mode = 0;
        UWORD8 au1_is_recon_available[2];

        ps_tu = &(ps_tu_enc_loop->s_tu); /* Points to the TU property ctxt */

        u1_compute_spatial_ssd_luma = 0;
        u1_compute_spatial_ssd_chroma = 0;

        trans_size = 1 << (ps_tu->b3_size + 2);
        num_4x4_in_tu = (trans_size >> 2);
        cu_pos_x_in_4x4 = ps_tu->b4_pos_x;
        cu_pos_y_in_4x4 = ps_tu->b4_pos_y;

        /* populate the coeffs scan idx */
        ps_ctxt->i4_scan_idx = SCAN_DIAG_UPRIGHT;

        /* get the current pos x and pos y in pixels */
        cu_pos_x_in_pix = (cu_pos_x_in_4x4 << 2) - (cu_pos_x << 3);
        cu_pos_y_in_pix = (cu_pos_y_in_4x4 << 2) - (cu_pos_y << 3);

        /* Update pointers based on the location */
        pu1_cur_src = pu1_src + cu_pos_x_in_pix;
        pu1_cur_src += (cu_pos_y_in_pix * src_strd);
        pu1_cur_pred = pu1_pred + cu_pos_x_in_pix;
        pu1_cur_pred += (cu_pos_y_in_pix * pred_strd);

        pu1_cur_luma_recon = pu1_luma_recon + cu_pos_x_in_pix;
        pu1_cur_luma_recon += (cu_pos_y_in_pix * recon_luma_strd);

        pi2_cur_deq_data = pi2_deq_data + cu_pos_x_in_pix;
        pi2_cur_deq_data += cu_pos_y_in_pix * cu_size;

        pu1_cur_src_chrm = pu1_src_chrm + cu_pos_x_in_pix;
        pu1_cur_src_chrm += ((cu_pos_y_in_pix >> 1) * src_chrm_strd) +
                            (u1_is_422 * ((cu_pos_y_in_pix >> 1) * src_chrm_strd));

        pu1_cur_pred_chrm = pu1_pred_chrm + cu_pos_x_in_pix;
        pu1_cur_pred_chrm += ((cu_pos_y_in_pix >> 1) * pred_chrm_strd) +
                             (u1_is_422 * ((cu_pos_y_in_pix >> 1) * pred_chrm_strd));

        pu1_cur_chroma_recon = pu1_chrm_recon + cu_pos_x_in_pix;
        pu1_cur_chroma_recon += ((cu_pos_y_in_pix >> 1) * recon_chrma_strd) +
                                (u1_is_422 * ((cu_pos_y_in_pix >> 1) * recon_chrma_strd));

        pi2_cur_deq_data_chrm = pi2_chrm_deq_data + cu_pos_x_in_pix;
        pi2_cur_deq_data_chrm +=
            ((cu_pos_y_in_pix >> 1) * cu_size) + (u1_is_422 * ((cu_pos_y_in_pix >> 1) * cu_size));

        /* if transfrom size is 4x4 then only first luma 4x4 will have chroma*/
        chrm_present_flag = 1; /* by default chroma present is set to 1*/

        if(4 == trans_size)
        {
            /* if tusize is 4x4 then only first luma 4x4 will have chroma*/
            if(0 != chrm_ctr)
            {
                chrm_present_flag = INTRA_PRED_CHROMA_IDX_NONE;
            }

            /* increment the chrm ctr unconditionally */
            chrm_ctr++;
            /* after ctr reached 4 reset it */
            if(4 == chrm_ctr)
            {
                chrm_ctr = 0;
            }
        }

        /**------------- Compute pred data if required --------------**/
        if(PRED_MODE_INTRA == packed_pred_mode) /* Inter pred calc. is done outside loop */
        {
            /* Get the pred mode for scan idx calculation, even if pred is not required */
            luma_pred_mode = *pu1_intra_pred_mode;

            if((ps_ctxt->i4_rc_pass == 1) ||
               (1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data))
            {
                WORD32 nbr_flags;
                WORD32 luma_pred_func_idx;
                UWORD8 *pu1_left;
                UWORD8 *pu1_top;
                UWORD8 *pu1_top_left;
                WORD32 left_strd;

                /* left cu boundary */
                if(0 == cu_pos_x_in_pix)
                {
                    left_strd = ps_cu_nbr_prms->cu_left_stride;
                    pu1_left = ps_cu_nbr_prms->pu1_cu_left + cu_pos_y_in_pix * left_strd;
                }
                else
                {
                    pu1_left = pu1_cur_luma_recon - 1;
                    left_strd = recon_luma_strd;
                }

                /* top cu boundary */
                if(0 == cu_pos_y_in_pix)
                {
                    pu1_top = ps_cu_nbr_prms->pu1_cu_top + cu_pos_x_in_pix;
                }
                else
                {
                    pu1_top = pu1_cur_luma_recon - recon_luma_strd;
                }

                /* by default top left is set to cu top left */
                pu1_top_left = ps_cu_nbr_prms->pu1_cu_top_left;

                /* top left based on position */
                if((0 != cu_pos_y_in_pix) && (0 == cu_pos_x_in_pix))
                {
                    pu1_top_left = pu1_left - left_strd;
                }
                else if(0 != cu_pos_x_in_pix)
                {
                    pu1_top_left = pu1_top - 1;
                }

                /* get the neighbour availability flags */
                nbr_flags = ihevce_get_nbr_intra(
                    &s_nbr,
                    ps_ctxt->pu1_ctb_nbr_map,
                    ps_ctxt->i4_nbr_map_strd,
                    cu_pos_x_in_4x4,
                    cu_pos_y_in_4x4,
                    num_4x4_in_tu);

                if(1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data)
                {
                    /* copy the nbr flags for chroma reuse */
                    if(4 != trans_size)
                    {
                        *pu4_nbr_flags = nbr_flags;
                    }
                    else if(1 == chrm_present_flag)
                    {
                        /* compute the avail flags assuming luma trans is 8x8 */
                        /* get the neighbour availability flags */
                        *pu4_nbr_flags = ihevce_get_nbr_intra_mxn_tu(
                            ps_ctxt->pu1_ctb_nbr_map,
                            ps_ctxt->i4_nbr_map_strd,
                            cu_pos_x_in_4x4,
                            cu_pos_y_in_4x4,
                            (num_4x4_in_tu << 1),
                            (num_4x4_in_tu << 1));
                    }

                    /* call reference array substitution */
                    ihevc_intra_pred_luma_ref_substitution_fptr(
                        pu1_top_left,
                        pu1_top,
                        pu1_left,
                        left_strd,
                        trans_size,
                        nbr_flags,
                        (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                        1);

                    /* call reference filtering */
                    ihevc_intra_pred_ref_filtering_fptr(
                        (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                        trans_size,
                        (UWORD8 *)ps_ctxt->pv_ref_filt_out,
                        luma_pred_mode,
                        ps_ctxt->i1_strong_intra_smoothing_enable_flag);

                    /* use the look up to get the function idx */
                    luma_pred_func_idx = g_i4_ip_funcs[luma_pred_mode];

                    /* call the intra prediction function */
                    ps_ctxt->apf_lum_ip[luma_pred_func_idx](
                        (UWORD8 *)ps_ctxt->pv_ref_filt_out,
                        1,
                        pu1_cur_pred,
                        pred_strd,
                        trans_size,
                        luma_pred_mode);
                }
            }
            else if(
                (1 == chrm_present_flag) &&
                (1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data))
            {
                WORD32 temp_num_4x4_in_tu = num_4x4_in_tu;

                if(4 == trans_size) /* compute the avail flags assuming luma trans is 8x8 */
                {
                    temp_num_4x4_in_tu = num_4x4_in_tu << 1;
                }

                *pu4_nbr_flags = ihevce_get_nbr_intra_mxn_tu(
                    ps_ctxt->pu1_ctb_nbr_map,
                    ps_ctxt->i4_nbr_map_strd,
                    cu_pos_x_in_4x4,
                    cu_pos_y_in_4x4,
                    temp_num_4x4_in_tu,
                    temp_num_4x4_in_tu);
            }

            /* Get the pred mode for scan idx calculation, even if pred is not required */
            chroma_pred_mode = ps_best_cu_prms->u1_chroma_intra_pred_actual_mode;
        }

        if(1 == ps_tu_enc_loop_temp_prms->b1_eval_luma_iq_and_coeff_data)
        {
            WORD32 temp_bits;
            LWORD64 temp_cost;
            UWORD32 u4_tu_sad;
            WORD32 perform_sbh, perform_rdoq;

            if(PRED_MODE_INTRA == packed_pred_mode)
            {
                /* for luma 4x4 and 8x8 transforms based on intra pred mode scan is choosen*/
                if(trans_size < 16)
                {
                    /* for modes from 22 upto 30 horizontal scan is used */
                    if((luma_pred_mode > 21) && (luma_pred_mode < 31))
                    {
                        ps_ctxt->i4_scan_idx = SCAN_HORZ;
                    }
                    /* for modes from 6 upto 14 horizontal scan is used */
                    else if((luma_pred_mode > 5) && (luma_pred_mode < 15))
                    {
                        ps_ctxt->i4_scan_idx = SCAN_VERT;
                    }
                }
            }

            /* RDOPT copy States :  TU init (best until prev TU) to current */
            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                        .s_cabac_ctxt.au1_ctxt_models[0] +
                    IHEVC_CAB_COEFFX_PREFIX,
                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

            if(ps_prms->u1_recompute_sbh_and_rdoq)
            {
                perform_sbh = (ps_ctxt->i4_sbh_level != NO_SBH);
                perform_rdoq = (ps_ctxt->i4_rdoq_level != NO_RDOQ);
            }
            else
            {
                /* RDOQ will change the coefficients. If coefficients are changed, we will have to do sbh again*/
                perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh;
                /* To do SBH we need the quant and iquant data. This would mean we need to do quantization again, which would mean
                we would have to do RDOQ again.*/
                perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq;
            }

#if DISABLE_RDOQ_INTRA
            if(PRED_MODE_INTRA == packed_pred_mode)
            {
                perform_rdoq = 0;
            }
#endif
            /*If BEST candidate RDOQ is enabled, Eithe no coef level rdoq or CU level rdoq has to be enabled
            so that all candidates and best candidate are quantized with same rounding factor  */
            if(1 == perform_rdoq)
            {
                ASSERT(ps_ctxt->i4_quant_rounding_level != TU_LEVEL_QUANT_ROUNDING);
            }

            cbf = ihevce_t_q_iq_ssd_scan_fxn(
                ps_ctxt,
                pu1_cur_pred,
                pred_strd,
                pu1_cur_src,
                src_strd,
                pi2_cur_deq_data,
                cu_size, /*deq_data stride is cu_size*/
                pu1_cur_luma_recon,
                recon_luma_strd,
                pu1_final_ecd_data,
                pu1_csbf_buf,
                csbf_strd,
                trans_size,
                packed_pred_mode,
                &temp_cost,
                &num_bytes,
                &temp_bits,
                &u4_tu_sad,
                &zero_cols,
                &zero_rows,
                &au1_is_recon_available[0],
                perform_rdoq,  //(BEST_CAND_RDOQ == ps_ctxt->i4_rdoq_level),
                perform_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                !ps_ctxt->u1_is_refPic ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                       : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                          (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                             100.0,
                ps_prms->u1_is_cu_noisy,
#endif
                u1_compute_spatial_ssd_luma ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                1 /*early cbf*/
            );  //(BEST_CAND_SBH == ps_ctxt->i4_sbh_level));

            /* Accumulate luma residual bits */
            ps_best_cu_prms->u4_cu_luma_res_bits += temp_bits;

            /* RDOPT copy States :  New updated after curr TU to TU init */
            if(0 != cbf)
            {
                /* update to new state only if CBF is non zero */
                COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                    &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                    &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                            .s_cabac_ctxt.au1_ctxt_models[0] +
                        IHEVC_CAB_COEFFX_PREFIX,
                    IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
            }

            /* accumulate the TU sad into cu sad */
            ps_best_cu_prms->u4_cu_sad += u4_tu_sad;
            ps_tu->b1_y_cbf = cbf;
            ps_tu_enc_loop_temp_prms->i2_luma_bytes_consumed = num_bytes;

            /* If somebody updates cbf (RDOQ or SBH), update in nbr str. for BS */
            if((ps_prms->u1_will_cabac_state_change) && (!ps_prms->u1_is_first_pass))
            {
                WORD32 num_4x4_in_cu = u1_cu_size >> 2;
                nbr_4x4_t *ps_cur_nbr_4x4 = &ps_ctxt->as_cu_nbr[rd_opt_best_idx][0];
                ps_cur_nbr_4x4 = (ps_cur_nbr_4x4 + (cu_pos_x_in_pix >> 2));
                ps_cur_nbr_4x4 += ((cu_pos_y_in_pix >> 2) * num_4x4_in_cu);
                /* repiclate the nbr 4x4 structure for all 4x4 blocks current TU */
                ps_cur_nbr_4x4->b1_y_cbf = cbf;
                /*copy the cu qp. This will be overwritten by qp calculated based on skip flag at final stage of cu mode decide*/
                ps_cur_nbr_4x4->b8_qp = ps_ctxt->i4_cu_qp;
                /* Qp and cbf are stored for the all 4x4 in TU */
                {
                    WORD32 i, j;
                    nbr_4x4_t *ps_tmp_4x4;
                    ps_tmp_4x4 = ps_cur_nbr_4x4;

                    for(i = 0; i < num_4x4_in_tu; i++)
                    {
                        for(j = 0; j < num_4x4_in_tu; j++)
                        {
                            ps_tmp_4x4[j].b8_qp = ps_ctxt->i4_cu_qp;
                            ps_tmp_4x4[j].b1_y_cbf = cbf;
                        }
                        /* row level update*/
                        ps_tmp_4x4 += num_4x4_in_cu;
                    }
                }
            }
        }
        else
        {
            zero_cols = ps_tu_enc_loop_temp_prms->u4_luma_zero_col;
            zero_rows = ps_tu_enc_loop_temp_prms->u4_luma_zero_row;

            if(ps_prms->u1_will_cabac_state_change)
            {
                num_bytes = ps_tu_enc_loop_temp_prms->i2_luma_bytes_consumed;
            }
            else
            {
                num_bytes = 0;
            }

            /* copy luma ecd data to final buffer */
            memcpy(pu1_final_ecd_data, pu1_old_ecd_data, num_bytes);

            pu1_old_ecd_data += num_bytes;

            au1_is_recon_available[0] = 0;
        }

        /**-------- Compute Recon data (Do IT & Recon) : Luma  -----------**/
        if(ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data &&
           (!u1_compute_spatial_ssd_luma ||
            (!au1_is_recon_available[0] && u1_compute_spatial_ssd_luma)))
        {
            if(!ps_recon_datastore->u1_is_lumaRecon_available ||
               (ps_recon_datastore->u1_is_lumaRecon_available &&
                (UCHAR_MAX == ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr])))
            {
                ihevce_it_recon_fxn(
                    ps_ctxt,
                    pi2_cur_deq_data,
                    cu_size,
                    pu1_cur_pred,
                    pred_strd,
                    pu1_cur_luma_recon,
                    recon_luma_strd,
                    pu1_final_ecd_data,
                    trans_size,
                    packed_pred_mode,
                    ps_tu->b1_y_cbf,
                    zero_cols,
                    zero_rows);
            }
            else if(
                ps_recon_datastore->u1_is_lumaRecon_available &&
                (UCHAR_MAX != ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr]))
            {
                UWORD8 *pu1_recon_src =
                    ((UWORD8 *)ps_recon_datastore->apv_luma_recon_bufs
                         [ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr]]) +
                    cu_pos_x_in_pix + cu_pos_y_in_pix * ps_recon_datastore->i4_lumaRecon_stride;

                ps_ctxt->s_cmn_opt_func.pf_copy_2d(
                    pu1_cur_luma_recon,
                    recon_luma_strd,
                    pu1_recon_src,
                    ps_recon_datastore->i4_lumaRecon_stride,
                    trans_size,
                    trans_size);
            }
        }

        if(ps_prms->u1_will_cabac_state_change)
        {
            ps_tu_enc_loop->i4_luma_coeff_offset = total_bytes;
        }

        pu1_final_ecd_data += num_bytes;
        /* update total bytes consumed */
        total_bytes += num_bytes;

        u1_is_cu_coded |= ps_tu->b1_y_cbf;

        /***************** Compute T,Q,IQ,IT & Recon for Chroma ********************/
        if(1 == chrm_present_flag)
        {
            pu1_cur_src_chrm = pu1_src_chrm + cu_pos_x_in_pix;
            pu1_cur_src_chrm += ((cu_pos_y_in_pix >> 1) * src_chrm_strd) +
                                (u1_is_422 * ((cu_pos_y_in_pix >> 1) * src_chrm_strd));

            pu1_cur_pred_chrm = pu1_pred_chrm + cu_pos_x_in_pix;
            pu1_cur_pred_chrm += ((cu_pos_y_in_pix >> 1) * pred_chrm_strd) +
                                 (u1_is_422 * ((cu_pos_y_in_pix >> 1) * pred_chrm_strd));

            pu1_cur_chroma_recon = pu1_chrm_recon + cu_pos_x_in_pix;
            pu1_cur_chroma_recon += ((cu_pos_y_in_pix >> 1) * recon_chrma_strd) +
                                    (u1_is_422 * ((cu_pos_y_in_pix >> 1) * recon_chrma_strd));

            pi2_cur_deq_data_chrm = pi2_chrm_deq_data + cu_pos_x_in_pix;
            pi2_cur_deq_data_chrm += ((cu_pos_y_in_pix >> 1) * cu_size) +
                                     (u1_is_422 * ((cu_pos_y_in_pix >> 1) * cu_size));

            if(INCLUDE_CHROMA_DURING_TU_RECURSION &&
               (ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P0) &&
               (PRED_MODE_INTRA != packed_pred_mode))
            {
                WORD32 i4_num_bytes;
                UWORD8 *pu1_chroma_pred;
                UWORD8 *pu1_chroma_recon;
                WORD16 *pi2_chroma_deq;
                UWORD32 u4_zero_col;
                UWORD32 u4_zero_row;

                for(i4_subtu_idx = 0; i4_subtu_idx < u1_num_subtus; i4_subtu_idx++)
                {
                    WORD32 chroma_trans_size = MAX(4, trans_size >> 1);
                    WORD32 i4_subtu_pos_x = cu_pos_x_in_pix;
                    WORD32 i4_subtu_pos_y = cu_pos_y_in_pix + (i4_subtu_idx * chroma_trans_size);

                    if(0 == u1_is_422)
                    {
                        i4_subtu_pos_y >>= 1;
                    }

                    pu1_chroma_pred =
                        pu1_cur_pred_chrm + (i4_subtu_idx * chroma_trans_size * pred_chrm_strd);
                    pu1_chroma_recon = pu1_cur_chroma_recon +
                                       (i4_subtu_idx * chroma_trans_size * recon_chrma_strd);
                    pi2_chroma_deq =
                        pi2_cur_deq_data_chrm + (i4_subtu_idx * chroma_trans_size * cu_size);

                    u4_zero_col = ps_tu_enc_loop_temp_prms->au4_cb_zero_col[i4_subtu_idx];
                    u4_zero_row = ps_tu_enc_loop_temp_prms->au4_cb_zero_row[i4_subtu_idx];

                    if(ps_prms->u1_will_cabac_state_change)
                    {
                        i4_num_bytes =
                            ps_tu_enc_loop_temp_prms->ai2_cb_bytes_consumed[i4_subtu_idx];
                    }
                    else
                    {
                        i4_num_bytes = 0;
                    }

                    memcpy(pu1_final_ecd_data, pu1_old_ecd_data, i4_num_bytes);

                    pu1_old_ecd_data += i4_num_bytes;

                    au1_is_recon_available[U_PLANE] = 0;

                    if(ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data &&
                       (!u1_compute_spatial_ssd_chroma ||
                        (!au1_is_recon_available[U_PLANE] && u1_compute_spatial_ssd_chroma)))
                    {
                        if(!ps_recon_datastore->au1_is_chromaRecon_available[0] ||
                           (ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX ==
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][i4_subtu_idx])))
                        {
                            ihevce_chroma_it_recon_fxn(
                                ps_ctxt,
                                pi2_chroma_deq,
                                cu_size,
                                pu1_chroma_pred,
                                pred_chrm_strd,
                                pu1_chroma_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data,
                                chroma_trans_size,
                                (i4_subtu_idx == 0) ? ps_tu->b1_cb_cbf : ps_tu->b1_cb_cbf_subtu1,
                                u4_zero_col,
                                u4_zero_row,
                                U_PLANE);
                        }
                        else if(
                            ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX !=
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][i4_subtu_idx]))
                        {
                            UWORD8 *pu1_recon_src =
                                ((UWORD8 *)ps_recon_datastore->apv_chroma_recon_bufs
                                     [ps_recon_datastore->au1_bufId_with_winning_ChromaRecon
                                          [U_PLANE][ctr][i4_subtu_idx]]) +
                                i4_subtu_pos_x +
                                i4_subtu_pos_y * ps_recon_datastore->i4_chromaRecon_stride;

                            ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                pu1_recon_src,
                                ps_recon_datastore->i4_lumaRecon_stride,
                                pu1_chroma_recon,
                                recon_chrma_strd,
                                chroma_trans_size,
                                chroma_trans_size,
                                U_PLANE);
                        }
                    }

                    u1_is_cu_coded |=
                        ((1 == i4_subtu_idx) ? ps_tu->b1_cb_cbf_subtu1 : ps_tu->b1_cb_cbf);

                    pu1_final_ecd_data += i4_num_bytes;
                    total_bytes += i4_num_bytes;
                }

                for(i4_subtu_idx = 0; i4_subtu_idx < u1_num_subtus; i4_subtu_idx++)
                {
                    WORD32 chroma_trans_size = MAX(4, trans_size >> 1);
                    WORD32 i4_subtu_pos_x = cu_pos_x_in_pix;
                    WORD32 i4_subtu_pos_y = cu_pos_y_in_pix + (i4_subtu_idx * chroma_trans_size);

                    if(0 == u1_is_422)
                    {
                        i4_subtu_pos_y >>= 1;
                    }

                    pu1_chroma_pred =
                        pu1_cur_pred_chrm + (i4_subtu_idx * chroma_trans_size * pred_chrm_strd);
                    pu1_chroma_recon = pu1_cur_chroma_recon +
                                       (i4_subtu_idx * chroma_trans_size * recon_chrma_strd);
                    pi2_chroma_deq = pi2_cur_deq_data_chrm +
                                     (i4_subtu_idx * chroma_trans_size * cu_size) +
                                     chroma_trans_size;

                    u4_zero_col = ps_tu_enc_loop_temp_prms->au4_cr_zero_col[i4_subtu_idx];
                    u4_zero_row = ps_tu_enc_loop_temp_prms->au4_cr_zero_row[i4_subtu_idx];

                    if(ps_prms->u1_will_cabac_state_change)
                    {
                        i4_num_bytes =
                            ps_tu_enc_loop_temp_prms->ai2_cr_bytes_consumed[i4_subtu_idx];
                    }
                    else
                    {
                        i4_num_bytes = 0;
                    }

                    memcpy(pu1_final_ecd_data, pu1_old_ecd_data, i4_num_bytes);

                    pu1_old_ecd_data += i4_num_bytes;

                    au1_is_recon_available[V_PLANE] = 0;

                    if(ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data &&
                       (!u1_compute_spatial_ssd_chroma ||
                        (!au1_is_recon_available[V_PLANE] && u1_compute_spatial_ssd_chroma)))
                    {
                        if(!ps_recon_datastore->au1_is_chromaRecon_available[0] ||
                           (ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX ==
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][i4_subtu_idx])))
                        {
                            ihevce_chroma_it_recon_fxn(
                                ps_ctxt,
                                pi2_chroma_deq,
                                cu_size,
                                pu1_chroma_pred,
                                pred_chrm_strd,
                                pu1_chroma_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data,
                                chroma_trans_size,
                                (i4_subtu_idx == 0) ? ps_tu->b1_cr_cbf : ps_tu->b1_cr_cbf_subtu1,
                                u4_zero_col,
                                u4_zero_row,
                                V_PLANE);
                        }
                        else if(
                            ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX !=
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][i4_subtu_idx]))
                        {
                            UWORD8 *pu1_recon_src =
                                ((UWORD8 *)ps_recon_datastore->apv_chroma_recon_bufs
                                     [ps_recon_datastore->au1_bufId_with_winning_ChromaRecon
                                          [V_PLANE][ctr][i4_subtu_idx]]) +
                                i4_subtu_pos_x +
                                i4_subtu_pos_y * ps_recon_datastore->i4_chromaRecon_stride;

                            ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                pu1_recon_src,
                                ps_recon_datastore->i4_lumaRecon_stride,
                                pu1_chroma_recon,
                                recon_chrma_strd,
                                chroma_trans_size,
                                chroma_trans_size,
                                V_PLANE);
                        }
                    }

                    u1_is_cu_coded |=
                        ((1 == i4_subtu_idx) ? ps_tu->b1_cr_cbf_subtu1 : ps_tu->b1_cr_cbf);

                    pu1_final_ecd_data += i4_num_bytes;
                    total_bytes += i4_num_bytes;
                }
            }
            else
            {
                WORD32 cb_zero_col, cb_zero_row, cr_zero_col, cr_zero_row;

                for(i4_subtu_idx = 0; i4_subtu_idx < u1_num_subtus; i4_subtu_idx++)
                {
                    WORD32 cb_cbf, cr_cbf;
                    WORD32 cb_num_bytes, cr_num_bytes;

                    WORD32 chroma_trans_size = MAX(4, trans_size >> 1);

                    WORD32 i4_subtu_pos_x = cu_pos_x_in_pix;
                    WORD32 i4_subtu_pos_y = cu_pos_y_in_pix + (i4_subtu_idx * chroma_trans_size);

                    if(0 == u1_is_422)
                    {
                        i4_subtu_pos_y >>= 1;
                    }

                    pu1_cur_src_chrm += (i4_subtu_idx * chroma_trans_size * src_chrm_strd);
                    pu1_cur_pred_chrm += (i4_subtu_idx * chroma_trans_size * pred_chrm_strd);
                    pu1_cur_chroma_recon += (i4_subtu_idx * chroma_trans_size * recon_chrma_strd);
                    pi2_cur_deq_data_chrm += (i4_subtu_idx * chroma_trans_size * cu_size);

                    if((PRED_MODE_INTRA == packed_pred_mode) &&
                       (1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data))
                    {
                        WORD32 nbr_flags, left_strd_chrm, chrm_pred_func_idx;
                        UWORD8 *pu1_left_chrm;
                        UWORD8 *pu1_top_chrm;
                        UWORD8 *pu1_top_left_chrm;

                        nbr_flags = ihevce_get_intra_chroma_tu_nbr(
                            *pu4_nbr_flags, i4_subtu_idx, chroma_trans_size, u1_is_422);

                        /* left cu boundary */
                        if(0 == i4_subtu_pos_x)
                        {
                            left_strd_chrm = ps_chrm_cu_buf_prms->i4_cu_left_stride;
                            pu1_left_chrm =
                                ps_chrm_cu_buf_prms->pu1_cu_left + i4_subtu_pos_y * left_strd_chrm;
                        }
                        else
                        {
                            pu1_left_chrm = pu1_cur_chroma_recon - 2;
                            left_strd_chrm = recon_chrma_strd;
                        }

                        /* top cu boundary */
                        if(0 == i4_subtu_pos_y)
                        {
                            pu1_top_chrm = ps_chrm_cu_buf_prms->pu1_cu_top + i4_subtu_pos_x;
                        }
                        else
                        {
                            pu1_top_chrm = pu1_cur_chroma_recon - recon_chrma_strd;
                        }

                        /* by default top left is set to cu top left */
                        pu1_top_left_chrm = ps_chrm_cu_buf_prms->pu1_cu_top_left;

                        /* top left based on position */
                        if((0 != i4_subtu_pos_y) && (0 == i4_subtu_pos_x))
                        {
                            pu1_top_left_chrm = pu1_left_chrm - left_strd_chrm;
                        }
                        else if(0 != i4_subtu_pos_x)
                        {
                            pu1_top_left_chrm = pu1_top_chrm - 2;
                        }

                        /* call the chroma reference array substitution */
                        ihevc_intra_pred_chroma_ref_substitution_fptr(
                            pu1_top_left_chrm,
                            pu1_top_chrm,
                            pu1_left_chrm,
                            left_strd_chrm,
                            chroma_trans_size,
                            nbr_flags,
                            (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                            1);

                        /* use the look up to get the function idx */
                        chrm_pred_func_idx = g_i4_ip_funcs[chroma_pred_mode];

                        /* call the intra prediction function */
                        ps_ctxt->apf_chrm_ip[chrm_pred_func_idx](
                            (UWORD8 *)ps_ctxt->pv_ref_sub_out,
                            1,
                            pu1_cur_pred_chrm,
                            pred_chrm_strd,
                            chroma_trans_size,
                            chroma_pred_mode);
                    }

                    /**---------- Compute iq&coeff data if required : Chroma ------------**/
                    if(1 == ps_tu_enc_loop_temp_prms->b1_eval_chroma_iq_and_coeff_data)
                    {
                        WORD32 perform_sbh, perform_rdoq, temp_bits;

                        if(ps_prms->u1_recompute_sbh_and_rdoq)
                        {
                            perform_sbh = (ps_ctxt->i4_sbh_level != NO_SBH);
                            perform_rdoq = (ps_ctxt->i4_rdoq_level != NO_RDOQ);
                        }
                        else
                        {
                            /* RDOQ will change the coefficients. If coefficients are changed, we will have to do sbh again*/
                            perform_sbh = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh;
                            /* To do SBH we need the quant and iquant data. This would mean we need to do quantization again, which would mean
                        we would have to do RDOQ again.*/
                            perform_rdoq = ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq;
                        }

                        /* populate the coeffs scan idx */
                        ps_ctxt->i4_scan_idx = SCAN_DIAG_UPRIGHT;

                        if(PRED_MODE_INTRA == packed_pred_mode)
                        {
                            /* for 4x4 transforms based on intra pred mode scan is choosen*/
                            if(4 == chroma_trans_size)
                            {
                                /* for modes from 22 upto 30 horizontal scan is used */
                                if((chroma_pred_mode > 21) && (chroma_pred_mode < 31))
                                {
                                    ps_ctxt->i4_scan_idx = SCAN_HORZ;
                                }
                                /* for modes from 6 upto 14 horizontal scan is used */
                                else if((chroma_pred_mode > 5) && (chroma_pred_mode < 15))
                                {
                                    ps_ctxt->i4_scan_idx = SCAN_VERT;
                                }
                            }
                        }

#if DISABLE_RDOQ_INTRA
                        if(PRED_MODE_INTRA == packed_pred_mode)
                        {
                            perform_rdoq = 0;
                        }
#endif

                        /* RDOPT copy States :  TU init (best until prev TU) to current */
                        COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                                    .s_cabac_ctxt.au1_ctxt_models[0] +
                                IHEVC_CAB_COEFFX_PREFIX,
                            &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                            IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);

                        ASSERT(rd_opt_best_idx == ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx);
                        /*If BEST candidate RDOQ is enabled, Eithe no coef level rdoq or CU level rdoq has to be enabled
                    so that all candidates and best candidate are quantized with same rounding factor  */
                        if(1 == perform_rdoq)
                        {
                            ASSERT(ps_ctxt->i4_quant_rounding_level != TU_LEVEL_QUANT_ROUNDING);
                        }

                        if(!ps_best_cu_prms->u1_skip_flag ||
                           !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt)
                        {
                            /* Cb */
                            cb_cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                                ps_ctxt,
                                pu1_cur_pred_chrm,
                                pred_chrm_strd,
                                pu1_cur_src_chrm,
                                src_chrm_strd,
                                pi2_cur_deq_data_chrm,
                                cu_size,
                                pu1_chrm_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data,
                                pu1_csbf_buf,
                                csbf_strd,
                                chroma_trans_size,
                                ps_ctxt->i4_scan_idx,
                                (PRED_MODE_INTRA == packed_pred_mode),
                                &cb_num_bytes,
                                &temp_bits,
                                &cb_zero_col,
                                &cb_zero_row,
                                &au1_is_recon_available[U_PLANE],
                                perform_sbh,
                                perform_rdoq,
                                &i8_ssd,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                                !ps_ctxt->u1_is_refPic
                                    ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                    : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                       (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                          100.0,
                                ps_prms->u1_is_cu_noisy,
#endif
                                ps_best_cu_prms->u1_skip_flag &&
                                    ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt,
                                u1_compute_spatial_ssd_chroma ? SPATIAL_DOMAIN_SSD
                                                              : FREQUENCY_DOMAIN_SSD,
                                U_PLANE);
                        }
                        else
                        {
                            cb_cbf = 0;
                            temp_bits = 0;
                            cb_num_bytes = 0;
                            au1_is_recon_available[U_PLANE] = 0;
                            cb_zero_col = 0;
                            cb_zero_row = 0;
                        }

                        /* Accumulate chroma residual bits */
                        ps_best_cu_prms->u4_cu_chroma_res_bits += temp_bits;

                        /* RDOPT copy States :  New updated after curr TU to TU init */
                        if(0 != cb_cbf)
                        {
                            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                                        .s_cabac_ctxt.au1_ctxt_models[0] +
                                    IHEVC_CAB_COEFFX_PREFIX,
                                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
                        }
                        /* RDOPT copy States :  Restoring back the Cb init state to Cr */
                        else
                        {
                            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                                        .s_cabac_ctxt.au1_ctxt_models[0] +
                                    IHEVC_CAB_COEFFX_PREFIX,
                                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
                        }

                        if(!ps_best_cu_prms->u1_skip_flag ||
                           !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt)
                        {
                            /* Cr */
                            cr_cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                                ps_ctxt,
                                pu1_cur_pred_chrm,
                                pred_chrm_strd,
                                pu1_cur_src_chrm,
                                src_chrm_strd,
                                pi2_cur_deq_data_chrm + chroma_trans_size,
                                cu_size,
                                pu1_chrm_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data + cb_num_bytes,
                                pu1_csbf_buf,
                                csbf_strd,
                                chroma_trans_size,
                                ps_ctxt->i4_scan_idx,
                                (PRED_MODE_INTRA == packed_pred_mode),
                                &cr_num_bytes,
                                &temp_bits,
                                &cr_zero_col,
                                &cr_zero_row,
                                &au1_is_recon_available[V_PLANE],
                                perform_sbh,
                                perform_rdoq,
                                &i8_ssd,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                                !ps_ctxt->u1_is_refPic
                                    ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                    : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                       (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                          100.0,
                                ps_prms->u1_is_cu_noisy,
#endif
                                ps_best_cu_prms->u1_skip_flag &&
                                    ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt,
                                u1_compute_spatial_ssd_chroma ? SPATIAL_DOMAIN_SSD
                                                              : FREQUENCY_DOMAIN_SSD,
                                V_PLANE);
                        }
                        else
                        {
                            cr_cbf = 0;
                            temp_bits = 0;
                            cr_num_bytes = 0;
                            au1_is_recon_available[V_PLANE] = 0;
                            cr_zero_col = 0;
                            cr_zero_row = 0;
                        }

                        /* Accumulate chroma residual bits */
                        ps_best_cu_prms->u4_cu_chroma_res_bits += temp_bits;

                        /* RDOPT copy States :  New updated after curr TU to TU init */
                        if(0 != cr_cbf)
                        {
                            COPY_CABAC_STATES_FRM_CAB_COEFFX_PREFIX(
                                &ps_ctxt->au1_rdopt_init_ctxt_models[0] + IHEVC_CAB_COEFFX_PREFIX,
                                &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                                        .s_cabac_ctxt.au1_ctxt_models[0] +
                                    IHEVC_CAB_COEFFX_PREFIX,
                                IHEVC_CAB_CTXT_END - IHEVC_CAB_COEFFX_PREFIX);
                        }

                        if(0 == i4_subtu_idx)
                        {
                            ps_tu->b1_cb_cbf = cb_cbf;
                            ps_tu->b1_cr_cbf = cr_cbf;
                        }
                        else
                        {
                            ps_tu->b1_cb_cbf_subtu1 = cb_cbf;
                            ps_tu->b1_cr_cbf_subtu1 = cr_cbf;
                        }
                    }
                    else
                    {
                        cb_zero_col = ps_tu_enc_loop_temp_prms->au4_cb_zero_col[i4_subtu_idx];
                        cb_zero_row = ps_tu_enc_loop_temp_prms->au4_cb_zero_row[i4_subtu_idx];
                        cr_zero_col = ps_tu_enc_loop_temp_prms->au4_cr_zero_col[i4_subtu_idx];
                        cr_zero_row = ps_tu_enc_loop_temp_prms->au4_cr_zero_row[i4_subtu_idx];

                        if(ps_prms->u1_will_cabac_state_change)
                        {
                            cb_num_bytes =
                                ps_tu_enc_loop_temp_prms->ai2_cb_bytes_consumed[i4_subtu_idx];
                        }
                        else
                        {
                            cb_num_bytes = 0;
                        }

                        if(ps_prms->u1_will_cabac_state_change)
                        {
                            cr_num_bytes =
                                ps_tu_enc_loop_temp_prms->ai2_cr_bytes_consumed[i4_subtu_idx];
                        }
                        else
                        {
                            cr_num_bytes = 0;
                        }

                        /* copy cb ecd data to final buffer */
                        memcpy(pu1_final_ecd_data, pu1_chrm_old_ecd_data, cb_num_bytes);

                        pu1_chrm_old_ecd_data += cb_num_bytes;

                        /* copy cb ecd data to final buffer */
                        memcpy(
                            (pu1_final_ecd_data + cb_num_bytes),
                            pu1_chrm_old_ecd_data,
                            cr_num_bytes);

                        pu1_chrm_old_ecd_data += cr_num_bytes;

                        au1_is_recon_available[U_PLANE] = 0;
                        au1_is_recon_available[V_PLANE] = 0;
                    }

                    /**-------- Compute Recon data (Do IT & Recon) : Chroma  -----------**/
                    if(ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data &&
                       (!u1_compute_spatial_ssd_chroma ||
                        (!au1_is_recon_available[U_PLANE] && u1_compute_spatial_ssd_chroma)))
                    {
                        if(!ps_recon_datastore->au1_is_chromaRecon_available[0] ||
                           (ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX ==
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][i4_subtu_idx])))
                        {
                            ihevce_chroma_it_recon_fxn(
                                ps_ctxt,
                                pi2_cur_deq_data_chrm,
                                cu_size,
                                pu1_cur_pred_chrm,
                                pred_chrm_strd,
                                pu1_cur_chroma_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data,
                                chroma_trans_size,
                                (i4_subtu_idx == 0) ? ps_tu->b1_cb_cbf : ps_tu->b1_cb_cbf_subtu1,
                                cb_zero_col,
                                cb_zero_row,
                                U_PLANE);
                        }
                        else if(
                            ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX !=
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][i4_subtu_idx]))
                        {
                            UWORD8 *pu1_recon_src =
                                ((UWORD8 *)ps_recon_datastore->apv_chroma_recon_bufs
                                     [ps_recon_datastore->au1_bufId_with_winning_ChromaRecon
                                          [U_PLANE][ctr][i4_subtu_idx]]) +
                                i4_subtu_pos_x +
                                i4_subtu_pos_y * ps_recon_datastore->i4_chromaRecon_stride;

                            ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                pu1_recon_src,
                                ps_recon_datastore->i4_lumaRecon_stride,
                                pu1_cur_chroma_recon,
                                recon_chrma_strd,
                                chroma_trans_size,
                                chroma_trans_size,
                                U_PLANE);
                        }
                    }

                    u1_is_cu_coded |=
                        ((1 == i4_subtu_idx) ? ps_tu->b1_cb_cbf_subtu1 : ps_tu->b1_cb_cbf);

                    if(ps_prms->u1_will_cabac_state_change)
                    {
                        ps_tu_enc_loop->ai4_cb_coeff_offset[i4_subtu_idx] = total_bytes;
                    }

                    pu1_final_ecd_data += cb_num_bytes;
                    /* update total bytes consumed */
                    total_bytes += cb_num_bytes;

                    if(ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data &&
                       (!u1_compute_spatial_ssd_chroma ||
                        (!au1_is_recon_available[V_PLANE] && u1_compute_spatial_ssd_chroma)))
                    {
                        if(!ps_recon_datastore->au1_is_chromaRecon_available[0] ||
                           (ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX ==
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][i4_subtu_idx])))
                        {
                            ihevce_chroma_it_recon_fxn(
                                ps_ctxt,
                                pi2_cur_deq_data_chrm + chroma_trans_size,
                                cu_size,
                                pu1_cur_pred_chrm,
                                pred_chrm_strd,
                                pu1_cur_chroma_recon,
                                recon_chrma_strd,
                                pu1_final_ecd_data,
                                chroma_trans_size,
                                (i4_subtu_idx == 0) ? ps_tu->b1_cr_cbf : ps_tu->b1_cr_cbf_subtu1,
                                cr_zero_col,
                                cr_zero_row,
                                V_PLANE);
                        }
                        else if(
                            ps_recon_datastore->au1_is_chromaRecon_available[0] &&
                            (UCHAR_MAX !=
                             ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][i4_subtu_idx]))
                        {
                            UWORD8 *pu1_recon_src =
                                ((UWORD8 *)ps_recon_datastore->apv_chroma_recon_bufs
                                     [ps_recon_datastore->au1_bufId_with_winning_ChromaRecon
                                          [V_PLANE][ctr][i4_subtu_idx]]) +
                                i4_subtu_pos_x +
                                i4_subtu_pos_y * ps_recon_datastore->i4_chromaRecon_stride;

                            ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                                pu1_recon_src,
                                ps_recon_datastore->i4_lumaRecon_stride,
                                pu1_cur_chroma_recon,
                                recon_chrma_strd,
                                chroma_trans_size,
                                chroma_trans_size,
                                V_PLANE);
                        }
                    }

                    u1_is_cu_coded |=
                        ((1 == i4_subtu_idx) ? ps_tu->b1_cr_cbf_subtu1 : ps_tu->b1_cr_cbf);

                    if(ps_prms->u1_will_cabac_state_change)
                    {
                        ps_tu_enc_loop->ai4_cr_coeff_offset[i4_subtu_idx] = total_bytes;
                    }

                    pu1_final_ecd_data += cr_num_bytes;
                    /* update total bytes consumed */
                    total_bytes += cr_num_bytes;
                }
            }
        }
        else
        {
            ps_tu_enc_loop->ai4_cb_coeff_offset[0] = total_bytes;
            ps_tu_enc_loop->ai4_cr_coeff_offset[0] = total_bytes;
            ps_tu_enc_loop->ai4_cb_coeff_offset[1] = total_bytes;
            ps_tu_enc_loop->ai4_cr_coeff_offset[1] = total_bytes;
            ps_tu->b1_cb_cbf = 0;
            ps_tu->b1_cr_cbf = 0;
            ps_tu->b1_cb_cbf_subtu1 = 0;
            ps_tu->b1_cr_cbf_subtu1 = 0;
        }

        /* Update to next TU */
        ps_tu_enc_loop++;
        ps_tu_enc_loop_temp_prms++;

        pu4_nbr_flags++;
        pu1_intra_pred_mode++;

        /*Do not set the nbr map for last pu in cu */
        if((num_tu_in_cu - 1) != ctr)
        {
            /* set the neighbour map to 1 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                cu_pos_x_in_4x4,
                cu_pos_y_in_4x4,
                (trans_size >> 2),
                1);
        }
    }

    if(ps_prms->u1_will_cabac_state_change)
    {
        ps_best_cu_prms->u1_is_cu_coded = u1_is_cu_coded;

        /* Modify skip flag, if luma is skipped & Chroma is coded */
        if((1 == u1_is_cu_coded) && (PRED_MODE_SKIP == packed_pred_mode))
        {
            ps_best_cu_prms->u1_skip_flag = 0;
        }
    }

    /* during chroma evaluation if skip decision was over written     */
    /* then the current skip candidate is set to a non skip candidate */
    if(PRED_MODE_INTRA != packed_pred_mode)
    {
        ps_best_inter_cand->b1_skip_flag = ps_best_cu_prms->u1_skip_flag;
    }

    /**------------- Compute header data if required --------------**/
    if(1 == ps_ctxt->s_cu_final_recon_flags.u1_eval_header_data)
    {
        WORD32 cbf_bits;
        WORD32 cu_bits;
        WORD32 unit_4x4_size = cu_size >> 2;

        /*Restoring the running reference into the best rdopt_ctxt cabac states which will then
        be copied as the base reference for the next cu
        Assumption : We are ensuring that the u1_eval_header_data flag is set to 1 only if either
        luma and chroma are being reevaluated*/
        COPY_CABAC_STATES(
            &ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[rd_opt_best_idx]
                 .s_cabac_ctxt.au1_ctxt_models[0],
            &ps_ctxt->au1_rdopt_init_ctxt_models[0],
            IHEVC_CAB_CTXT_END);

        /* get the neighbour availability flags for current cu  */
        ihevce_get_only_nbr_flag(
            &s_nbr,
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            (cu_pos_x << 1),
            (cu_pos_y << 1),
            unit_4x4_size,
            unit_4x4_size);

        cu_bits = ihevce_entropy_rdo_encode_cu(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_best_cu_prms,
            cu_pos_x,
            cu_pos_y,
            cu_size,
            ps_ctxt->u1_disable_intra_eval ? !DISABLE_TOP_SYNC && s_nbr.u1_top_avail
                                           : s_nbr.u1_top_avail,
            s_nbr.u1_left_avail,
            (pu1_final_ecd_data - total_bytes),
            &cbf_bits);

        /* cbf bits are excluded from header bits, instead considered as texture bits */
        ps_best_cu_prms->u4_cu_hdr_bits = cu_bits - cbf_bits;
        ps_best_cu_prms->u4_cu_cbf_bits = cbf_bits;
    }

    if(ps_prms->u1_will_cabac_state_change)
    {
        ps_best_cu_prms->i4_num_bytes_ecd_data = total_bytes;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_set_eval_flags \endif
*
* \brief
*    Function which decides which eval flags have to be set based on present
*    and RDOQ conditions
*
* \param[in] ps_ctxt : encoder ctxt pointer
* \param[in] enc_loop_cu_final_prms_t : pointer to final cu params
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_set_eval_flags(
    ihevce_enc_loop_ctxt_t *ps_ctxt, enc_loop_cu_final_prms_t *ps_enc_loop_bestprms)
{
    WORD32 count = 0;

    ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data = 0;

    ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data =
        !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;

    if(ps_ctxt->u1_disable_intra_eval && (!(ps_ctxt->i4_deblk_pad_hpel_cur_pic & 0x1)))
    {
        ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data = 0;
    }
    else
    {
        ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data = 1;
    }

    if((1 == ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq) ||
       (1 == ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh))
    {
        /* When rdoq is enabled only for the best candidate, in case of in Intra nTU
        RDOQ might have altered the coeffs of the neighbour CU. As a result, the pred
        for the current CU will change. Therefore, we need to reevaluate the pred data*/
        if((ps_enc_loop_bestprms->u2_num_tus_in_cu > 1) &&
           (ps_enc_loop_bestprms->u1_intra_flag == 1))
        {
            ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data = 1;
            ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data = 1;
        }
        if(ps_enc_loop_bestprms->u1_skip_flag == 1)
        {
            for(count = 0; count < ps_enc_loop_bestprms->u2_num_tus_in_cu; count++)
            {
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_luma_iq_and_coeff_data = 0;
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_chroma_iq_and_coeff_data = 0;
            }
        }
        else
        {
            for(count = 0; count < ps_enc_loop_bestprms->u2_num_tus_in_cu; count++)
            {
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_luma_iq_and_coeff_data = 1;
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_chroma_iq_and_coeff_data = 1;
            }
        }
    }
    else
    {
        switch(ps_ctxt->i4_quality_preset)
        {
        case IHEVCE_QUALITY_P0:
        case IHEVCE_QUALITY_P2:
        case IHEVCE_QUALITY_P3:
        {
            for(count = 0; count < ps_enc_loop_bestprms->u2_num_tus_in_cu; count++)
            {
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_luma_iq_and_coeff_data = 0;
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_chroma_iq_and_coeff_data =
                    !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;
            }

            break;
        }
        case IHEVCE_QUALITY_P4:
        case IHEVCE_QUALITY_P5:
        {
            for(count = 0; count < ps_enc_loop_bestprms->u2_num_tus_in_cu; count++)
            {
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_luma_iq_and_coeff_data = 0;
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_chroma_iq_and_coeff_data =
                    !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;
            }

            break;
        }
        case IHEVCE_QUALITY_P6:
        {
            for(count = 0; count < ps_enc_loop_bestprms->u2_num_tus_in_cu; count++)
            {
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_luma_iq_and_coeff_data = 0;
#if !ENABLE_CHROMA_TRACKING_OF_LUMA_CBF_IN_XS25
                ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                    .b1_eval_chroma_iq_and_coeff_data =
                    !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;
#else
                if((ps_ctxt->i1_slice_type == BSLICE) && (ps_ctxt->i4_temporal_layer_id > 1) &&
                   (ps_enc_loop_bestprms->as_tu_enc_loop[count].s_tu.b3_size >= 2))
                {
                    ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                        .b1_eval_chroma_iq_and_coeff_data =
                        ps_enc_loop_bestprms->as_tu_enc_loop[count].s_tu.b1_y_cbf;
                }
                else
                {
                    ps_enc_loop_bestprms->as_tu_enc_loop_temp_prms[count]
                        .b1_eval_chroma_iq_and_coeff_data =
                        !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;
                }
#endif
            }

            break;
        }
        default:
        {
            break;
        }
        }
    }

    /* Not recomputing Luma pred-data and header data for any preset now */
    ps_ctxt->s_cu_final_recon_flags.u1_eval_header_data = 1;
}

/**
******************************************************************************
*
*  @brief Shrink's TU tree of inter CUs by merging redundnant child nodes
*         (not coded children) into a parent node(not coded).
*
*  @par   Description
*         This is required post RDO evaluation as TU decisions are
*         pre-determined(pre RDO) based on recursive SATD,
*         while the quad children TU's can be skipped during RDO
*
*         The shrink process is applied iteratively till there are no
*         more modes to shrink
*
*  @param[inout]   ps_tu_enc_loop
*       pointer to tu enc loop params of inter cu
*
*  @param[inout]   ps_tu_enc_loop_temp_prms
*       pointer to temp tu enc loop params of inter cu
*
*  @param[in]   num_tu_in_cu
*       number of tus in cu
*
*  @return      modified number of tus in cu
*
******************************************************************************
*/
WORD32 ihevce_shrink_inter_tu_tree(
    tu_enc_loop_out_t *ps_tu_enc_loop,
    tu_enc_loop_temp_prms_t *ps_tu_enc_loop_temp_prms,
    recon_datastore_t *ps_recon_datastore,
    WORD32 num_tu_in_cu,
    UWORD8 u1_is_422)
{
    WORD32 recurse = 1;
    WORD32 ctr;

    /* ------------- Quadtree TU Split Transform flag optimization ------------  */
    /* Post RDO, if all 4 child nodes are not coded the overheads of split TU    */
    /* flags and cbf flags are saved by merging to parent node and marking       */
    /* parent TU as not coded                                                    */
    /*                                                                           */
    /*                               ParentTUSplit=1                             */
    /*                                      |                                    */
    /*       ---------------------------------------------------------           */
    /*       |C0(Not coded) | C1(Not coded) | C2(Not coded) | C3(Not coded)      */
    /*                                     ||                                    */
    /*                                     \/                                    */
    /*                                                                           */
    /*                              ParentTUSplit=0 (Not Coded)                  */
    /*                                                                           */
    /* ------------- Quadtree TU Split Transform flag optimization ------------  */
    while((num_tu_in_cu > 4) && recurse)
    {
        recurse = 0;

        /* Validate inter CU */
        //ASSERT(ps_tu_enc_loop[0].s_tu.s_tu.b1_intra_flag == 0); /*b1_intra_flag no longer a member of tu structure */

        /* loop for all tu blocks in current cu */
        for(ctr = 0; ctr < num_tu_in_cu;)
        {
            /* Get current tu posx, posy and size */
            WORD32 curr_pos_x = ps_tu_enc_loop[ctr].s_tu.b4_pos_x << 2;
            WORD32 curr_pos_y = ps_tu_enc_loop[ctr].s_tu.b4_pos_y << 2;
            /* +1 is for parents size */
            WORD32 parent_tu_size = 1 << (ps_tu_enc_loop[ctr].s_tu.b3_size + 2 + 1);

            /* eval merge if leaf nodes reached i.e all child tus are of same size and first tu pos is same as parent pos */
            WORD32 eval_merge = ((curr_pos_x & (parent_tu_size - 1)) == 0);
            eval_merge &= ((curr_pos_y & (parent_tu_size - 1)) == 0);

            /* As TUs are published in encode order (Z SCAN),                      */
            /* Four consecutive TUS of same size implies we have hit leaf nodes.   */
            if(((ps_tu_enc_loop[ctr].s_tu.b3_size) == (ps_tu_enc_loop[ctr + 1].s_tu.b3_size)) &&
               ((ps_tu_enc_loop[ctr].s_tu.b3_size) == (ps_tu_enc_loop[ctr + 2].s_tu.b3_size)) &&
               ((ps_tu_enc_loop[ctr].s_tu.b3_size) == (ps_tu_enc_loop[ctr + 3].s_tu.b3_size)) &&
               eval_merge)
            {
                WORD32 merge_parent = 1;

                /* If any leaf noded is coded, it cannot be merged to parent */
                if((ps_tu_enc_loop[ctr].s_tu.b1_y_cbf) || (ps_tu_enc_loop[ctr].s_tu.b1_cb_cbf) ||
                   (ps_tu_enc_loop[ctr].s_tu.b1_cr_cbf) ||

                   (ps_tu_enc_loop[ctr + 1].s_tu.b1_y_cbf) ||
                   (ps_tu_enc_loop[ctr + 1].s_tu.b1_cb_cbf) ||
                   (ps_tu_enc_loop[ctr + 1].s_tu.b1_cr_cbf) ||

                   (ps_tu_enc_loop[ctr + 2].s_tu.b1_y_cbf) ||
                   (ps_tu_enc_loop[ctr + 2].s_tu.b1_cb_cbf) ||
                   (ps_tu_enc_loop[ctr + 2].s_tu.b1_cr_cbf) ||

                   (ps_tu_enc_loop[ctr + 3].s_tu.b1_y_cbf) ||
                   (ps_tu_enc_loop[ctr + 3].s_tu.b1_cb_cbf) ||
                   (ps_tu_enc_loop[ctr + 3].s_tu.b1_cr_cbf))
                {
                    merge_parent = 0;
                }

                if(u1_is_422)
                {
                    if((ps_tu_enc_loop[ctr].s_tu.b1_cb_cbf_subtu1) ||
                       (ps_tu_enc_loop[ctr].s_tu.b1_cr_cbf_subtu1) ||

                       (ps_tu_enc_loop[ctr + 1].s_tu.b1_cb_cbf_subtu1) ||
                       (ps_tu_enc_loop[ctr + 1].s_tu.b1_cr_cbf_subtu1) ||

                       (ps_tu_enc_loop[ctr + 2].s_tu.b1_cb_cbf_subtu1) ||
                       (ps_tu_enc_loop[ctr + 2].s_tu.b1_cr_cbf_subtu1) ||

                       (ps_tu_enc_loop[ctr + 3].s_tu.b1_cb_cbf_subtu1) ||
                       (ps_tu_enc_loop[ctr + 3].s_tu.b1_cr_cbf_subtu1))
                    {
                        merge_parent = 0;
                    }
                }

                if(merge_parent)
                {
                    /* Merge all the children (ctr,ctr+1,ctr+2,ctr+3) to parent (ctr) */

                    if(ps_recon_datastore->u1_is_lumaRecon_available)
                    {
                        ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr] = UCHAR_MAX;

                        memmove(
                            &ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr + 1],
                            &ps_recon_datastore->au1_bufId_with_winning_LumaRecon[ctr + 4],
                            (num_tu_in_cu - ctr - 4) * sizeof(UWORD8));
                    }

                    if(ps_recon_datastore->au1_is_chromaRecon_available[0])
                    {
                        ps_recon_datastore->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][0] =
                            UCHAR_MAX;
                        ps_recon_datastore->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][0] =
                            UCHAR_MAX;

                        memmove(
                            &ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr + 1][0],
                            &ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr + 4][0],
                            (num_tu_in_cu - ctr - 4) * sizeof(UWORD8));

                        memmove(
                            &ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr + 1][0],
                            &ps_recon_datastore
                                 ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr + 4][0],
                            (num_tu_in_cu - ctr - 4) * sizeof(UWORD8));

                        if(u1_is_422)
                        {
                            ps_recon_datastore->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr][1] =
                                UCHAR_MAX;
                            ps_recon_datastore->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr][1] =
                                UCHAR_MAX;

                            memmove(
                                &ps_recon_datastore
                                     ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr + 1][1],
                                &ps_recon_datastore
                                     ->au1_bufId_with_winning_ChromaRecon[U_PLANE][ctr + 4][1],
                                (num_tu_in_cu - ctr - 4) * sizeof(UWORD8));

                            memmove(
                                &ps_recon_datastore
                                     ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr + 1][1],
                                &ps_recon_datastore
                                     ->au1_bufId_with_winning_ChromaRecon[V_PLANE][ctr + 4][1],
                                (num_tu_in_cu - ctr - 4) * sizeof(UWORD8));
                        }
                    }

                    /* Parent node size is one more than that of child */
                    ps_tu_enc_loop[ctr].s_tu.b3_size++;

                    ctr++;

                    /* move the subsequent TUs to next element */
                    ASSERT(num_tu_in_cu >= (ctr + 3));
                    memmove(
                        (void *)(ps_tu_enc_loop + ctr),
                        (void *)(ps_tu_enc_loop + ctr + 3),
                        (num_tu_in_cu - ctr - 3) * sizeof(tu_enc_loop_out_t));

                    /* Also memmove the temp TU params */
                    memmove(
                        (void *)(ps_tu_enc_loop_temp_prms + ctr),
                        (void *)(ps_tu_enc_loop_temp_prms + ctr + 3),
                        (num_tu_in_cu - ctr - 3) * sizeof(tu_enc_loop_temp_prms_t));

                    /* Number of TUs in CU are now less by 3 */
                    num_tu_in_cu -= 3;

                    /* Recurse again as new parent also be can be merged later */
                    recurse = 1;
                }
                else
                {
                    /* Go to next set of leaf nodes */
                    ctr += 4;
                }
            }
            else
            {
                ctr++;
            }
        }
    }

    /* return the modified num TUs*/
    ASSERT(num_tu_in_cu > 0);
    return (num_tu_in_cu);
}

UWORD8 ihevce_intra_mode_nxn_hash_updater(
    UWORD8 *pu1_mode_array, UWORD8 *pu1_hash_table, UWORD8 u1_num_ipe_modes)
{
    WORD32 i;
    WORD32 i4_mode;

    for(i = 0; i < MAX_INTRA_CU_CANDIDATES; i++)
    {
        if(pu1_mode_array[i] < 35)
        {
            if(pu1_mode_array[i] != 0)
            {
                i4_mode = pu1_mode_array[i] - 1;

                if(!pu1_hash_table[i4_mode])
                {
                    pu1_hash_table[i4_mode] = 1;
                    pu1_mode_array[u1_num_ipe_modes] = i4_mode;
                    u1_num_ipe_modes++;
                }
            }

            if(pu1_mode_array[i] != 34)
            {
                i4_mode = pu1_mode_array[i] + 1;

                if((!pu1_hash_table[i4_mode]))
                {
                    pu1_hash_table[i4_mode] = 1;
                    pu1_mode_array[u1_num_ipe_modes] = i4_mode;
                    u1_num_ipe_modes++;
                }
            }
        }
    }

    if(!pu1_hash_table[INTRA_PLANAR])
    {
        pu1_hash_table[INTRA_PLANAR] = 1;
        pu1_mode_array[u1_num_ipe_modes] = INTRA_PLANAR;
        u1_num_ipe_modes++;
    }

    if(!pu1_hash_table[INTRA_DC])
    {
        pu1_hash_table[INTRA_DC] = 1;
        pu1_mode_array[u1_num_ipe_modes] = INTRA_DC;
        u1_num_ipe_modes++;
    }

    return u1_num_ipe_modes;
}

#if ENABLE_TU_TREE_DETERMINATION_IN_RDOPT
WORD32 ihevce_determine_tu_tree_distribution(
    cu_inter_cand_t *ps_cu_data,
    me_func_selector_t *ps_func_selector,
    WORD16 *pi2_scratch_mem,
    UWORD8 *pu1_inp,
    WORD32 i4_inp_stride,
    WORD32 i4_lambda,
    UWORD8 u1_lambda_q_shift,
    UWORD8 u1_cu_size,
    UWORD8 u1_max_tr_depth)
{
    err_prms_t s_err_prms;

    PF_SAD_FXN_TU_REC pf_err_compute[4];

    WORD32 i4_satd;

    s_err_prms.pi4_sad_grid = &i4_satd;
    s_err_prms.pi4_tu_split_flags = ps_cu_data->ai4_tu_split_flag;
    s_err_prms.pu1_inp = pu1_inp;
    s_err_prms.pu1_ref = ps_cu_data->pu1_pred_data;
    s_err_prms.i4_inp_stride = i4_inp_stride;
    s_err_prms.i4_ref_stride = ps_cu_data->i4_pred_data_stride;
    s_err_prms.pu1_wkg_mem = (UWORD8 *)pi2_scratch_mem;

    if(u1_cu_size == 64)
    {
        s_err_prms.u1_max_tr_depth = MIN(1, u1_max_tr_depth);
    }
    else
    {
        s_err_prms.u1_max_tr_depth = u1_max_tr_depth;
    }

    pf_err_compute[CU_64x64] = hme_evalsatd_pt_pu_64x64_tu_rec;
    pf_err_compute[CU_32x32] = hme_evalsatd_pt_pu_32x32_tu_rec;
    pf_err_compute[CU_16x16] = hme_evalsatd_pt_pu_16x16_tu_rec;
    pf_err_compute[CU_8x8] = hme_evalsatd_pt_pu_8x8_tu_rec;

    i4_satd = pf_err_compute[hme_get_range(u1_cu_size) - 4](
        &s_err_prms, i4_lambda, u1_lambda_q_shift, 0, ps_func_selector);

    if((0 == u1_max_tr_depth) && (ps_cu_data->b3_part_size != 0) && (u1_cu_size != 64))
    {
        ps_cu_data->ai4_tu_split_flag[0] = 1;
    }

    return i4_satd;
}
#endif

void ihevce_populate_nbr_4x4_with_pu_data(
    nbr_4x4_t *ps_nbr_4x4, pu_t *ps_pu, WORD32 i4_nbr_buf_stride)
{
    WORD32 i, j;

    nbr_4x4_t *ps_tmp_4x4 = ps_nbr_4x4;

    WORD32 ht = (ps_pu->b4_ht + 1);
    WORD32 wd = (ps_pu->b4_wd + 1);

    ps_nbr_4x4->b1_intra_flag = 0;
    ps_nbr_4x4->b1_pred_l0_flag = !(ps_pu->b2_pred_mode & 1);
    ps_nbr_4x4->b1_pred_l1_flag = (ps_pu->b2_pred_mode > PRED_L0);
    ps_nbr_4x4->mv = ps_pu->mv;

    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            ps_tmp_4x4[j] = *ps_nbr_4x4;
        }

        ps_tmp_4x4 += i4_nbr_buf_stride;
    }
}

void ihevce_call_luma_inter_pred_rdopt_pass1(
    ihevce_enc_loop_ctxt_t *ps_ctxt, cu_inter_cand_t *ps_inter_cand, WORD32 cu_size)
{
    pu_t *ps_pu;
    UWORD8 *pu1_pred;
    WORD32 pred_stride, ctr, num_cu_part, skip_or_merge_flag = 0;
    WORD32 inter_pu_wd, inter_pu_ht;

    pu1_pred = ps_inter_cand->pu1_pred_data_scr;
    pred_stride = ps_inter_cand->i4_pred_data_stride;
    num_cu_part = (SIZE_2Nx2N != ps_inter_cand->b3_part_size) + 1;

    for(ctr = 0; ctr < num_cu_part; ctr++)
    {
        ps_pu = &ps_inter_cand->as_inter_pu[ctr];

        /* IF AMP then each partitions can have diff wd ht */
        inter_pu_wd = (ps_pu->b4_wd + 1) << 2;
        inter_pu_ht = (ps_pu->b4_ht + 1) << 2;

        skip_or_merge_flag = ps_inter_cand->b1_skip_flag | ps_pu->b1_merge_flag;
        //if(0 == skip_or_merge_flag)
        {
            ihevce_luma_inter_pred_pu(&ps_ctxt->s_mc_ctxt, ps_pu, pu1_pred, pred_stride, 1);
        }
        if((2 == num_cu_part) && (0 == ctr))
        {
            /* 2Nx__ partion case */
            if(inter_pu_wd == cu_size)
            {
                pu1_pred += (inter_pu_ht * pred_stride);
            }

            /* __x2N partion case */
            if(inter_pu_ht == cu_size)
            {
                pu1_pred += inter_pu_wd;
            }
        }
    }
}

LWORD64 ihevce_it_recon_ssd(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_src,
    WORD32 i4_src_strd,
    UWORD8 *pu1_pred,
    WORD32 i4_pred_strd,
    WORD16 *pi2_deq_data,
    WORD32 i4_deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 u1_trans_size,
    UWORD8 u1_pred_mode,
    WORD32 i4_cbf,
    WORD32 i4_zero_col,
    WORD32 i4_zero_row,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    if(NULL_PLANE == e_chroma_plane)
    {
        ihevce_it_recon_fxn(
            ps_ctxt,
            pi2_deq_data,
            i4_deq_data_strd,
            pu1_pred,
            i4_pred_strd,
            pu1_recon,
            i4_recon_stride,
            pu1_ecd_data,
            u1_trans_size,
            u1_pred_mode,
            i4_cbf,
            i4_zero_col,
            i4_zero_row);

        return ps_ctxt->s_cmn_opt_func.pf_ssd_calculator(
            pu1_recon, pu1_src, i4_recon_stride, i4_src_strd, u1_trans_size, u1_trans_size,
            e_chroma_plane);
    }
    else
    {
        ihevce_chroma_it_recon_fxn(
            ps_ctxt,
            pi2_deq_data,
            i4_deq_data_strd,
            pu1_pred,
            i4_pred_strd,
            pu1_recon,
            i4_recon_stride,
            pu1_ecd_data,
            u1_trans_size,
            i4_cbf,
            i4_zero_col,
            i4_zero_row,
            e_chroma_plane);

        return ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_ssd_calculator(
            pu1_recon,
            pu1_src,
            i4_recon_stride,
            i4_src_strd,
            u1_trans_size,
            u1_trans_size,
            e_chroma_plane);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_t_q_iq_ssd_scan_fxn \endif
*
* \brief
*    Transform unit level (Chroma) enc_loop function
*
* \param[in] ps_ctxt    enc_loop module ctxt pointer
* \param[in] pu1_pred       pointer to predicted data buffer
* \param[in] pred_strd      predicted buffer stride
* \param[in] pu1_src    pointer to source data buffer
* \param[in] src_strd   source buffer stride
* \param[in] pi2_deq_data   pointer to store iq data
* \param[in] deq_data_strd  iq data buffer stride
* \param[out] pu1_ecd_data  pointer coeff output buffer (input to ent cod)
* \param[out] pu1_csbf_buf  pointer to store the csbf for all 4x4 in a current
*                           block
* \param[out] csbf_strd     csbf buffer stride
* \param[in] trans_size     transform size (4, 8, 16)
* \param[in] intra_flag     0:Inter/Skip 1:Intra
* \param[out] pi4_coeff_off pointer to store the number of bytes produced in
*                           coeff buffer
the current TU in RDopt Mode
* \param[out] pi4_zero_col  pointer to store the zero_col info for the TU
* \param[out] pi4_zero_row  pointer to store the zero_row info for the TU
*
* \return
*    CBF of the current block
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_chroma_t_q_iq_ssd_scan_fxn(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD16 *pi2_deq_data,
    WORD32 deq_data_strd,
    UWORD8 *pu1_recon,
    WORD32 i4_recon_stride,
    UWORD8 *pu1_ecd_data,
    UWORD8 *pu1_csbf_buf,
    WORD32 csbf_strd,
    WORD32 trans_size,
    WORD32 i4_scan_idx,
    WORD32 intra_flag,
    WORD32 *pi4_coeff_off,
    WORD32 *pi4_tu_bits,
    WORD32 *pi4_zero_col,
    WORD32 *pi4_zero_row,
    UWORD8 *pu1_is_recon_available,
    WORD32 i4_perform_sbh,
    WORD32 i4_perform_rdoq,
    LWORD64 *pi8_cost,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_is_skip,
    SSD_TYPE_T e_ssd_type,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 trans_idx, cbf, u4_blk_sad;
    WORD16 *pi2_quant_coeffs;
    WORD16 *pi2_trans_values;
    WORD32 quant_scale_mat_offset;
    WORD32 *pi4_trans_scratch;
    WORD32 *pi4_subBlock2csbfId_map = NULL;

#if PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
    WORD32 ai4_quant_rounding_factors[3][MAX_TU_SIZE * MAX_TU_SIZE], i;
#endif

    rdoq_sbh_ctxt_t *ps_rdoq_sbh_ctxt = &ps_ctxt->s_rdoq_sbh_ctxt;

    WORD32 i4_perform_zcbf = (ps_ctxt->i4_zcbf_rdo_level == ZCBF_ENABLE) ||
                             (!intra_flag && ENABLE_INTER_ZCU_COST);
    WORD32 i4_perform_coeff_level_rdoq =
        (ps_ctxt->i4_quant_rounding_level != FIXED_QUANT_ROUNDING) &&
        (ps_ctxt->i4_chroma_quant_rounding_level == CHROMA_QUANT_ROUNDING);

    ASSERT((e_chroma_plane == U_PLANE) || (e_chroma_plane == V_PLANE));
    ASSERT(csbf_strd == MAX_TU_IN_CTB_ROW);

    *pi4_coeff_off = 0;
    *pi4_tu_bits = 0;
    pu1_is_recon_available[0] = 0;

    pi4_trans_scratch = (WORD32 *)&ps_ctxt->ai2_scratch[0];
    pi2_quant_coeffs = &ps_ctxt->ai2_scratch[0];
    pi2_trans_values = &ps_ctxt->ai2_scratch[0] + (MAX_TRANS_SIZE * 2);

    if(2 == trans_size)
    {
        trans_size = 4;
    }

    /* translate the transform size to index */
    trans_idx = trans_size >> 2;

    if(16 == trans_size)
    {
        trans_idx = 3;
    }

    if(u1_is_skip)
    {
        pi8_cost[0] = ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_ssd_calculator(
            pu1_pred,
            pu1_src,
            pred_strd,
            src_strd,
            trans_size,
            trans_size,
            e_chroma_plane);

        if(e_ssd_type == SPATIAL_DOMAIN_SSD)
        {
            /* buffer copy fromp pred to recon */
            ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                pu1_pred,
                pred_strd,
                pu1_recon,
                i4_recon_stride,
                trans_size,
                trans_size,
                e_chroma_plane);

            pu1_is_recon_available[0] = 1;
        }

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi8_cost[0],
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                e_chroma_plane);
        }
#endif

#if ENABLE_INTER_ZCU_COST
#if !WEIGH_CHROMA_COST
        /* cbf = 0, accumulate cu not coded cost */
        ps_ctxt->i8_cu_not_coded_cost += pi8_cost[0];
#else
        ps_ctxt->i8_cu_not_coded_cost += (pi8_cost[0] * ps_ctxt->u4_chroma_cost_weighing_factor +
                                          (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                                         CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT;
#endif
#endif

        return 0;
    }

    if(intra_flag == 1)
    {
        quant_scale_mat_offset = 0;

#if PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
        ai4_quant_rounding_factors[0][0] =
            MAX(ps_ctxt->i4_quant_rnd_factor[intra_flag], (1 << QUANT_ROUND_FACTOR_Q) / 3);

        for(i = 0; i < trans_size * trans_size; i++)
        {
            ai4_quant_rounding_factors[1][i] =
                MAX(ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[trans_size >> 3][i],
                    (1 << QUANT_ROUND_FACTOR_Q) / 3);
            ai4_quant_rounding_factors[2][i] =
                MAX(ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[trans_size >> 3][i],
                    (1 << QUANT_ROUND_FACTOR_Q) / 3);
        }
#endif
    }
    else
    {
        quant_scale_mat_offset = NUM_TRANS_TYPES;
    }

    switch(trans_size)
    {
    case 4:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map4x4TU;

        break;
    }
    case 8:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map8x8TU;

        break;
    }
    case 16:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map16x16TU;

        break;
    }
    case 32:
    {
        pi4_subBlock2csbfId_map = gai4_subBlock2csbfId_map32x32TU;

        break;
    }
    }

    /* ---------- call residue and transform block ------- */
    u4_blk_sad = ps_ctxt->apf_chrm_resd_trns[trans_idx - 1](
        pu1_src,
        pu1_pred,
        pi4_trans_scratch,
        pi2_trans_values,
        src_strd,
        pred_strd,
        trans_size,
        e_chroma_plane);
    (void)u4_blk_sad;
    /* -------- calculate SSD calculation in Transform Domain ------ */

    cbf = ps_ctxt->apf_quant_iquant_ssd
              [i4_perform_coeff_level_rdoq + (e_ssd_type != FREQUENCY_DOMAIN_SSD) * 2]

          (pi2_trans_values,
           ps_ctxt->api2_rescal_mat[trans_idx + quant_scale_mat_offset],
           pi2_quant_coeffs,
           pi2_deq_data,
           trans_size,
           ps_ctxt->i4_chrm_cu_qp_div6,
           ps_ctxt->i4_chrm_cu_qp_mod6,
#if !PROHIBIT_INTRA_QUANT_ROUNDING_FACTOR_TO_DROP_BELOW_1BY3
           ps_ctxt->i4_quant_rnd_factor[intra_flag],
           ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[trans_size >> 3],
           ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[trans_size >> 3],
#else
           intra_flag ? ai4_quant_rounding_factors[0][0] : ps_ctxt->i4_quant_rnd_factor[intra_flag],
           intra_flag ? ai4_quant_rounding_factors[1]
                      : ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[trans_size >> 3],
           intra_flag ? ai4_quant_rounding_factors[2]
                      : ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[trans_size >> 3],
#endif
           trans_size,
           trans_size,
           deq_data_strd,
           pu1_csbf_buf,
           csbf_strd,
           pi4_zero_col,
           pi4_zero_row,
           ps_ctxt->api2_scal_mat[trans_idx + quant_scale_mat_offset],
           pi8_cost);

    if(e_ssd_type != FREQUENCY_DOMAIN_SSD)
    {
        pi8_cost[0] = UINT_MAX;
    }

    if(0 != cbf)
    {
        if(i4_perform_sbh || i4_perform_rdoq)
        {
            ps_rdoq_sbh_ctxt->i4_iq_data_strd = deq_data_strd;
            ps_rdoq_sbh_ctxt->i4_q_data_strd = trans_size;

            ps_rdoq_sbh_ctxt->i4_qp_div = ps_ctxt->i4_chrm_cu_qp_div6;
            ps_rdoq_sbh_ctxt->i2_qp_rem = ps_ctxt->i4_chrm_cu_qp_mod6;
            ps_rdoq_sbh_ctxt->i4_scan_idx = i4_scan_idx;
            ps_rdoq_sbh_ctxt->i8_ssd_cost = *pi8_cost;
            ps_rdoq_sbh_ctxt->i4_trans_size = trans_size;

            ps_rdoq_sbh_ctxt->pi2_dequant_coeff =
                ps_ctxt->api2_scal_mat[trans_idx + quant_scale_mat_offset];
            ps_rdoq_sbh_ctxt->pi2_iquant_coeffs = pi2_deq_data;
            ps_rdoq_sbh_ctxt->pi2_quant_coeffs = pi2_quant_coeffs;
            ps_rdoq_sbh_ctxt->pi2_trans_values = pi2_trans_values;
            ps_rdoq_sbh_ctxt->pu1_csbf_buf = pu1_csbf_buf;
            ps_rdoq_sbh_ctxt->pi4_subBlock2csbfId_map = pi4_subBlock2csbfId_map;

            if((!i4_perform_rdoq))
            {
                ihevce_sign_data_hiding(ps_rdoq_sbh_ctxt);

                pi8_cost[0] = ps_rdoq_sbh_ctxt->i8_ssd_cost;
            }
        }

        /* ------- call coeffs scan function ------- */
        *pi4_coeff_off = ps_ctxt->s_cmn_opt_func.pf_scan_coeffs(
            pi2_quant_coeffs,
            pi4_subBlock2csbfId_map,
            i4_scan_idx,
            trans_size,
            pu1_ecd_data,
            pu1_csbf_buf,
            csbf_strd);
    }

    /*  Normalize Cost. Note : trans_idx, not (trans_idx-1) */
    pi8_cost[0] >>= ga_trans_shift[trans_idx];

#if RDOPT_ZERO_CBF_ENABLE
    if((0 != cbf))
    {
        WORD32 tu_bits;
        LWORD64 zero_cbf_cost_u, curr_cb_cod_cost;

        zero_cbf_cost_u = 0;

        /*Populating the feilds of rdoq_ctxt structure*/
        if(i4_perform_rdoq)
        {
            //memset(ps_rdoq_sbh_ctxt,0,sizeof(rdoq_sbh_ctxt_t));
            /* transform size to log2transform size */
            GETRANGE(ps_rdoq_sbh_ctxt->i4_log2_trans_size, trans_size);
            ps_rdoq_sbh_ctxt->i4_log2_trans_size -= 1;

            ps_rdoq_sbh_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->i8_cl_ssd_lambda_chroma_qf;
            ps_rdoq_sbh_ctxt->i4_is_luma = 0;
            ps_rdoq_sbh_ctxt->i4_shift_val_ssd_in_td = ga_trans_shift[trans_idx];
            ps_rdoq_sbh_ctxt->i4_round_val_ssd_in_td =
                (1 << (ps_rdoq_sbh_ctxt->i4_shift_val_ssd_in_td - 1));
            ps_rdoq_sbh_ctxt->i1_tu_is_coded = 0;
            ps_rdoq_sbh_ctxt->pi4_zero_col = pi4_zero_col;
            ps_rdoq_sbh_ctxt->pi4_zero_row = pi4_zero_row;
        }
        else if(i4_perform_zcbf)
        {
            /* cost of zero cbf encoding */
            zero_cbf_cost_u =

                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_ssd_calculator(
                    pu1_pred,
                    pu1_src,
                    pred_strd,
                    src_strd,
                    trans_size,
                    trans_size,
                    e_chroma_plane);
        }

        /************************************************************************/
        /* call the entropy rdo encode to get the bit estimate for current tu   */
        /* note that tu includes only residual coding bits and does not include */
        /* tu split, cbf and qp delta encoding bits for a TU                    */
        /************************************************************************/
        if(i4_perform_rdoq)
        {
            tu_bits = ihevce_entropy_rdo_encode_tu_rdoq(
                &ps_ctxt->s_rdopt_entropy_ctxt,
                pu1_ecd_data,
                trans_size,
                0,
                ps_rdoq_sbh_ctxt,
                pi8_cost,
                &zero_cbf_cost_u,
                0);
            //Currently, we are not accounting for sign bit in RDOPT bits calculation when RDOQ is turned on

            if(ps_rdoq_sbh_ctxt->i1_tu_is_coded == 0)
            {
                cbf = 0;

                /* num bytes is set to 0 */
                *pi4_coeff_off = 0;
            }

            (*pi4_tu_bits) += tu_bits;

            if((i4_perform_sbh) && (0 != cbf))
            {
                ps_rdoq_sbh_ctxt->i8_ssd_cost = pi8_cost[0];

                ihevce_sign_data_hiding(ps_rdoq_sbh_ctxt);

                pi8_cost[0] = ps_rdoq_sbh_ctxt->i8_ssd_cost;
            }

            /*Add round value before normalizing*/
            pi8_cost[0] += ps_rdoq_sbh_ctxt->i4_round_val_ssd_in_td;
            pi8_cost[0] >>= ga_trans_shift[trans_idx];

            if(ps_rdoq_sbh_ctxt->i1_tu_is_coded == 1)
            {
                *pi4_coeff_off = ps_ctxt->s_cmn_opt_func.pf_scan_coeffs(
                    pi2_quant_coeffs,
                    pi4_subBlock2csbfId_map,
                    i4_scan_idx,
                    trans_size,
                    pu1_ecd_data,
                    ps_rdoq_sbh_ctxt->pu1_csbf_buf,
                    csbf_strd);
            }
        }
        else
        {
            /************************************************************************/
            /* call the entropy rdo encode to get the bit estimate for current tu   */
            /* note that tu includes only residual coding bits and does not include */
            /* tu split, cbf and qp delta encoding bits for a TU                    */
            /************************************************************************/
            tu_bits = ihevce_entropy_rdo_encode_tu(
                &ps_ctxt->s_rdopt_entropy_ctxt, pu1_ecd_data, trans_size, 0, i4_perform_sbh);

            (*pi4_tu_bits) += tu_bits;
        }

        if(e_ssd_type == SPATIAL_DOMAIN_SSD)
        {
            pi8_cost[0] = ihevce_it_recon_ssd(
                ps_ctxt,
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi2_deq_data,
                deq_data_strd,
                pu1_recon,
                i4_recon_stride,
                pu1_ecd_data,
                trans_size,
                PRED_MODE_INTRA,
                cbf,
                pi4_zero_col[0],
                pi4_zero_row[0],
                e_chroma_plane);

            pu1_is_recon_available[0] = 1;
        }

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        if(u1_is_cu_noisy && (e_ssd_type == SPATIAL_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_recon,
                i4_recon_stride,
                pi8_cost[0],
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                e_chroma_plane);
        }
        else if(u1_is_cu_noisy && (e_ssd_type == FREQUENCY_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi8_cost[0],
                i4_alpha_stim_multiplier,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                e_chroma_plane);
        }
#endif

        curr_cb_cod_cost = pi8_cost[0];

        /* add the SSD cost to bits estimate given by ECD */
        curr_cb_cod_cost +=
            COMPUTE_RATE_COST_CLIP30(tu_bits, ps_ctxt->i8_cl_ssd_lambda_chroma_qf, LAMBDA_Q_SHIFT);

        if(i4_perform_zcbf)
        {
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            if(u1_is_cu_noisy && i4_alpha_stim_multiplier)
            {
                zero_cbf_cost_u = ihevce_inject_stim_into_distortion(
                    pu1_src,
                    src_strd,
                    pu1_pred,
                    pred_strd,
                    zero_cbf_cost_u,
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                                 100.0,
                    trans_size,
                    0,
                    ps_ctxt->u1_enable_psyRDOPT,
                    e_chroma_plane);
            }
#endif
            /* force the tu as zero cbf if zero_cbf_cost is lower */
            if(zero_cbf_cost_u < curr_cb_cod_cost)
            {
                *pi4_coeff_off = 0;
                cbf = 0;
                (*pi4_tu_bits) = 0;
                pi8_cost[0] = zero_cbf_cost_u;

                pu1_is_recon_available[0] = 0;

                if(e_ssd_type == SPATIAL_DOMAIN_SSD)
                {
                    ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy(
                        pu1_pred,
                        pred_strd,
                        pu1_recon,
                        i4_recon_stride,
                        trans_size,
                        trans_size,
                        e_chroma_plane);

                    pu1_is_recon_available[0] = 1;
                }
            }

#if ENABLE_INTER_ZCU_COST
            if(!intra_flag)
            {
#if !WEIGH_CHROMA_COST
                ps_ctxt->i8_cu_not_coded_cost += zero_cbf_cost_u;
#else
                ps_ctxt->i8_cu_not_coded_cost += (LWORD64)(
                    (zero_cbf_cost_u * ps_ctxt->u4_chroma_cost_weighing_factor +
                     (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                    CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif
            }
#endif
        }
    }
    else
    {
        if(e_ssd_type == SPATIAL_DOMAIN_SSD)
        {
            pi8_cost[0] = ihevce_it_recon_ssd(
                ps_ctxt,
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi2_deq_data,
                deq_data_strd,
                pu1_recon,
                i4_recon_stride,
                pu1_ecd_data,
                trans_size,
                PRED_MODE_INTRA,
                cbf,
                pi4_zero_col[0],
                pi4_zero_row[0],
                e_chroma_plane);

            pu1_is_recon_available[0] = 1;
        }

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
        if(u1_is_cu_noisy && (e_ssd_type == SPATIAL_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_recon,
                i4_recon_stride,
                pi8_cost[0],
                !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                       : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                          (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                             100.0,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                e_chroma_plane);
        }
        else if(u1_is_cu_noisy && (e_ssd_type == FREQUENCY_DOMAIN_SSD) && i4_alpha_stim_multiplier)
        {
            pi8_cost[0] = ihevce_inject_stim_into_distortion(
                pu1_src,
                src_strd,
                pu1_pred,
                pred_strd,
                pi8_cost[0],
                !ps_ctxt->u1_is_refPic ? ALPHA_FOR_ZERO_CODING_DECISIONS
                                       : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                          (double)ALPHA_FOR_ZERO_CODING_DECISIONS) /
                                             100.0,
                trans_size,
                0,
                ps_ctxt->u1_enable_psyRDOPT,
                e_chroma_plane);
        }
#endif

#if ENABLE_INTER_ZCU_COST
        if(!intra_flag)
        {
#if !WEIGH_CHROMA_COST
            /* cbf = 0, accumulate cu not coded cost */
            ps_ctxt->i8_cu_not_coded_cost += pi8_cost[0];
#else
            /* cbf = 0, accumulate cu not coded cost */

            ps_ctxt->i8_cu_not_coded_cost += (LWORD64)(
                (pi8_cost[0] * ps_ctxt->u4_chroma_cost_weighing_factor +
                 (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT);
#endif
        }
#endif
    }
#endif /* RDOPT_ZERO_CBF_ENABLE */

    return (cbf);
}
