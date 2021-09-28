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
* \file ihevce_enc_loop_pass.c
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

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern PART_ID_T ge_part_type_to_part_id[MAX_PART_TYPES][MAX_NUM_PARTS];

extern UWORD8 gau1_num_parts_in_part_type[MAX_PART_TYPES];

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define UPDATE_QP_AT_CTB 6
#define INTRAPRED_SIMD_LEFT_PADDING 16
#define INTRAPRED_SIMD_RIGHT_PADDING 8

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_ctb_left_copy \endif
*
* \brief
*    This function copy the right data of CTB to context buffers
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
void ihevce_enc_loop_ctb_left_copy(ihevce_enc_loop_ctxt_t *ps_ctxt, enc_loop_cu_prms_t *ps_cu_prms)
{
    /* ------------------------------------------------------------------ */
    /* copy the right coloum data to the context buffers                  */
    /* ------------------------------------------------------------------ */

    nbr_4x4_t *ps_left_nbr;
    nbr_4x4_t *ps_nbr;
    UWORD8 *pu1_buff;
    WORD32 num_pels;
    UWORD8 *pu1_luma_left, *pu1_chrm_left;

    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);

    pu1_luma_left = (UWORD8 *)ps_ctxt->pv_left_luma_data;
    pu1_chrm_left = (UWORD8 *)ps_ctxt->pv_left_chrm_data;
    ps_left_nbr = &ps_ctxt->as_left_col_nbr[0];

    /* copy right luma data */
    pu1_buff = ps_cu_prms->pu1_luma_recon + ps_cu_prms->i4_ctb_size - 1;

    for(num_pels = 0; num_pels < ps_cu_prms->i4_ctb_size; num_pels++)
    {
        WORD32 i4_indx = ps_cu_prms->i4_luma_recon_stride * num_pels;

        pu1_luma_left[num_pels] = pu1_buff[i4_indx];
    }

    /* copy right chroma data */
    pu1_buff = ps_cu_prms->pu1_chrm_recon + ps_cu_prms->i4_ctb_size - 2;

    for(num_pels = 0; num_pels < (ps_cu_prms->i4_ctb_size >> (0 == u1_is_422)); num_pels++)
    {
        WORD32 i4_indx = ps_cu_prms->i4_chrm_recon_stride * num_pels;

        *pu1_chrm_left++ = pu1_buff[i4_indx];
        *pu1_chrm_left++ = pu1_buff[i4_indx + 1];
    }

    /* store the nbr 4x4 data at ctb level */
    {
        WORD32 ctr;
        WORD32 nbr_strd;

        nbr_strd = ps_cu_prms->i4_ctb_size >> 2;

        /* copy right nbr data */
        ps_nbr = &ps_ctxt->as_ctb_nbr_arr[0];
        ps_nbr += ((ps_cu_prms->i4_ctb_size >> 2) - 1);

        for(ctr = 0; ctr < (ps_cu_prms->i4_ctb_size >> 2); ctr++)
        {
            WORD32 i4_indx = nbr_strd * ctr;

            ps_left_nbr[ctr] = ps_nbr[i4_indx];
        }
    }
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_mark_all_modes_to_evaluate \endif
*
* \brief
*   Mark all modes for inter/intra for evaluation. This function will be
*   called by ref instance
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_cu_analyse : pointer to cu analyse
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_mark_all_modes_to_evaluate(void *pv_ctxt, cu_analyse_t *ps_cu_analyse)
{
    UWORD8 ctr;
    WORD32 i4_part;

    (void)pv_ctxt;
    /* run a loop over all Inter cands */
    for(ctr = 0; ctr < MAX_INTER_CU_CANDIDATES; ctr++)
    {
        ps_cu_analyse->as_cu_inter_cand[ctr].b1_eval_mark = 1;
    }

    /* run a loop over all intra candidates */
    if(0 != ps_cu_analyse->u1_num_intra_rdopt_cands)
    {
        for(ctr = 0; ctr < MAX_INTRA_CU_CANDIDATES + 1; ctr++)
        {
            ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_eval_mark[ctr] = 1;
            ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_by_2_eval_mark[ctr] = 1;

            for(i4_part = 0; i4_part < NUM_PU_PARTS; i4_part++)
            {
                ps_cu_analyse->s_cu_intra_cand.au1_nxn_eval_mark[i4_part][ctr] = 1;
            }
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_cu_mode_decide \endif
*
* \brief
*    Coding Unit mode decide function. Performs RD opt and decides the best mode
*
* \param[in] ps_ctxt : pointer to enc_loop module
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
LWORD64 ihevce_cu_mode_decide(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    enc_loop_cu_prms_t *ps_cu_prms,
    cu_analyse_t *ps_cu_analyse,
    final_mode_state_t *ps_final_mode_state,
    UWORD8 *pu1_ecd_data,
    pu_col_mv_t *ps_col_pu,
    UWORD8 *pu1_col_pu_map,
    WORD32 col_start_pu_idx)
{
    enc_loop_chrm_cu_buf_prms_t s_chrm_cu_buf_prms;
    cu_nbr_prms_t s_cu_nbr_prms;
    inter_cu_mode_info_t s_inter_cu_mode_info;
    cu_inter_cand_t *ps_best_inter_cand = NULL;
    UWORD8 *pu1_cu_top;
    UWORD8 *pu1_cu_top_left;
    UWORD8 *pu1_cu_left;
    UWORD8 *pu1_final_recon = NULL;
    UWORD8 *pu1_curr_src = NULL;
    void *pv_curr_src = NULL;
    void *pv_cu_left = NULL;
    void *pv_cu_top = NULL;
    void *pv_cu_top_left = NULL;

    WORD32 cu_left_stride = 0;
    WORD32 ctr;
    WORD32 rd_opt_best_idx;
    LWORD64 rd_opt_least_cost;
    WORD32 rd_opt_curr_idx;
    WORD32 num_4x4_in_ctb;
    WORD32 nbr_4x4_left_strd = 0;

    nbr_4x4_t *ps_topleft_nbr_4x4;
    nbr_4x4_t *ps_left_nbr_4x4 = NULL;
    nbr_4x4_t *ps_top_nbr_4x4 = NULL;
    nbr_4x4_t *ps_curr_nbr_4x4;
    WORD32 enable_intra_eval_flag;
    WORD32 i4_best_cu_qp = ps_ctxt->ps_rc_quant_ctxt->i2_min_qp - 1;
    WORD32 curr_cu_pos_in_row;
    WORD32 cu_top_right_offset;
    WORD32 cu_top_right_dep_pos;
    WORD32 i4_ctb_x_off, i4_ctb_y_off;

    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);
    (void)ps_final_mode_state;
    /* default init */
    rd_opt_least_cost = MAX_COST_64;
    ps_ctxt->as_cu_prms[0].i8_best_rdopt_cost = MAX_COST_64;
    ps_ctxt->as_cu_prms[1].i8_best_rdopt_cost = MAX_COST_64;

    /* Zero cbf tool is enabled by default for all presets */
    ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;

    rd_opt_best_idx = 1;
    rd_opt_curr_idx = 0;
    enable_intra_eval_flag = 1;

    /* CU params in enc ctxt*/
    ps_ctxt->ps_enc_out_ctxt->b3_cu_pos_x = ps_cu_analyse->b3_cu_pos_x;
    ps_ctxt->ps_enc_out_ctxt->b3_cu_pos_y = ps_cu_analyse->b3_cu_pos_y;
    ps_ctxt->ps_enc_out_ctxt->u1_cu_size = ps_cu_analyse->u1_cu_size;

    num_4x4_in_ctb = (ps_cu_prms->i4_ctb_size >> 2);
    ps_curr_nbr_4x4 = &ps_ctxt->as_ctb_nbr_arr[0];
    ps_curr_nbr_4x4 += (ps_cu_analyse->b3_cu_pos_x << 1);
    ps_curr_nbr_4x4 += ((ps_cu_analyse->b3_cu_pos_y << 1) * num_4x4_in_ctb);

    /* CB and Cr are pixel interleaved */
    s_chrm_cu_buf_prms.i4_chrm_recon_stride = ps_cu_prms->i4_chrm_recon_stride;

    s_chrm_cu_buf_prms.i4_chrm_src_stride = ps_cu_prms->i4_chrm_src_stride;

    if(!ps_ctxt->u1_is_input_data_hbd)
    {
        /* --------------------------------------- */
        /* ----- Luma Pointers Derivation -------- */
        /* --------------------------------------- */

        /* based on CU position derive the pointers */
        pu1_final_recon = ps_cu_prms->pu1_luma_recon + (ps_cu_analyse->b3_cu_pos_x << 3);

        pu1_curr_src = ps_cu_prms->pu1_luma_src + (ps_cu_analyse->b3_cu_pos_x << 3);

        pu1_final_recon += ((ps_cu_analyse->b3_cu_pos_y << 3) * ps_cu_prms->i4_luma_recon_stride);

        pu1_curr_src += ((ps_cu_analyse->b3_cu_pos_y << 3) * ps_cu_prms->i4_luma_src_stride);

        pv_curr_src = pu1_curr_src;

        /* CU left */
        if(0 == ps_cu_analyse->b3_cu_pos_x)
        {
            /* CTB boundary */
            pu1_cu_left = (UWORD8 *)ps_ctxt->pv_left_luma_data;
            pu1_cu_left += (ps_cu_analyse->b3_cu_pos_y << 3);
            cu_left_stride = 1;

            ps_left_nbr_4x4 = &ps_ctxt->as_left_col_nbr[0];
            ps_left_nbr_4x4 += ps_cu_analyse->b3_cu_pos_y << 1;
            nbr_4x4_left_strd = 1;
        }
        else
        {
            /* inside CTB */
            pu1_cu_left = pu1_final_recon - 1;
            cu_left_stride = ps_cu_prms->i4_luma_recon_stride;

            ps_left_nbr_4x4 = ps_curr_nbr_4x4 - 1;
            nbr_4x4_left_strd = num_4x4_in_ctb;
        }

        pv_cu_left = pu1_cu_left;

        /* CU top */
        if(0 == ps_cu_analyse->b3_cu_pos_y)
        {
            /* CTB boundary */
            pu1_cu_top = (UWORD8 *)ps_ctxt->pv_top_row_luma;
            pu1_cu_top += ps_cu_prms->i4_ctb_pos * ps_cu_prms->i4_ctb_size;
            pu1_cu_top += (ps_cu_analyse->b3_cu_pos_x << 3);

            ps_top_nbr_4x4 = ps_ctxt->ps_top_row_nbr;
            ps_top_nbr_4x4 += (ps_cu_prms->i4_ctb_pos * (ps_cu_prms->i4_ctb_size >> 2));
            ps_top_nbr_4x4 += (ps_cu_analyse->b3_cu_pos_x << 1);
        }
        else
        {
            /* inside CTB */
            pu1_cu_top = pu1_final_recon - ps_cu_prms->i4_luma_recon_stride;

            ps_top_nbr_4x4 = ps_curr_nbr_4x4 - num_4x4_in_ctb;
        }

        pv_cu_top = pu1_cu_top;

        /* CU top left */
        if((0 == ps_cu_analyse->b3_cu_pos_x) && (0 != ps_cu_analyse->b3_cu_pos_y))
        {
            /* left ctb boundary but not first row */
            pu1_cu_top_left = pu1_cu_left - 1; /* stride is 1 */
            ps_topleft_nbr_4x4 = ps_left_nbr_4x4 - 1; /* stride is 1 */
        }
        else
        {
            /* rest all cases topleft is top -1 */
            pu1_cu_top_left = pu1_cu_top - 1;
            ps_topleft_nbr_4x4 = ps_top_nbr_4x4 - 1;
        }

        pv_cu_top_left = pu1_cu_top_left;

        /* Store the CU nbr information in the ctxt for final reconstruction fun. */
        s_cu_nbr_prms.nbr_4x4_left_strd = nbr_4x4_left_strd;
        s_cu_nbr_prms.ps_left_nbr_4x4 = ps_left_nbr_4x4;
        s_cu_nbr_prms.ps_topleft_nbr_4x4 = ps_topleft_nbr_4x4;
        s_cu_nbr_prms.ps_top_nbr_4x4 = ps_top_nbr_4x4;
        s_cu_nbr_prms.pu1_cu_left = pu1_cu_left;
        s_cu_nbr_prms.pu1_cu_top = pu1_cu_top;
        s_cu_nbr_prms.pu1_cu_top_left = pu1_cu_top_left;
        s_cu_nbr_prms.cu_left_stride = cu_left_stride;

        /* ------------------------------------------------------------ */
        /* -- Initialize the number of neigbour skip cu count for rdo --*/
        /* ------------------------------------------------------------ */
        {
            nbr_avail_flags_t s_nbr;
            WORD32 i4_num_nbr_skip_cus = 0;

            /* get the neighbour availability flags for current cu  */
            ihevce_get_nbr_intra(
                &s_nbr,
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ps_cu_analyse->b3_cu_pos_x << 1),
                (ps_cu_analyse->b3_cu_pos_y << 1),
                (ps_cu_analyse->u1_cu_size >> 2));
            if(s_nbr.u1_top_avail)
            {
                i4_num_nbr_skip_cus += ps_top_nbr_4x4->b1_skip_flag;
            }

            if(s_nbr.u1_left_avail)
            {
                i4_num_nbr_skip_cus += ps_left_nbr_4x4->b1_skip_flag;
            }
            ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[0].i4_num_nbr_skip_cus =
                i4_num_nbr_skip_cus;
            ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[1].i4_num_nbr_skip_cus =
                i4_num_nbr_skip_cus;
        }

        /* --------------------------------------- */
        /* --- Chroma Pointers Derivation -------- */
        /* --------------------------------------- */

        /* based on CU position derive the pointers */
        s_chrm_cu_buf_prms.pu1_final_recon =
            ps_cu_prms->pu1_chrm_recon + (ps_cu_analyse->b3_cu_pos_x << 3);

        s_chrm_cu_buf_prms.pu1_curr_src =
            ps_cu_prms->pu1_chrm_src + (ps_cu_analyse->b3_cu_pos_x << 3);

        s_chrm_cu_buf_prms.pu1_final_recon +=
            ((ps_cu_analyse->b3_cu_pos_y << (u1_is_422 + 2)) * ps_cu_prms->i4_chrm_recon_stride);

        s_chrm_cu_buf_prms.pu1_curr_src +=
            ((ps_cu_analyse->b3_cu_pos_y << (u1_is_422 + 2)) * ps_cu_prms->i4_chrm_src_stride);

        /* CU left */
        if(0 == ps_cu_analyse->b3_cu_pos_x)
        {
            /* CTB boundary */
            s_chrm_cu_buf_prms.pu1_cu_left = (UWORD8 *)ps_ctxt->pv_left_chrm_data;
            s_chrm_cu_buf_prms.pu1_cu_left += (ps_cu_analyse->b3_cu_pos_y << (u1_is_422 + 3));
            s_chrm_cu_buf_prms.i4_cu_left_stride = 2;
        }
        else
        {
            /* inside CTB */
            s_chrm_cu_buf_prms.pu1_cu_left = s_chrm_cu_buf_prms.pu1_final_recon - 2;
            s_chrm_cu_buf_prms.i4_cu_left_stride = ps_cu_prms->i4_chrm_recon_stride;
        }

        /* CU top */
        if(0 == ps_cu_analyse->b3_cu_pos_y)
        {
            /* CTB boundary */
            s_chrm_cu_buf_prms.pu1_cu_top = (UWORD8 *)ps_ctxt->pv_top_row_chroma;
            s_chrm_cu_buf_prms.pu1_cu_top += ps_cu_prms->i4_ctb_pos * ps_cu_prms->i4_ctb_size;
            s_chrm_cu_buf_prms.pu1_cu_top += (ps_cu_analyse->b3_cu_pos_x << 3);
        }
        else
        {
            /* inside CTB */
            s_chrm_cu_buf_prms.pu1_cu_top =
                s_chrm_cu_buf_prms.pu1_final_recon - ps_cu_prms->i4_chrm_recon_stride;
        }

        /* CU top left */
        if((0 == ps_cu_analyse->b3_cu_pos_x) && (0 != ps_cu_analyse->b3_cu_pos_y))
        {
            /* left ctb boundary but not first row */
            s_chrm_cu_buf_prms.pu1_cu_top_left =
                s_chrm_cu_buf_prms.pu1_cu_left - 2; /* stride is 1 (2 pixels) */
        }
        else
        {
            /* rest all cases topleft is top -2 */
            s_chrm_cu_buf_prms.pu1_cu_top_left = s_chrm_cu_buf_prms.pu1_cu_top - 2;
        }
    }

    /* Set Variables for Dep. Checking and Setting */
    i4_ctb_x_off = (ps_cu_prms->i4_ctb_pos << 6);

    i4_ctb_y_off = ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_y;
    ps_ctxt->i4_satd_buf_idx = rd_opt_curr_idx;

    /* Set the pred pointer count for ME/intra to 0 to start */
    ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count = 0;

    ASSERT(
        (ps_cu_analyse->u1_num_inter_cands > 0) || (ps_cu_analyse->u1_num_intra_rdopt_cands > 0));

    ASSERT(ps_cu_analyse->u1_num_inter_cands <= MAX_INTER_CU_CANDIDATES);
    s_inter_cu_mode_info.u1_num_inter_cands = 0;
    s_inter_cu_mode_info.u1_idx_of_worst_cost_in_cost_array = 0;
    s_inter_cu_mode_info.u1_idx_of_worst_cost_in_pred_buf_array = 0;

    ps_ctxt->s_cu_inter_merge_skip.u1_num_merge_cands = 0;
    ps_ctxt->s_cu_inter_merge_skip.u1_num_skip_cands = 0;
    ps_ctxt->s_mixed_mode_inter_cu.u1_num_mixed_mode_type0_cands = 0;
    ps_ctxt->s_mixed_mode_inter_cu.u1_num_mixed_mode_type1_cands = 0;
    ps_ctxt->s_pred_buf_data.i4_pred_stride = ps_cu_analyse->u1_cu_size;
    if(0 != ps_cu_analyse->u1_num_inter_cands)
    {
        ihevce_inter_cand_sifter_prms_t s_prms;

        UWORD8 u1_enable_top_row_sync;

        if(ps_ctxt->u1_disable_intra_eval)
        {
            u1_enable_top_row_sync = !DISABLE_TOP_SYNC;
        }
        else
        {
            u1_enable_top_row_sync = 1;
        }

        if((!ps_ctxt->u1_use_top_at_ctb_boundary) && u1_enable_top_row_sync)
        {
            /* Wait till top data is ready          */
            /* Currently checking till top right CU */
            curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

            if(i4_ctb_y_off == 0)
            {
                /* No wait for 1st row */
                cu_top_right_offset = -(MAX_CTB_SIZE);
                {
                    ihevce_tile_params_t *ps_col_tile_params =
                        ((ihevce_tile_params_t *)ps_ctxt->pv_tile_params_base +
                         ps_ctxt->i4_tile_col_idx);
                    /* No wait for 1st row */
                    cu_top_right_offset = -(ps_col_tile_params->i4_first_sample_x + (MAX_CTB_SIZE));
                }
                cu_top_right_dep_pos = 0;
            }
            else
            {
                cu_top_right_offset = (ps_cu_analyse->u1_cu_size) + 4;
                cu_top_right_dep_pos = (i4_ctb_y_off >> 6) - 1;
            }

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

        if(ps_ctxt->i1_cu_qp_delta_enable)
        {
            ihevce_update_cu_level_qp_lamda(ps_ctxt, ps_cu_analyse, 4, 0);
        }

        s_prms.i4_ctb_nbr_map_stride = ps_ctxt->i4_nbr_map_strd;
        s_prms.i4_max_num_inter_rdopt_cands = ps_ctxt->i4_max_num_inter_rdopt_cands;
        s_prms.i4_nbr_4x4_left_strd = nbr_4x4_left_strd;
        s_prms.i4_src_strd = ps_cu_prms->i4_luma_src_stride;
        s_prms.ps_cu_inter_merge_skip = &ps_ctxt->s_cu_inter_merge_skip;
        s_prms.aps_cu_nbr_buf[0] = &ps_ctxt->as_cu_nbr[ps_ctxt->i4_satd_buf_idx][0];
        s_prms.aps_cu_nbr_buf[1] = &ps_ctxt->as_cu_nbr[!ps_ctxt->i4_satd_buf_idx][0];
        s_prms.ps_left_nbr_4x4 = ps_left_nbr_4x4;
        s_prms.ps_mc_ctxt = &ps_ctxt->s_mc_ctxt;
        s_prms.ps_me_cands = ps_cu_analyse->as_cu_inter_cand;
        s_prms.ps_mixed_modes_datastore = &ps_ctxt->s_mixed_mode_inter_cu;
        s_prms.ps_mv_pred_ctxt = &ps_ctxt->s_mv_pred_ctxt;
        s_prms.ps_pred_buf_data = &ps_ctxt->s_pred_buf_data;
        s_prms.ps_topleft_nbr_4x4 = ps_topleft_nbr_4x4;
        s_prms.ps_top_nbr_4x4 = ps_top_nbr_4x4;
        s_prms.pu1_ctb_nbr_map = ps_ctxt->pu1_ctb_nbr_map;
        s_prms.pv_src = pv_curr_src;
        s_prms.u1_cu_pos_x = ps_cu_analyse->b3_cu_pos_x << 3;
        s_prms.u1_cu_pos_y = ps_cu_analyse->b3_cu_pos_y << 3;
        s_prms.u1_cu_size = ps_cu_analyse->u1_cu_size;
        s_prms.u1_max_merge_candidates = ps_ctxt->i4_max_merge_candidates;
        s_prms.u1_num_me_cands = ps_cu_analyse->u1_num_inter_cands;
        s_prms.u1_use_satd_for_merge_eval = ps_ctxt->i4_use_satd_for_merge_eval;
        s_prms.u1_quality_preset = ps_ctxt->i4_quality_preset;
        s_prms.i1_slice_type = ps_ctxt->i1_slice_type;
        s_prms.ps_cu_me_intra_pred_prms = &ps_ctxt->s_cu_me_intra_pred_prms;
        s_prms.u1_is_hbd = (ps_ctxt->u1_bit_depth > 8);
        s_prms.ps_inter_cu_mode_info = &s_inter_cu_mode_info;
        s_prms.pai4_mv_cost = ps_cu_analyse->ai4_mv_cost;
        s_prms.i4_lambda_qf = ps_ctxt->i4_sad_lamda;
        s_prms.u1_use_merge_cand_from_top_row =
            (u1_enable_top_row_sync || (s_prms.u1_cu_pos_y > 0));
        s_prms.u1_merge_idx_cabac_model =
            ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[IHEVC_CAB_MERGE_IDX_EXT];
#if REUSE_ME_COMPUTED_ERROR_FOR_INTER_CAND_SIFTING
        s_prms.pai4_me_err_metric = ps_cu_analyse->ai4_err_metric;
        s_prms.u1_reuse_me_sad = 1;
#else
        s_prms.u1_reuse_me_sad = 0;
#endif

        if(ps_ctxt->s_mv_pred_ctxt.ps_slice_hdr->i1_slice_type != PSLICE)
        {
            if(ps_ctxt->i4_temporal_layer == 1)
            {
                s_prms.i4_alpha_stim_multiplier = ALPHA_FOR_NOISE_TERM_IN_ME_BREF;
            }
            else
            {
                s_prms.i4_alpha_stim_multiplier = ALPHA_FOR_NOISE_TERM_IN_ME;
            }
        }
        else
        {
            s_prms.i4_alpha_stim_multiplier = ALPHA_FOR_NOISE_TERM_IN_ME_P;
        }
        s_prms.u1_is_cu_noisy = ps_cu_prms->u1_is_cu_noisy;

        if(s_prms.u1_is_cu_noisy)
        {
            s_prms.i4_lambda_qf =
                ((float)s_prms.i4_lambda_qf) * (100.0f - ME_LAMBDA_DISCOUNT_WHEN_NOISY) / 100.0f;
        }
        s_prms.pf_luma_inter_pred_pu = ihevce_luma_inter_pred_pu;

        s_prms.ps_cmn_utils_optimised_function_list = &ps_ctxt->s_cmn_opt_func;

        s_prms.pf_evalsad_pt_npu_mxn_8bit = (FT_SAD_EVALUATOR *)ps_ctxt->pv_evalsad_pt_npu_mxn_8bit;
        ihevce_inter_cand_sifter(&s_prms);
    }
    if(u1_is_422)
    {
        UWORD8 au1_buf_ids[NUM_CU_ME_INTRA_PRED_IDX - 1];
        UWORD8 u1_num_bufs_allocated;

        u1_num_bufs_allocated = ihevce_get_free_pred_buf_indices(
            au1_buf_ids, &ps_ctxt->s_pred_buf_data.u4_is_buf_in_use, NUM_CU_ME_INTRA_PRED_IDX - 1);

        ASSERT(u1_num_bufs_allocated == (NUM_CU_ME_INTRA_PRED_IDX - 1));

        for(ctr = ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count; ctr < u1_num_bufs_allocated;
            ctr++)
        {
            {
                ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[ctr] =
                    (UWORD8 *)ps_ctxt->s_pred_buf_data.apv_inter_pred_data[au1_buf_ids[ctr]];
            }

            ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[ctr] = ps_cu_analyse->u1_cu_size;

            ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count++;
        }

        {
            ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[ctr] =
                (UWORD8 *)ps_ctxt->pv_422_chroma_intra_pred_buf;
        }

        ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[ctr] = ps_cu_analyse->u1_cu_size;

        ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count++;
    }
    else
    {
        UWORD8 au1_buf_ids[NUM_CU_ME_INTRA_PRED_IDX];
        UWORD8 u1_num_bufs_allocated;

        u1_num_bufs_allocated = ihevce_get_free_pred_buf_indices(
            au1_buf_ids, &ps_ctxt->s_pred_buf_data.u4_is_buf_in_use, NUM_CU_ME_INTRA_PRED_IDX);

        ASSERT(u1_num_bufs_allocated == NUM_CU_ME_INTRA_PRED_IDX);

        for(ctr = ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count; ctr < u1_num_bufs_allocated;
            ctr++)
        {
            {
                ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[ctr] =
                    (UWORD8 *)ps_ctxt->s_pred_buf_data.apv_inter_pred_data[au1_buf_ids[ctr]];
            }

            ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[ctr] = ps_cu_analyse->u1_cu_size;

            ps_ctxt->s_cu_me_intra_pred_prms.i4_pointer_count++;
        }
    }

    ihevce_mark_all_modes_to_evaluate(ps_ctxt, ps_cu_analyse);

    ps_ctxt->as_cu_prms[0].s_recon_datastore.u1_is_lumaRecon_available = 0;
    ps_ctxt->as_cu_prms[1].s_recon_datastore.u1_is_lumaRecon_available = 0;
    ps_ctxt->as_cu_prms[0].s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
    ps_ctxt->as_cu_prms[1].s_recon_datastore.au1_is_chromaRecon_available[0] = 0;
    ps_ctxt->as_cu_prms[0].s_recon_datastore.au1_is_chromaRecon_available[1] = 0;
    ps_ctxt->as_cu_prms[1].s_recon_datastore.au1_is_chromaRecon_available[1] = 0;
    ps_ctxt->as_cu_prms[0].s_recon_datastore.au1_is_chromaRecon_available[2] = 0;
    ps_ctxt->as_cu_prms[1].s_recon_datastore.au1_is_chromaRecon_available[2] = 0;
    /* --------------------------------------- */
    /* ------ Inter RD OPT stage ------------- */
    /* --------------------------------------- */
    if(0 != s_inter_cu_mode_info.u1_num_inter_cands)
    {
        UWORD8 u1_ssd_bit_info_ctr = 0;

        /* -- run a loop over all Inter rd opt cands ------ */
        for(ctr = 0; ctr < s_inter_cu_mode_info.u1_num_inter_cands; ctr++)
        {
            cu_inter_cand_t *ps_inter_cand;

            LWORD64 rd_opt_cost = 0;

            ps_inter_cand = s_inter_cu_mode_info.aps_cu_data[ctr];

            if((ps_inter_cand->b1_skip_flag) || (ps_inter_cand->as_inter_pu[0].b1_merge_flag) ||
               (ps_inter_cand->b3_part_size && ps_inter_cand->as_inter_pu[1].b1_merge_flag))
            {
                ps_inter_cand->b1_eval_mark = 1;
            }

            /****************************************************************/
            /* This check is only valid for derived instances.              */
            /* check if this mode needs to be evaluated or not.             */
            /* if it is a skip candidate, go ahead and evaluate it even if  */
            /* it has not been marked while sorting.                        */
            /****************************************************************/
            if((0 == ps_inter_cand->b1_eval_mark) && (0 == ps_inter_cand->b1_skip_flag))
            {
                continue;
            }

            /* RDOPT related copies and settings */
            ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx = rd_opt_curr_idx;

            /* RDOPT copy States : Prev Cu best to current init */
            COPY_CABAC_STATES(
                &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                IHEVC_CAB_CTXT_END * sizeof(UWORD8));
            /* MVP ,MVD calc and Motion compensation */
            rd_opt_cost = ((pf_inter_rdopt_cu_mc_mvp)ps_ctxt->pv_inter_rdopt_cu_mc_mvp)(
                ps_ctxt,
                ps_inter_cand,
                ps_cu_analyse->u1_cu_size,
                ps_cu_analyse->b3_cu_pos_x,
                ps_cu_analyse->b3_cu_pos_y,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                ps_topleft_nbr_4x4,
                nbr_4x4_left_strd,
                rd_opt_curr_idx);

#if ENABLE_TU_TREE_DETERMINATION_IN_RDOPT
            if((ps_ctxt->u1_bit_depth == 8) && (!ps_inter_cand->b1_skip_flag))
            {
                ihevce_determine_tu_tree_distribution(
                    ps_inter_cand,
                    (me_func_selector_t *)ps_ctxt->pv_err_func_selector,
                    ps_ctxt->ai2_scratch,
                    (UWORD8 *)pv_curr_src,
                    ps_cu_prms->i4_luma_src_stride,
                    ps_ctxt->i4_satd_lamda,
                    LAMBDA_Q_SHIFT,
                    ps_cu_analyse->u1_cu_size,
                    ps_ctxt->u1_max_tr_depth);
            }
#endif
#if DISABLE_ZERO_ZBF_IN_INTER
            ps_ctxt->i4_zcbf_rdo_level = NO_ZCBF;
#else
            ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;
#endif
            /* Recon loop with different TUs based on partition type*/
            rd_opt_cost += ((pf_inter_rdopt_cu_ntu)ps_ctxt->pv_inter_rdopt_cu_ntu)(
                ps_ctxt,
                ps_cu_prms,
                pv_curr_src,
                ps_cu_analyse->u1_cu_size,
                ps_cu_analyse->b3_cu_pos_x,
                ps_cu_analyse->b3_cu_pos_y,
                rd_opt_curr_idx,
                &s_chrm_cu_buf_prms,
                ps_inter_cand,
                ps_cu_analyse,
                !ps_ctxt->u1_is_refPic ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                       : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                          (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                             100.0);

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
            if(!ps_ctxt->u1_enable_psyRDOPT && ps_cu_prms->u1_is_cu_noisy)
            {
                ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
                    ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
            }
#endif

            /* based on the rd opt cost choose the best and current index */
            if(rd_opt_cost < rd_opt_least_cost)
            {
                /* swap the best and current indx */
                rd_opt_best_idx = !rd_opt_best_idx;
                rd_opt_curr_idx = !rd_opt_curr_idx;

                ps_ctxt->as_cu_prms[rd_opt_best_idx].i8_best_rdopt_cost = rd_opt_cost;
                rd_opt_least_cost = rd_opt_cost;
                i4_best_cu_qp = ps_ctxt->i4_cu_qp;

                /* Store the best Inter cand. for final_recon function */
                ps_best_inter_cand = ps_inter_cand;
            }

            /* set the neighbour map to 0 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ps_cu_analyse->b3_cu_pos_x << 1),
                (ps_cu_analyse->b3_cu_pos_y << 1),
                (ps_cu_analyse->u1_cu_size >> 2),
                0);

        } /* end of loop for all the Inter RD OPT cand */
    }
    /* --------------------------------------- */
    /* ---- Conditional Eval of Intra -------- */
    /* --------------------------------------- */
    {
        enc_loop_cu_final_prms_t *ps_enc_loop_bestprms;
        ps_enc_loop_bestprms = &ps_ctxt->as_cu_prms[rd_opt_best_idx];

        /* check if inter candidates are valid */
        if(0 != ps_cu_analyse->u1_num_inter_cands)
        {
            /* if skip or no residual inter candidates has won then */
            /* evaluation of intra candidates is disabled           */
            if((1 == ps_enc_loop_bestprms->u1_skip_flag) ||
               (0 == ps_enc_loop_bestprms->u1_is_cu_coded))
            {
                enable_intra_eval_flag = 0;
            }
        }
        /* Disable Intra Gating for HIGH QUALITY PRESET */
#if !ENABLE_INTRA_GATING_FOR_HQ
        if(IHEVCE_QUALITY_P3 > ps_ctxt->i4_quality_preset)
        {
            enable_intra_eval_flag = 1;

#if DISABLE_LARGE_INTRA_PQ
            if((IHEVCE_QUALITY_P0 == ps_ctxt->i4_quality_preset) && (ps_cu_prms->u1_is_cu_noisy) &&
               (ps_ctxt->i1_slice_type != ISLICE) && (0 != s_inter_cu_mode_info.u1_num_inter_cands))
            {
                if(ps_cu_analyse->u1_cu_size > 16)
                {
                    /* Disable 32x32 / 64x64 Intra in PQ P and B pics */
                    enable_intra_eval_flag = 0;
                }
                else if(ps_cu_analyse->u1_cu_size == 16)
                {
                    /* Disable tu equal to cu mode in 16x16 Intra in PQ P and B pics */
                    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0] = 255;
                }
            }
#endif
        }
#endif
    }

    /* --------------------------------------- */
    /* ------ Intra RD OPT stage ------------- */
    /* --------------------------------------- */

    /* -- run a loop over all Intra rd opt cands ------ */
    if((0 != ps_cu_analyse->u1_num_intra_rdopt_cands) && (1 == enable_intra_eval_flag))
    {
        LWORD64 rd_opt_cost;
        WORD32 end_flag = 0;
        WORD32 cu_eval_done = 0;
        WORD32 subcu_eval_done = 0;
        WORD32 subpu_eval_done = 0;
        WORD32 max_trans_size;
        WORD32 sync_wait_stride;
        max_trans_size = MIN(MAX_TU_SIZE, (ps_cu_analyse->u1_cu_size));
        sync_wait_stride = (ps_cu_analyse->u1_cu_size) + max_trans_size;

        if(!ps_ctxt->u1_use_top_at_ctb_boundary)
        {
            /* Wait till top data is ready          */
            /* Currently checking till top right CU */
            curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

            if(i4_ctb_y_off == 0)
            {
                /* No wait for 1st row */
                cu_top_right_offset = -(MAX_CTB_SIZE);
                {
                    ihevce_tile_params_t *ps_col_tile_params =
                        ((ihevce_tile_params_t *)ps_ctxt->pv_tile_params_base +
                         ps_ctxt->i4_tile_col_idx);
                    /* No wait for 1st row */
                    cu_top_right_offset = -(ps_col_tile_params->i4_first_sample_x + (MAX_CTB_SIZE));
                }
                cu_top_right_dep_pos = 0;
            }
            else
            {
                cu_top_right_offset = sync_wait_stride;
                cu_top_right_dep_pos = (i4_ctb_y_off >> 6) - 1;
            }

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
        ctr = 0;

        /* Zero cbf tool is disabled for intra CUs */
#if ENABLE_ZERO_CBF_IN_INTRA
        ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;
#else
        ps_ctxt->i4_zcbf_rdo_level = NO_ZCBF;
#endif

        /* Intra Mode gating based on MPM cand list and encoder quality preset */
        if((ps_ctxt->i1_slice_type != ISLICE) && (ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P3))
        {
            ihevce_mpm_idx_based_filter_RDOPT_cand(
                ps_ctxt,
                ps_cu_analyse,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                &ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[0],
                &ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_eval_mark[0]);

            ihevce_mpm_idx_based_filter_RDOPT_cand(
                ps_ctxt,
                ps_cu_analyse,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                &ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0],
                &ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_by_2_eval_mark[0]);
        }

        /* Call Chroma SATD function for curr_func_mode in HIGH QUALITY mode */
        if(1 == ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd)
        {
            /* For cu_size = 64, there won't be any TU_EQ_CU case */
            if(64 != ps_cu_analyse->u1_cu_size)
            {
                /* RDOPT copy States : Prev Cu best to current init */
                COPY_CABAC_STATES(
                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    IHEVC_CAB_CTXT_END);

                /* RDOPT related copies and settings */
                ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx = rd_opt_curr_idx;

                /* Calc. best SATD mode for TU_EQ_CU case */
                ((pf_intra_chroma_pred_mode_selector)ps_ctxt->pv_intra_chroma_pred_mode_selector)(
                    ps_ctxt,
                    &s_chrm_cu_buf_prms,
                    ps_cu_analyse,
                    rd_opt_curr_idx,
                    TU_EQ_CU,
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                                 100.0,
                    ps_cu_prms->u1_is_cu_noisy && !DISABLE_INTRA_WHEN_NOISY);

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
                if(!ps_ctxt->u1_enable_psyRDOPT && ps_cu_prms->u1_is_cu_noisy)
                {
                    ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
                    ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
                        ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
                }
#endif
            }

            /* For cu_size=8 case, chroma cost will be same for TU_EQ_CU and
            TU_EQ_CU_DIV2 case */

            if((ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] !=
                255) &&
               (8 != ps_cu_analyse->u1_cu_size))
            {
                /* RDOPT copy States : Prev Cu best to current init */
                COPY_CABAC_STATES(
                    &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                    &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                    IHEVC_CAB_CTXT_END);

                /* RDOPT related copies and settings */
                ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx = rd_opt_curr_idx;

                /* Calc. best SATD mode for TU_EQ_CU_DIV2 case */
                ((pf_intra_chroma_pred_mode_selector)ps_ctxt->pv_intra_chroma_pred_mode_selector)(
                    ps_ctxt,
                    &s_chrm_cu_buf_prms,
                    ps_cu_analyse,
                    rd_opt_curr_idx,
                    TU_EQ_CU_DIV2,
                    !ps_ctxt->u1_is_refPic ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                           : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                              (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                                 100.0,
                    ps_cu_prms->u1_is_cu_noisy && !DISABLE_INTRA_WHEN_NOISY);

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
                if(!ps_ctxt->u1_enable_psyRDOPT && ps_cu_prms->u1_is_cu_noisy)
                {
                    ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
                    ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
                        ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
                }
#endif
            }
        }

        while(0 == end_flag)
        {
            UWORD8 *pu1_mode = NULL;
            WORD32 curr_func_mode = 0;
            void *pv_pred;

            ASSERT(ctr < 36);

            /* TU equal to CU size evaluation of different modes */
            if(0 == cu_eval_done)
            {
                /* check if the all the modes have been evaluated */
                if(255 == ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[ctr])
                {
                    cu_eval_done = 1;
                    ctr = 0;
                }
                else if(
                    (1 == ctr) &&
                    ((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P5) ||
                     (ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6)) &&
                    (ps_ctxt->i1_slice_type != ISLICE))
                {
                    ctr = 0;
                    cu_eval_done = 1;
                    subcu_eval_done = 1;
                    subpu_eval_done = 1;
                }
                else
                {
                    if(0 == ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_eval_mark[ctr])
                    {
                        ctr++;
                        continue;
                    }

                    pu1_mode =
                        &ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu[ctr];
                    ctr++;
                    curr_func_mode = TU_EQ_CU;
                }
            }
            /* Sub CU (NXN) mode evaluation of different pred modes */
            if((0 == subpu_eval_done) && (1 == cu_eval_done))
            {
                /*For NxN modes evaluation all candidates for all PU parts are evaluated */
                /*inside the ihevce_intra_rdopt_cu_ntu function, so the subpu_eval_done is set to 1 */
                {
                    pu1_mode = &ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_nxn[0][ctr];

                    curr_func_mode = TU_EQ_SUBCU;
                    /* check if the any modes have to be evaluated */
                    if(255 == *pu1_mode)
                    {
                        subpu_eval_done = 1;
                        ctr = 0;
                    }
                    else if(ctr != 0) /* If the modes have to be evaluated, then terminate, as all modes are already evaluated */
                    {
                        subpu_eval_done = 1;
                        ctr = 0;
                    }
                    else
                    {
                        ctr++;
                    }
                }
            }

            /* TU size equal to CU div2 mode evaluation of different pred modes */
            if((0 == subcu_eval_done) && (1 == subpu_eval_done) && (1 == cu_eval_done))
            {
                /* check if the all the modes have been evaluated */
                if(255 ==
                   ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[ctr])
                {
                    subcu_eval_done = 1;
                }
                else if(
                    (1 == ctr) &&
                    ((ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P5) ||
                     (ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P6)) &&
                    (ps_ctxt->i1_slice_type != ISLICE) && (ps_cu_analyse->u1_cu_size == 64))
                {
                    subcu_eval_done = 1;
                }
                else
                {
                    if(0 == ps_cu_analyse->s_cu_intra_cand.au1_2nx2n_tu_eq_cu_by_2_eval_mark[ctr])
                    {
                        ctr++;
                        continue;
                    }

                    pu1_mode = &ps_cu_analyse->s_cu_intra_cand
                                    .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[ctr];

                    ctr++;
                    curr_func_mode = TU_EQ_CU_DIV2;
                }
            }

            /* check if all CU option have been evalueted */
            if((1 == cu_eval_done) && (1 == subcu_eval_done) && (1 == subpu_eval_done))
            {
                break;
            }

            /* RDOPT related copies and settings */
            ps_ctxt->s_rdopt_entropy_ctxt.i4_curr_buf_idx = rd_opt_curr_idx;

            /* Assign ME/Intra pred buf. to the current intra cand. since we
            are storing pred data for final_reon function */
            {
                pv_pred = ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[rd_opt_curr_idx];
            }

            /* RDOPT copy States : Prev Cu best to current init */
            COPY_CABAC_STATES(
                &ps_ctxt->au1_rdopt_init_ctxt_models[0],
                &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                IHEVC_CAB_CTXT_END);

            /* call the function which performs the normative Intra encode */
            rd_opt_cost = ((pf_intra_rdopt_cu_ntu)ps_ctxt->pv_intra_rdopt_cu_ntu)(
                ps_ctxt,
                ps_cu_prms,
                pv_pred,
                ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[rd_opt_curr_idx],
                &s_chrm_cu_buf_prms,
                pu1_mode,
                ps_cu_analyse,
                pv_curr_src,
                pv_cu_left,
                pv_cu_top,
                pv_cu_top_left,
                ps_left_nbr_4x4,
                ps_top_nbr_4x4,
                nbr_4x4_left_strd,
                cu_left_stride,
                rd_opt_curr_idx,
                curr_func_mode,
                !ps_ctxt->u1_is_refPic ? ALPHA_FOR_NOISE_TERM_IN_RDOPT
                                       : ((100 - ALPHA_DISCOUNT_IN_REF_PICS_IN_RDOPT) *
                                          (double)ALPHA_FOR_NOISE_TERM_IN_RDOPT) /
                                             100.0);

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
            if(!ps_ctxt->u1_enable_psyRDOPT && ps_cu_prms->u1_is_cu_noisy)
            {
                ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
                    ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
            }
#endif

            /* based on the rd opt cost choose the best and current index */
            if(rd_opt_cost < rd_opt_least_cost)
            {
                /* swap the best and current indx */
                rd_opt_best_idx = !rd_opt_best_idx;
                rd_opt_curr_idx = !rd_opt_curr_idx;
                i4_best_cu_qp = ps_ctxt->i4_cu_qp;

                rd_opt_least_cost = rd_opt_cost;
                ps_ctxt->as_cu_prms[rd_opt_best_idx].i8_best_rdopt_cost = rd_opt_cost;
            }

            if((TU_EQ_SUBCU == curr_func_mode) &&
               (ps_ctxt->as_cu_prms[rd_opt_best_idx].u1_intra_flag) &&
               (ps_ctxt->i4_quality_preset <= IHEVCE_QUALITY_P2) && !FORCE_INTRA_TU_DEPTH_TO_0)
            {
                UWORD8 au1_tu_eq_cu_div2_modes[4];
                UWORD8 au1_freq_of_mode[4];

                if(ps_ctxt->as_cu_prms[rd_opt_best_idx].u1_part_mode == SIZE_2Nx2N)
                {
                    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] =
                        255;  //ps_ctxt->as_cu_prms[rd_opt_best_idx].au1_intra_pred_mode[0];
                    ps_cu_analyse->s_cu_intra_cand.au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[1] =
                        255;
                }
                else
                {
                    WORD32 i4_num_clusters = ihevce_find_num_clusters_of_identical_points_1D(
                        ps_ctxt->as_cu_prms[rd_opt_best_idx].au1_intra_pred_mode,
                        au1_tu_eq_cu_div2_modes,
                        au1_freq_of_mode,
                        4);

                    if(2 == i4_num_clusters)
                    {
                        if(au1_freq_of_mode[0] == 3)
                        {
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] =
                                au1_tu_eq_cu_div2_modes[0];
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[1] = 255;
                        }
                        else if(au1_freq_of_mode[1] == 3)
                        {
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] =
                                au1_tu_eq_cu_div2_modes[1];
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[1] = 255;
                        }
                        else
                        {
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[0] =
                                au1_tu_eq_cu_div2_modes[0];
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[1] =
                                au1_tu_eq_cu_div2_modes[1];
                            ps_cu_analyse->s_cu_intra_cand
                                .au1_intra_luma_modes_2nx2n_tu_eq_cu_by_2[2] = 255;
                        }
                    }
                }
            }

            /* set the neighbour map to 0 */
            ihevce_set_nbr_map(
                ps_ctxt->pu1_ctb_nbr_map,
                ps_ctxt->i4_nbr_map_strd,
                (ps_cu_analyse->b3_cu_pos_x << 1),
                (ps_cu_analyse->b3_cu_pos_y << 1),
                (ps_cu_analyse->u1_cu_size >> 2),
                0);
        }

    } /* end of Intra RD OPT cand evaluation */

    ASSERT(i4_best_cu_qp > (ps_ctxt->ps_rc_quant_ctxt->i2_min_qp - 1));
    ps_ctxt->i4_cu_qp = i4_best_cu_qp;
    ps_cu_analyse->i1_cu_qp = i4_best_cu_qp;

    /* --------------------------------------- */
    /* --------Final mode Recon ---------- */
    /* --------------------------------------- */
    {
        enc_loop_cu_final_prms_t *ps_enc_loop_bestprms;
        void *pv_final_pred = NULL;
        WORD32 final_pred_strd = 0;
        void *pv_final_pred_chrm = NULL;
        WORD32 final_pred_strd_chrm = 0;
        WORD32 packed_pred_mode;

#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
            pu1_ecd_data = &ps_ctxt->pu1_cu_recur_coeffs[0];
        }
#else
        pu1_ecd_data = &ps_ctxt->pu1_cu_recur_coeffs[0];
#endif

        ps_enc_loop_bestprms = &ps_ctxt->as_cu_prms[rd_opt_best_idx];
        packed_pred_mode =
            ps_enc_loop_bestprms->u1_intra_flag + (ps_enc_loop_bestprms->u1_skip_flag) * 2;

        if(!ps_ctxt->u1_is_input_data_hbd)
        {
            if(ps_enc_loop_bestprms->u1_intra_flag)
            {
                pv_final_pred = ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[rd_opt_best_idx];
                final_pred_strd =
                    ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[rd_opt_best_idx];
            }
            else
            {
                pv_final_pred = ps_best_inter_cand->pu1_pred_data;
                final_pred_strd = ps_best_inter_cand->i4_pred_data_stride;
            }

            pv_final_pred_chrm =
                ps_ctxt->s_cu_me_intra_pred_prms.pu1_pred_data[CU_ME_INTRA_PRED_CHROMA_IDX] +
                rd_opt_best_idx * ((MAX_CTB_SIZE * MAX_CTB_SIZE >> 1) +
                                   (u1_is_422 * (MAX_CTB_SIZE * MAX_CTB_SIZE >> 1)));
            final_pred_strd_chrm =
                ps_ctxt->s_cu_me_intra_pred_prms.ai4_pred_data_stride[CU_ME_INTRA_PRED_CHROMA_IDX];
        }

        ihevce_set_eval_flags(ps_ctxt, ps_enc_loop_bestprms);

        {
            final_mode_process_prms_t s_prms;

            void *pv_cu_luma_recon;
            void *pv_cu_chroma_recon;
            WORD32 luma_stride, chroma_stride;

            if(!ps_ctxt->u1_is_input_data_hbd)
            {
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
                if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
                {
                    pv_cu_luma_recon = ps_ctxt->pv_cu_luma_recon;
                    pv_cu_chroma_recon = ps_ctxt->pv_cu_chrma_recon;
                    luma_stride = ps_cu_analyse->u1_cu_size;
                    chroma_stride = ps_cu_analyse->u1_cu_size;
                }
                else
                {
                    /* based on CU position derive the luma pointers */
                    pv_cu_luma_recon = pu1_final_recon;

                    /* based on CU position derive the chroma pointers */
                    pv_cu_chroma_recon = s_chrm_cu_buf_prms.pu1_final_recon;

                    luma_stride = ps_cu_prms->i4_luma_recon_stride;

                    chroma_stride = ps_cu_prms->i4_chrm_recon_stride;
                }
#else
                pv_cu_luma_recon = ps_ctxt->pv_cu_luma_recon;
                pv_cu_chroma_recon = ps_ctxt->pv_cu_chrma_recon;
                luma_stride = ps_cu_analyse->u1_cu_size;
                chroma_stride = ps_cu_analyse->u1_cu_size;
#endif

                s_prms.ps_cu_nbr_prms = &s_cu_nbr_prms;
                s_prms.ps_best_inter_cand = ps_best_inter_cand;
                s_prms.ps_chrm_cu_buf_prms = &s_chrm_cu_buf_prms;
                s_prms.packed_pred_mode = packed_pred_mode;
                s_prms.rd_opt_best_idx = rd_opt_best_idx;
                s_prms.pv_src = pu1_curr_src;
                s_prms.src_strd = ps_cu_prms->i4_luma_src_stride;
                s_prms.pv_pred = pv_final_pred;
                s_prms.pred_strd = final_pred_strd;
                s_prms.pv_pred_chrm = pv_final_pred_chrm;
                s_prms.pred_chrm_strd = final_pred_strd_chrm;
                s_prms.pu1_final_ecd_data = pu1_ecd_data;
                s_prms.pu1_csbf_buf = &ps_ctxt->au1_cu_csbf[0];
                s_prms.csbf_strd = ps_ctxt->i4_cu_csbf_strd;
                s_prms.pv_luma_recon = pv_cu_luma_recon;
                s_prms.recon_luma_strd = luma_stride;
                s_prms.pv_chrm_recon = pv_cu_chroma_recon;
                s_prms.recon_chrma_strd = chroma_stride;
                s_prms.u1_cu_pos_x = ps_cu_analyse->b3_cu_pos_x;
                s_prms.u1_cu_pos_y = ps_cu_analyse->b3_cu_pos_y;
                s_prms.u1_cu_size = ps_cu_analyse->u1_cu_size;
                s_prms.i1_cu_qp = ps_cu_analyse->i1_cu_qp;
                s_prms.u1_will_cabac_state_change = 1;
                s_prms.u1_recompute_sbh_and_rdoq = 0;
                s_prms.u1_is_first_pass = 1;
            }

#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            s_prms.u1_is_cu_noisy = !ps_enc_loop_bestprms->u1_intra_flag
                                        ? ps_cu_prms->u1_is_cu_noisy
                                        : ps_cu_prms->u1_is_cu_noisy && !DISABLE_INTRA_WHEN_NOISY;
#endif

            ((pf_final_rdopt_mode_prcs)ps_ctxt->pv_final_rdopt_mode_prcs)(ps_ctxt, &s_prms);

#if USE_NOISE_TERM_IN_ENC_LOOP && RDOPT_LAMBDA_DISCOUNT_WHEN_NOISY
            if(!ps_ctxt->u1_enable_psyRDOPT && ps_cu_prms->u1_is_cu_noisy)
            {
                ps_ctxt->i8_cl_ssd_lambda_qf = ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf;
                ps_ctxt->i8_cl_ssd_lambda_chroma_qf =
                    ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf;
            }
#endif
        }
    }

    /* --------------------------------------- */
    /* --------Populate CU out prms ---------- */
    /* --------------------------------------- */
    {
        enc_loop_cu_final_prms_t *ps_enc_loop_bestprms;
        UWORD8 *pu1_pu_map;
        ps_enc_loop_bestprms = &ps_ctxt->as_cu_prms[rd_opt_best_idx];

        /* Corner case : If Part is 2Nx2N and Merge has all TU with zero cbf */
        /* then it has to be coded as skip CU */
        if((SIZE_2Nx2N == ps_enc_loop_bestprms->u1_part_mode) &&
           (1 == ps_enc_loop_bestprms->as_pu_enc_loop[0].b1_merge_flag) &&
           (0 == ps_enc_loop_bestprms->u1_skip_flag) && (0 == ps_enc_loop_bestprms->u1_is_cu_coded))
        {
            ps_enc_loop_bestprms->u1_skip_flag = 1;
        }

        /* update number PUs in CU */
        ps_cu_prms->i4_num_pus_in_cu = ps_enc_loop_bestprms->u2_num_pus_in_cu;

        /* ---- populate the colocated pu map index --- */
        for(ctr = 0; ctr < ps_enc_loop_bestprms->u2_num_pus_in_cu; ctr++)
        {
            WORD32 i;
            WORD32 vert_ht;
            WORD32 horz_wd;

            if(ps_enc_loop_bestprms->u1_intra_flag)
            {
                ps_enc_loop_bestprms->as_col_pu_enc_loop[ctr].b1_intra_flag = 1;
                vert_ht = ps_cu_analyse->u1_cu_size >> 2;
                horz_wd = ps_cu_analyse->u1_cu_size >> 2;
            }
            else
            {
                vert_ht = (((ps_enc_loop_bestprms->as_pu_enc_loop[ctr].b4_ht + 1) << 2) >> 2);
                horz_wd = (((ps_enc_loop_bestprms->as_pu_enc_loop[ctr].b4_wd + 1) << 2) >> 2);
            }

            pu1_pu_map = pu1_col_pu_map + ps_enc_loop_bestprms->as_pu_enc_loop[ctr].b4_pos_x;
            pu1_pu_map += (ps_enc_loop_bestprms->as_pu_enc_loop[ctr].b4_pos_y * num_4x4_in_ctb);

            for(i = 0; i < vert_ht; i++)
            {
                memset(pu1_pu_map, col_start_pu_idx, horz_wd);
                pu1_pu_map += num_4x4_in_ctb;
            }
            /* increment the index */
            col_start_pu_idx++;
        }
        /* ---- copy the colocated PUs to frm pu ----- */
        memcpy(
            ps_col_pu,
            &ps_enc_loop_bestprms->as_col_pu_enc_loop[0],
            ps_enc_loop_bestprms->u2_num_pus_in_cu * sizeof(pu_col_mv_t));

        /*---populate qp for 4x4 nbr array based on skip and cbf zero flag---*/
        {
            entropy_context_t *ps_entropy_ctxt;

            WORD32 diff_cu_qp_delta_depth, log2_ctb_size;

            WORD32 log2_min_cu_qp_delta_size;
            UWORD32 block_addr_align;
            ps_entropy_ctxt = ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt;

            log2_ctb_size = ps_entropy_ctxt->i1_log2_ctb_size;
            diff_cu_qp_delta_depth = ps_entropy_ctxt->ps_pps->i1_diff_cu_qp_delta_depth;

            log2_min_cu_qp_delta_size = log2_ctb_size - diff_cu_qp_delta_depth;
            block_addr_align = 15 << (log2_min_cu_qp_delta_size - 3);

            ps_entropy_ctxt->i4_qg_pos_x = ps_cu_analyse->b3_cu_pos_x & block_addr_align;
            ps_entropy_ctxt->i4_qg_pos_y = ps_cu_analyse->b3_cu_pos_y & block_addr_align;
            /*Update the Qp value used. It will not have a valid value iff
            current CU is (skipped/no_cbf). In that case the Qp needed for
            deblocking is calculated from top/left/previous coded CU*/

            ps_ctxt->ps_enc_out_ctxt->i1_cu_qp = ps_cu_analyse->i1_cu_qp;

            if(ps_entropy_ctxt->i4_qg_pos_x == ps_cu_analyse->b3_cu_pos_x &&
               ps_entropy_ctxt->i4_qg_pos_y == ps_cu_analyse->b3_cu_pos_y)
            {
                ps_ctxt->ps_enc_out_ctxt->b1_first_cu_in_qg = 1;
            }
            else
            {
                ps_ctxt->ps_enc_out_ctxt->b1_first_cu_in_qg = 0;
            }
        }

        /* -- at the end of CU set the neighbour map to 1 -- */
        ihevce_set_nbr_map(
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            (ps_cu_analyse->b3_cu_pos_x << 1),
            (ps_cu_analyse->b3_cu_pos_y << 1),
            (ps_cu_analyse->u1_cu_size >> 2),
            1);

        /* -- at the end of CU update best cabac rdopt states -- */
        /* -- and also set the top row skip flags  ------------- */
        ihevce_entropy_update_best_cu_states(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_cu_analyse->b3_cu_pos_x,
            ps_cu_analyse->b3_cu_pos_y,
            ps_cu_analyse->u1_cu_size,
            0,
            rd_opt_best_idx);
    }

    /* Store Output struct */
#if PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    {
        {
            memcpy(
                &ps_ctxt->ps_enc_out_ctxt->s_cu_prms,
                &ps_ctxt->as_cu_prms[rd_opt_best_idx],
                sizeof(enc_loop_cu_final_prms_t));
        }

        memcpy(
            &ps_ctxt->as_cu_recur_nbr[0],
            &ps_ctxt->as_cu_nbr[rd_opt_best_idx][0],
            sizeof(nbr_4x4_t) * (ps_cu_analyse->u1_cu_size >> 2) *
                (ps_cu_analyse->u1_cu_size >> 2));

        ps_ctxt->ps_enc_out_ctxt->ps_cu_prms = &ps_ctxt->ps_enc_out_ctxt->s_cu_prms;

        ps_ctxt->ps_cu_recur_nbr = &ps_ctxt->as_cu_recur_nbr[0];
    }
#else
    if(ps_ctxt->i4_quality_preset >= IHEVCE_QUALITY_P2)
    {
        ps_ctxt->ps_enc_out_ctxt->ps_cu_prms = &ps_ctxt->as_cu_prms[rd_opt_best_idx];

        ps_ctxt->ps_cu_recur_nbr = &ps_ctxt->as_cu_nbr[rd_opt_best_idx][0];

        if(ps_ctxt->u1_disable_intra_eval && ps_ctxt->i4_deblk_pad_hpel_cur_pic)
        {
            /* Wait till top data is ready          */
            /* Currently checking till top right CU */
            curr_cu_pos_in_row = i4_ctb_x_off + (ps_cu_analyse->b3_cu_pos_x << 3);

            if(i4_ctb_y_off == 0)
            {
                /* No wait for 1st row */
                cu_top_right_offset = -(MAX_CTB_SIZE);
                {
                    ihevce_tile_params_t *ps_col_tile_params =
                        ((ihevce_tile_params_t *)ps_ctxt->pv_tile_params_base +
                         ps_ctxt->i4_tile_col_idx);

                    /* No wait for 1st row */
                    cu_top_right_offset = -(ps_col_tile_params->i4_first_sample_x + (MAX_CTB_SIZE));
                }
                cu_top_right_dep_pos = 0;
            }
            else
            {
                cu_top_right_offset = (ps_cu_analyse->u1_cu_size);
                cu_top_right_dep_pos = (i4_ctb_y_off >> 6) - 1;
            }

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
    }
    else
    {
        {
            memcpy(
                &ps_ctxt->ps_enc_out_ctxt->s_cu_prms,
                &ps_ctxt->as_cu_prms[rd_opt_best_idx],
                sizeof(enc_loop_cu_final_prms_t));
        }

        memcpy(
            &ps_ctxt->as_cu_recur_nbr[0],
            &ps_ctxt->as_cu_nbr[rd_opt_best_idx][0],
            sizeof(nbr_4x4_t) * (ps_cu_analyse->u1_cu_size >> 2) *
                (ps_cu_analyse->u1_cu_size >> 2));

        ps_ctxt->ps_enc_out_ctxt->ps_cu_prms = &ps_ctxt->ps_enc_out_ctxt->s_cu_prms;

        ps_ctxt->ps_cu_recur_nbr = &ps_ctxt->as_cu_recur_nbr[0];
    }
#endif

    ps_ctxt->s_pred_buf_data.u4_is_buf_in_use &=
        ~((1 << (ps_ctxt->i4_max_num_inter_rdopt_cands + 4)) - 1);

    return rd_opt_least_cost;
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_process_row \endif
*
* \brief
*    Row level enc_loop pass function
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_curr_src_bufs  : pointer to input yuv buffer (row buffer)
* \param[out] ps_curr_recon_bufs : pointer recon picture structure pointer (row buffer)
* \param[in] ps_ctb_in : pointer CTB structure (output of ME/IPE) (row buffer)
* \param[out] ps_ctb_out : pointer CTB output structure (row buffer)
* \param[out] ps_cu_out : pointer CU output structure (row buffer)
* \param[out] ps_tu_out : pointer TU output structure (row buffer)
* \param[out] pi2_frm_coeffs : pointer coeff output (row buffer)
* \param[in] i4_poc : current poc. Needed to send recon in dist-client mode
*
* \return
*    None
*
* Note : Currently the frame level calcualtions done assumes that
*        framewidth of the input /recon are excat multiple of ctbsize
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_process_row(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    iv_enc_yuv_buf_t *ps_curr_src_bufs,
    iv_enc_yuv_buf_t *ps_curr_recon_bufs,
    iv_enc_yuv_buf_src_t *ps_curr_recon_bufs_src,
    UWORD8 **ppu1_y_subpel_planes,
    ctb_analyse_t *ps_ctb_in,
    ctb_enc_loop_out_t *ps_ctb_out,
    ipe_l0_ctb_analyse_for_me_t *ps_row_ipe_analyse,
    cur_ctb_cu_tree_t *ps_row_cu_tree,
    cu_enc_loop_out_t *ps_row_cu,
    tu_enc_loop_out_t *ps_row_tu,
    pu_t *ps_row_pu,
    pu_col_mv_t *ps_row_col_pu,
    UWORD16 *pu2_num_pu_map,
    UWORD8 *pu1_row_pu_map,
    UWORD8 *pu1_row_ecd_data,
    UWORD32 *pu4_pu_offsets,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 vert_ctr,
    recon_pic_buf_t *ps_frm_recon,
    void *pv_dep_mngr_encloop_dep_me,
    pad_interp_recon_frm_t *ps_pad_interp_recon,
    WORD32 i4_pass,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    ihevce_tile_params_t *ps_tile_params)
{
    enc_loop_cu_prms_t s_cu_prms;
    ctb_enc_loop_out_t *ps_ctb_out_dblk;

    WORD32 ctb_ctr, ctb_start, ctb_end;
    WORD32 col_pu_map_idx;
    WORD32 num_ctbs_horz_pic;
    WORD32 ctb_size;
    WORD32 last_ctb_row_flag;
    WORD32 last_ctb_col_flag;
    WORD32 last_hz_ctb_wd;
    WORD32 last_vt_ctb_ht;
    void *pv_dep_mngr_enc_loop_dblk = ps_ctxt->pv_dep_mngr_enc_loop_dblk;
    void *pv_dep_mngr_enc_loop_sao = ps_ctxt->pv_dep_mngr_enc_loop_sao;
    void *pv_dep_mngr_enc_loop_cu_top_right = ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right;
    WORD32 dblk_offset, dblk_check_dep_pos;
    WORD32 sao_offset, sao_check_dep_pos;
    WORD32 aux_offset, aux_check_dep_pos;
    void *pv_dep_mngr_me_dep_encloop;
    ctb_enc_loop_out_t *ps_ctb_out_sao;
    /*Structure to store deblocking parameters at CTB-row level*/
    deblk_ctbrow_prms_t s_deblk_ctb_row_params;
    UWORD8 is_inp_422 = (ps_ctxt->u1_chroma_array_type == 2);

    pv_dep_mngr_me_dep_encloop = (void *)ps_frm_recon->pv_dep_mngr_recon;
    num_ctbs_horz_pic = ps_frm_ctb_prms->i4_num_ctbs_horz;
    ctb_size = ps_frm_ctb_prms->i4_ctb_size;

    /* Store the num_ctb_horz in sao context*/
    ps_ctxt->s_sao_ctxt_t.u4_num_ctbs_horz = ps_frm_ctb_prms->i4_num_ctbs_horz;
    ps_ctxt->s_sao_ctxt_t.u4_num_ctbs_vert = ps_frm_ctb_prms->i4_num_ctbs_vert;

    /* Set Variables for Dep. Checking and Setting */
    aux_check_dep_pos = vert_ctr;
    aux_offset = 2; /* Should be there for 0th row also */
    if(vert_ctr > 0)
    {
        dblk_check_dep_pos = vert_ctr - 1;
        dblk_offset = 2;
    }
    else
    {
        /* First row should run without waiting */
        dblk_check_dep_pos = 0;
        dblk_offset = -(ps_tile_params->i4_first_sample_x + 1);
    }

    /* Set sao_offset and sao_check_dep_pos */
    if(vert_ctr > 1)
    {
        sao_check_dep_pos = vert_ctr - 2;
        sao_offset = 2;
    }
    else
    {
        /* First row should run without waiting */
        sao_check_dep_pos = 0;
        sao_offset = -(ps_tile_params->i4_first_sample_x + 1);
    }

    /* check if the current row processed in last CTb row */
    last_ctb_row_flag = (vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1));

    /* Valid Width (pixels) in the last CTB in every row (padding cases) */
    last_hz_ctb_wd = ps_frm_ctb_prms->i4_cu_aligned_pic_wd - ((num_ctbs_horz_pic - 1) * ctb_size);

    /* Valid Height (pixels) in the last CTB row (padding cases) */
    last_vt_ctb_ht = ps_frm_ctb_prms->i4_cu_aligned_pic_ht -
                     ((ps_frm_ctb_prms->i4_num_ctbs_vert - 1) * ctb_size);
    /* reset the states copied flag */
    ps_ctxt->u1_cabac_states_next_row_copied_flag = 0;
    ps_ctxt->u1_cabac_states_first_cu_copied_flag = 0;

    /* populate the cu prms which are common for entire ctb row */
    s_cu_prms.i4_luma_src_stride = ps_curr_src_bufs->i4_y_strd;
    s_cu_prms.i4_chrm_src_stride = ps_curr_src_bufs->i4_uv_strd;
    s_cu_prms.i4_luma_recon_stride = ps_curr_recon_bufs->i4_y_strd;
    s_cu_prms.i4_chrm_recon_stride = ps_curr_recon_bufs->i4_uv_strd;
    s_cu_prms.i4_ctb_size = ctb_size;

    ps_ctxt->i4_is_first_cu_qg_coded = 0;

    /* Initialize the number of PUs for the first CTB to 0 */
    *pu2_num_pu_map = 0;

    /*Getting the address of BS and Qp arrays and other info*/
    memcpy(&s_deblk_ctb_row_params, &ps_ctxt->s_deblk_ctbrow_prms, sizeof(deblk_ctbrow_prms_t));
    {
        WORD32 num_ctbs_horz_tile;
        /* Update the pointers which are accessed not by using ctb_ctr
        to the tile start here! */
        ps_ctb_in += ps_tile_params->i4_first_ctb_x;
        ps_ctb_out += ps_tile_params->i4_first_ctb_x;

        ps_row_cu += (ps_tile_params->i4_first_ctb_x * ps_frm_ctb_prms->i4_num_cus_in_ctb);
        ps_row_tu += (ps_tile_params->i4_first_ctb_x * ps_frm_ctb_prms->i4_num_tus_in_ctb);
        ps_row_pu += (ps_tile_params->i4_first_ctb_x * ps_frm_ctb_prms->i4_num_pus_in_ctb);
        pu1_row_pu_map += (ps_tile_params->i4_first_ctb_x * ps_frm_ctb_prms->i4_num_pus_in_ctb);
        pu1_row_ecd_data +=
            (ps_tile_params->i4_first_ctb_x *
             ((is_inp_422 == 1) ? (ps_frm_ctb_prms->i4_num_tus_in_ctb << 1)
                                : ((ps_frm_ctb_prms->i4_num_tus_in_ctb * 3) >> 1)) *
             MAX_SCAN_COEFFS_BYTES_4x4);

        /* Update the pointers to the tile start */
        s_deblk_ctb_row_params.pu4_ctb_row_bs_vert +=
            (ps_tile_params->i4_first_ctb_x * (ctb_size >> 3));  //one vertical edge per 8x8 block
        s_deblk_ctb_row_params.pu4_ctb_row_bs_horz +=
            (ps_tile_params->i4_first_ctb_x * (ctb_size >> 3));  //one horizontal edge per 8x8 block
        s_deblk_ctb_row_params.pi1_ctb_row_qp += (ps_tile_params->i4_first_ctb_x * (ctb_size >> 2));

        num_ctbs_horz_tile = ps_tile_params->i4_curr_tile_wd_in_ctb_unit;

        ctb_start = ps_tile_params->i4_first_ctb_x;
        ctb_end = ps_tile_params->i4_first_ctb_x + num_ctbs_horz_tile;
    }
    ps_ctb_out_dblk = ps_ctb_out;

    ps_ctxt->i4_last_cu_qp_from_prev_ctb = ps_ctxt->i4_frame_qp;

    /* --------- Loop over all the CTBs in a row --------------- */
    for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
    {
        cu_final_update_prms s_cu_update_prms;

        cur_ctb_cu_tree_t *ps_cu_tree_analyse;
        me_ctb_data_t *ps_cu_me_data;
        ipe_l0_ctb_analyse_for_me_t *ps_ctb_ipe_analyse;
        cu_enc_loop_out_t *ps_cu_final;
        pu_col_mv_t *ps_ctb_col_pu;

        WORD32 cur_ctb_ht, cur_ctb_wd;
        WORD32 last_cu_pos_in_ctb;
        WORD32 last_cu_size;
        WORD32 num_pus_in_ctb;
        UWORD8 u1_is_ctb_noisy;
        ps_ctb_col_pu = ps_row_col_pu + ctb_ctr * ps_frm_ctb_prms->i4_num_pus_in_ctb;

        if(ctb_ctr)
        {
            ps_ctxt->i4_prev_QP = ps_ctxt->i4_last_cu_qp_from_prev_ctb;
        }
        /*If Sup pic rc is enabled*/
        if(ps_ctxt->i4_sub_pic_level_rc)
        {
            ihevce_sub_pic_rc_scale_query((void *)ps_multi_thrd_ctxt, (void *)ps_ctxt);
        }
        /* check if the current row processed in last CTb row */
        last_ctb_col_flag = (ctb_ctr == (num_ctbs_horz_pic - 1));
        if(1 == last_ctb_col_flag)
        {
            cur_ctb_wd = last_hz_ctb_wd;
        }
        else
        {
            cur_ctb_wd = ctb_size;
        }

        /* If it's the last CTB, get the actual ht of CTB */
        if(1 == last_ctb_row_flag)
        {
            cur_ctb_ht = last_vt_ctb_ht;
        }
        else
        {
            cur_ctb_ht = ctb_size;
        }

        ps_ctxt->u4_cur_ctb_ht = cur_ctb_ht;
        ps_ctxt->u4_cur_ctb_wd = cur_ctb_wd;

        /* Wait till reference frame recon is available */

        /* ------------ Wait till current data is ready from ME -------------- */

        /*only for ref instance and Non I pics */
        if((ps_ctxt->i4_bitrate_instance_num == 0) &&
           ((ISLICE != ps_ctxt->i1_slice_type) || L0ME_IN_OPENLOOP_MODE))
        {
            if(ctb_ctr < (num_ctbs_horz_pic))
            {
                ihevce_dmgr_chk_row_row_sync(
                    pv_dep_mngr_encloop_dep_me,
                    ctb_ctr,
                    1,
                    vert_ctr,
                    ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                    ps_ctxt->thrd_id);
            }
        }

        /* store the cu pointer for current ctb out */
        ps_ctb_out->ps_enc_cu = ps_row_cu;
        ps_cu_final = ps_row_cu;

        /* Get the base point of CU recursion tree */
        if(ISLICE != ps_ctxt->i1_slice_type)
        {
            ps_cu_tree_analyse = ps_ctb_in->ps_cu_tree;
            ASSERT(ps_ctb_in->ps_cu_tree == (ps_row_cu_tree + (ctb_ctr * MAX_NUM_NODES_CU_TREE)));
        }
        else
        {
            /* Initialize ptr to current CTB */
            ps_cu_tree_analyse = ps_row_cu_tree + (ctb_ctr * MAX_NUM_NODES_CU_TREE);
        }

        /* Get the ME data pointer for 16x16 block data in ctb */
        ps_cu_me_data = ps_ctb_in->ps_me_ctb_data;
        u1_is_ctb_noisy = ps_ctb_in->s_ctb_noise_params.i4_noise_present;
        s_cu_prms.u1_is_cu_noisy = u1_is_ctb_noisy;
        s_cu_prms.pu1_is_8x8Blk_noisy = ps_ctb_in->s_ctb_noise_params.au1_is_8x8Blk_noisy;

        /* store the ctb level prms in cu prms */
        s_cu_prms.i4_ctb_pos = ctb_ctr;

        s_cu_prms.pu1_luma_src = (UWORD8 *)ps_curr_src_bufs->pv_y_buf + ctb_ctr * ctb_size;
        s_cu_prms.pu1_luma_recon = (UWORD8 *)ps_curr_recon_bufs->pv_y_buf + ctb_ctr * ctb_size;

        {
            s_cu_prms.pu1_chrm_src = (UWORD8 *)ps_curr_src_bufs->pv_u_buf + ctb_ctr * ctb_size;
            s_cu_prms.pu1_chrm_recon = (UWORD8 *)ps_curr_recon_bufs->pv_u_buf + ctb_ctr * ctb_size;
        }

        s_cu_prms.pu1_sbpel_hxfy = (UWORD8 *)ppu1_y_subpel_planes[0] + ctb_ctr * ctb_size;

        s_cu_prms.pu1_sbpel_fxhy = (UWORD8 *)ppu1_y_subpel_planes[1] + ctb_ctr * ctb_size;

        s_cu_prms.pu1_sbpel_hxhy = (UWORD8 *)ppu1_y_subpel_planes[2] + ctb_ctr * ctb_size;

        /* Initialize ptr to current CTB */
        ps_ctb_ipe_analyse = ps_row_ipe_analyse + ctb_ctr;  // * ctb_size;

        /* reset the map idx for current ctb */
        col_pu_map_idx = 0;
        num_pus_in_ctb = 0;

        /* reset the map buffer to 0*/

        memset(
            &ps_ctxt->au1_nbr_ctb_map[0][0],
            0,
            (MAX_PU_IN_CTB_ROW + 1 + 8) * (MAX_PU_IN_CTB_ROW + 1 + 8));

        /* set the CTB neighbour availability flags */
        ihevce_set_ctb_nbr(
            &ps_ctb_out->s_ctb_nbr_avail_flags,
            ps_ctxt->pu1_ctb_nbr_map,
            ps_ctxt->i4_nbr_map_strd,
            ctb_ctr,
            vert_ctr,
            ps_frm_ctb_prms);

        /* -------- update the cur CTB offsets for inter prediction-------- */
        ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_x = ctb_ctr * ctb_size;
        ps_ctxt->s_mc_ctxt.i4_ctb_frm_pos_y = vert_ctr * ctb_size;

        /* -------- update the cur CTB offsets for MV prediction-------- */
        ps_ctxt->s_mv_pred_ctxt.i4_ctb_x = ctb_ctr;
        ps_ctxt->s_mv_pred_ctxt.i4_ctb_y = vert_ctr;

        /* -------------- Boundary Strength Initialization ----------- */
        if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
        {
            ihevce_bs_init_ctb(&ps_ctxt->s_deblk_bs_prms, ps_frm_ctb_prms, ctb_ctr, vert_ctr);
        }

        /* -------- update cur CTB offsets for entropy rdopt context------- */
        ihevce_entropy_rdo_ctb_init(&ps_ctxt->s_rdopt_entropy_ctxt, ctb_ctr, vert_ctr);

        /* --------- CU Recursion --------------- */

        {
#if PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
            WORD32 i4_max_tree_depth = 4;
#endif
            WORD32 i4_tree_depth = 0;
            /* Init no. of CU in CTB to 0*/
            ps_ctb_out->u1_num_cus_in_ctb = 0;

#if PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
            if(ps_ctxt->i4_bitrate_instance_num == 0)
            {
                WORD32 i4_max_tree_depth = 4;
                WORD32 i;
                for(i = 0; i < i4_max_tree_depth; i++)
                {
                    COPY_CABAC_STATES(
                        &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                        &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                        IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                }
            }
#else
            if(ps_ctxt->i4_bitrate_instance_num == 0)
            {
                if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
                {
                    WORD32 i4_max_tree_depth = 4;
                    WORD32 i;
                    for(i = 0; i < i4_max_tree_depth; i++)
                    {
                        COPY_CABAC_STATES(
                            &ps_ctxt->au1_rdopt_recur_ctxt_models[i][0],
                            &ps_ctxt->s_rdopt_entropy_ctxt.au1_init_cabac_ctxt_states[0],
                            IHEVC_CAB_CTXT_END * sizeof(UWORD8));
                    }
                }
            }

#endif
            if(ps_ctxt->i4_bitrate_instance_num == 0)
            {
                /* FOR I- PIC populate the curr_ctb accordingly */
                if(ISLICE == ps_ctxt->i1_slice_type)
                {
                    ps_ctb_ipe_analyse->ps_cu_tree_root = ps_cu_tree_analyse;
                    ps_ctb_ipe_analyse->nodes_created_in_cu_tree = 1;

                    ihevce_populate_cu_tree(
                        ps_ctb_ipe_analyse,
                        ps_cu_tree_analyse,
                        0,
                        (IHEVCE_QUALITY_CONFIG_T)ps_ctxt->i4_quality_preset,
                        POS_NA,
                        POS_NA,
                        POS_NA);
                }
            }
            ps_ctb_ipe_analyse->nodes_created_in_cu_tree = 1;
            ps_ctxt->ps_enc_out_ctxt = &ps_ctxt->as_enc_cu_ctxt[0];
            ps_ctxt->pu1_ecd_data = pu1_row_ecd_data;

            s_cu_update_prms.ppu1_row_ecd_data = &pu1_row_ecd_data;
            s_cu_update_prms.pi4_last_cu_pos_in_ctb = &last_cu_pos_in_ctb;
            s_cu_update_prms.pi4_last_cu_size = &last_cu_size;
            s_cu_update_prms.pi4_num_pus_in_ctb = &num_pus_in_ctb;
            s_cu_update_prms.pps_cu_final = &ps_cu_final;
            s_cu_update_prms.pps_row_pu = &ps_row_pu;
            s_cu_update_prms.pps_row_tu = &ps_row_tu;
            s_cu_update_prms.pu1_num_cus_in_ctb_out = &ps_ctb_out->u1_num_cus_in_ctb;

            // source satd computation
            /* compute the source 8x8 SATD for the current CTB */
            /* populate  pui4_source_satd in some structure and pass it inside */
            if(ps_ctxt->u1_enable_psyRDOPT)
            {
                /* declare local variables */
                WORD32 i;
                WORD32 ctb_size;
                WORD32 num_comp_had_blocks;
                UWORD8 *pu1_l0_block;
                WORD32 block_ht;
                WORD32 block_wd;
                WORD32 ht_offset;
                WORD32 wd_offset;

                WORD32 num_horz_blocks;
                WORD32 had_block_size;
                WORD32 total_had_block_size;
                WORD16 pi2_residue_had_zscan[64];
                UWORD8 ai1_zeros_buffer[64];

                WORD32 index_satd;
                WORD32 is_hbd;
                /* initialize the variables */
                block_ht = cur_ctb_ht;
                block_wd = cur_ctb_wd;

                is_hbd = ps_ctxt->u1_is_input_data_hbd;

                had_block_size = 8;
                total_had_block_size = had_block_size * had_block_size;

                for(i = 0; i < total_had_block_size; i++)
                {
                    ai1_zeros_buffer[i] = 0;
                }

                ctb_size = block_ht * block_wd;  //ctb_width * ctb_height;
                num_comp_had_blocks = ctb_size / (had_block_size * had_block_size);

                num_horz_blocks = block_wd / had_block_size;  //ctb_width / had_block_size;
                ht_offset = -had_block_size;
                wd_offset = -had_block_size;

                index_satd = 0;
                /*Loop over all 8x8 blocsk in the CTB*/
                for(i = 0; i < num_comp_had_blocks; i++)
                {
                    if(i % num_horz_blocks == 0)
                    {
                        wd_offset = -had_block_size;
                        ht_offset += had_block_size;
                    }
                    wd_offset += had_block_size;

                    if(!is_hbd)
                    {
                        /* get memory pointers for each of L0 and L1 blocks whose hadamard has to be computed */
                        pu1_l0_block = s_cu_prms.pu1_luma_src +
                                       ps_curr_src_bufs->i4_y_strd * ht_offset + wd_offset;

                        ps_ctxt->ai4_source_satd_8x8[index_satd] =

                            ps_ctxt->s_cmn_opt_func.pf_AC_HAD_8x8_8bit(
                                pu1_l0_block,
                                ps_curr_src_bufs->i4_y_strd,
                                ai1_zeros_buffer,
                                had_block_size,
                                pi2_residue_had_zscan,
                                had_block_size);
                    }
                    index_satd++;
                }
            }

            if(ps_ctxt->u1_enable_psyRDOPT)
            {
                /* declare local variables */
                WORD32 i;
                WORD32 ctb_size;
                WORD32 num_comp_had_blocks;
                UWORD8 *pu1_l0_block;
                UWORD8 *pu1_l0_block_prev = NULL;
                WORD32 block_ht;
                WORD32 block_wd;
                WORD32 ht_offset;
                WORD32 wd_offset;

                WORD32 num_horz_blocks;
                WORD32 had_block_size;
                WORD16 pi2_residue_had[64];
                UWORD8 ai1_zeros_buffer[64];
                WORD32 index_satd = 0;

                WORD32 is_hbd;
                is_hbd = ps_ctxt->u1_is_input_data_hbd;  // 8 bit

                /* initialize the variables */
                /* change this based ont he bit depth */
                // ps_ctxt->u1_chroma_array_type
                if(ps_ctxt->u1_chroma_array_type == 1)
                {
                    block_ht = cur_ctb_ht / 2;
                    block_wd = cur_ctb_wd / 2;
                }
                else
                {
                    block_ht = cur_ctb_ht;
                    block_wd = cur_ctb_wd / 2;
                }

                had_block_size = 4;
                memset(ai1_zeros_buffer, 0, 64 * sizeof(UWORD8));

                ctb_size = block_ht * block_wd;  //ctb_width * ctb_height;
                num_comp_had_blocks = 2 * ctb_size / (had_block_size * had_block_size);

                num_horz_blocks = 2 * block_wd / had_block_size;  //ctb_width / had_block_size;
                ht_offset = -had_block_size;
                wd_offset = -had_block_size;

                if(!is_hbd)
                {
                    /* loop over for every 4x4 blocks in the CU for Cb */
                    for(i = 0; i < num_comp_had_blocks; i++)
                    {
                        if(i % num_horz_blocks == 0)
                        {
                            wd_offset = -had_block_size;
                            ht_offset += had_block_size;
                        }
                        wd_offset += had_block_size;

                        /* get memory pointers for each of L0 and L1 blocks whose hadamard has to be computed */
                        if(i % 2 != 0)
                        {
                            if(!is_hbd)
                            {
                                pu1_l0_block = pu1_l0_block_prev + 1;
                            }
                        }
                        else
                        {
                            if(!is_hbd)
                            {
                                pu1_l0_block = s_cu_prms.pu1_chrm_src +
                                               s_cu_prms.i4_chrm_src_stride * ht_offset + wd_offset;
                                pu1_l0_block_prev = pu1_l0_block;
                            }
                        }

                        if(had_block_size == 4)
                        {
                            if(!is_hbd)
                            {
                                ps_ctxt->ai4_source_chroma_satd[index_satd] =
                                    ps_ctxt->s_cmn_opt_func.pf_chroma_AC_HAD_4x4_8bit(
                                        pu1_l0_block,
                                        s_cu_prms.i4_chrm_src_stride,
                                        ai1_zeros_buffer,
                                        had_block_size,
                                        pi2_residue_had,
                                        had_block_size);
                            }

                            index_satd++;

                        }  // block size of 4x4

                    }  // for all blocks

                }  // is hbd check
            }

            ihevce_cu_recurse_decide(
                ps_ctxt,
                &s_cu_prms,
                ps_cu_tree_analyse,
                ps_cu_tree_analyse,
                ps_ctb_ipe_analyse,
                ps_cu_me_data,
                &ps_ctb_col_pu,
                &s_cu_update_prms,
                pu1_row_pu_map,
                &col_pu_map_idx,
                i4_tree_depth,
                ctb_ctr << 6,
                vert_ctr << 6,
                cur_ctb_ht);

            if(ps_ctxt->i1_slice_type != ISLICE)
            {
                ASSERT(
                    (cur_ctb_wd * cur_ctb_ht) <=
                    ihevce_compute_area_of_valid_cus_in_ctb(ps_cu_tree_analyse));
            }
            /*If Sup pic rc is enabled*/
            if(1 == ps_ctxt->i4_sub_pic_level_rc)
            {
                /*In a row, after the required CTB is reached, send data and query scale from Bit Control thread */
                ihevce_sub_pic_rc_in_data(
                    (void *)ps_multi_thrd_ctxt,
                    (void *)ps_ctxt,
                    (void *)ps_ctb_ipe_analyse,
                    (void *)ps_frm_ctb_prms);
            }

            ps_ctxt->ps_enc_out_ctxt->u1_cu_size = 128;

        } /* End of CU recursion block */

#if PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
        {
            ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt = &ps_ctxt->as_enc_cu_ctxt[0];
            enc_loop_cu_prms_t *ps_cu_prms = &s_cu_prms;
            ps_ctxt->pu1_ecd_data = pu1_row_ecd_data;

            do
            {
                ihevce_update_final_cu_results(
                    ps_ctxt,
                    ps_enc_out_ctxt,
                    ps_cu_prms,
                    NULL, /* &ps_ctb_col_pu */
                    NULL, /* &col_pu_map_idx */
                    &s_cu_update_prms,
                    ctb_ctr,
                    vert_ctr);

                ps_enc_out_ctxt++;

                ASSERT(ps_ctb_in->u1_num_cus_in_ctb <= MAX_CTB_SIZE);

            } while(ps_enc_out_ctxt->u1_cu_size != 128);
        }
#else
        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
            ihevce_enc_cu_node_ctxt_t *ps_enc_out_ctxt = &ps_ctxt->as_enc_cu_ctxt[0];
            enc_loop_cu_prms_t *ps_cu_prms = &s_cu_prms;
            ps_ctxt->pu1_ecd_data = pu1_row_ecd_data;

            do
            {
                ihevce_update_final_cu_results(
                    ps_ctxt,
                    ps_enc_out_ctxt,
                    ps_cu_prms,
                    NULL, /* &ps_ctb_col_pu */
                    NULL, /* &col_pu_map_idx */
                    &s_cu_update_prms,
                    ctb_ctr,
                    vert_ctr);

                ps_enc_out_ctxt++;

                ASSERT(ps_ctb_in->u1_num_cus_in_ctb <= MAX_CTB_SIZE);

            } while(ps_enc_out_ctxt->u1_cu_size != 128);
        }
#endif

        /* --- ctb level copy of data to left buffers--*/
        ((pf_enc_loop_ctb_left_copy)ps_ctxt->pv_enc_loop_ctb_left_copy)(ps_ctxt, &s_cu_prms);

        if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
        {
            /* For the Unaligned CTB, make the invalid edge boundary strength 0 */
            ihevce_bs_clear_invalid(
                &ps_ctxt->s_deblk_bs_prms,
                last_ctb_row_flag,
                (ctb_ctr == (num_ctbs_horz_pic - 1)),
                last_hz_ctb_wd,
                last_vt_ctb_ht);

            /* -----------------Read boundary strengts for current CTB------------- */

            if((0 == ps_ctxt->i4_deblock_type) && (ps_ctxt->i4_deblk_pad_hpel_cur_pic))
            {
                /*Storing boundary strengths of current CTB*/
                UWORD32 *pu4_bs_horz = &ps_ctxt->s_deblk_bs_prms.au4_horz_bs[0];
                UWORD32 *pu4_bs_vert = &ps_ctxt->s_deblk_bs_prms.au4_vert_bs[0];

                memcpy(s_deblk_ctb_row_params.pu4_ctb_row_bs_vert, pu4_bs_vert, (ctb_size * 4) / 8);
                memcpy(s_deblk_ctb_row_params.pu4_ctb_row_bs_horz, pu4_bs_horz, (ctb_size * 4) / 8);
            }
            //Increment for storing next CTB info
            s_deblk_ctb_row_params.pu4_ctb_row_bs_vert +=
                (ctb_size >> 3);  //one vertical edge per 8x8 block
            s_deblk_ctb_row_params.pu4_ctb_row_bs_horz +=
                (ctb_size >> 3);  //one horizontal edge per 8x8 block
        }

        /* -------------- ctb level updates ----------------- */
        ps_row_cu += ps_ctb_out->u1_num_cus_in_ctb;

        pu1_row_pu_map += (ctb_size >> 2) * (ctb_size >> 2);

        /* first ctb offset will be populated by the caller */
        if(0 != ctb_ctr)
        {
            pu4_pu_offsets[ctb_ctr] = pu4_pu_offsets[ctb_ctr - 1] + num_pus_in_ctb;
        }
        pu2_num_pu_map[ctb_ctr] = num_pus_in_ctb;
        ASSERT(ps_ctb_out->u1_num_cus_in_ctb != 0);

        ps_ctb_in++;
        ps_ctb_out++;
    }

    /* ---------- Encloop end of row updates ----------------- */

    /* at the end of row processing cu pixel counter is set to */
    /* (num ctb * ctbzise) + ctb size                          */
    /* this is to set the dependency for right most cu of last */
    /* ctb's top right data dependency                         */
    /* this even takes care of entropy dependency for          */
    /* incomplete ctb as well                                  */
    ihevce_dmgr_set_row_row_sync(
        pv_dep_mngr_enc_loop_cu_top_right,
        (ctb_ctr * ctb_size + ctb_size),
        vert_ctr,
        ps_ctxt->i4_tile_col_idx /* Col Tile No. */);

    ps_ctxt->s_sao_ctxt_t.ps_cmn_utils_optimised_function_list = &ps_ctxt->s_cmn_opt_func;

    /* Restore structure.
    Getting the address of stored-BS and Qp-map and other info */
    memcpy(&s_deblk_ctb_row_params, &ps_ctxt->s_deblk_ctbrow_prms, sizeof(deblk_ctbrow_prms_t));
    {
        /* Update the pointers to the tile start */
        s_deblk_ctb_row_params.pu4_ctb_row_bs_vert +=
            (ps_tile_params->i4_first_ctb_x * (ctb_size >> 3));  //one vertical edge per 8x8 block
        s_deblk_ctb_row_params.pu4_ctb_row_bs_horz +=
            (ps_tile_params->i4_first_ctb_x * (ctb_size >> 3));  //one horizontal edge per 8x8 block
        s_deblk_ctb_row_params.pi1_ctb_row_qp += (ps_tile_params->i4_first_ctb_x * (ctb_size >> 2));
    }

#if PROFILE_ENC_REG_DATA
    s_profile.u8_enc_reg_data[vert_ctr] = 0;
#endif

    /* -- Loop over all the CTBs in a row for Deblocking and Subpel gen --- */
    if(!ps_ctxt->u1_is_input_data_hbd)
    {
        WORD32 last_col_pic, last_col_tile;

        for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
        {
            /* store the ctb level prms in cu prms */
            s_cu_prms.i4_ctb_pos = ctb_ctr;
            s_cu_prms.pu1_luma_src = (UWORD8 *)ps_curr_src_bufs->pv_y_buf + ctb_ctr * ctb_size;
            s_cu_prms.pu1_chrm_src = (UWORD8 *)ps_curr_src_bufs->pv_u_buf + ctb_ctr * ctb_size;

            s_cu_prms.pu1_luma_recon = (UWORD8 *)ps_curr_recon_bufs->pv_y_buf + ctb_ctr * ctb_size;
            s_cu_prms.pu1_chrm_recon = (UWORD8 *)ps_curr_recon_bufs->pv_u_buf + ctb_ctr * ctb_size;
            s_cu_prms.pu1_sbpel_hxfy = (UWORD8 *)ppu1_y_subpel_planes[0] + ctb_ctr * ctb_size;

            s_cu_prms.pu1_sbpel_fxhy = (UWORD8 *)ppu1_y_subpel_planes[1] + ctb_ctr * ctb_size;

            s_cu_prms.pu1_sbpel_hxhy = (UWORD8 *)ppu1_y_subpel_planes[2] + ctb_ctr * ctb_size;

            /* If last ctb in the horizontal row */
            if(ctb_ctr == (num_ctbs_horz_pic - 1))
            {
                last_col_pic = 1;
            }
            else
            {
                last_col_pic = 0;
            }

            /* If last ctb in the tile row */
            if(ctb_ctr == (ctb_end - 1))
            {
                last_col_tile = 1;
            }
            else
            {
                last_col_tile = 0;
            }

            if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
            {
                /* for last ctb of a row check top instead of top right */
                if(((ctb_ctr + 1) == ctb_end) && (vert_ctr > 0))
                {
                    dblk_offset = 1;
                }
                /* Wait till top neighbour CTB has done it's deblocking*/
                ihevce_dmgr_chk_row_row_sync(
                    pv_dep_mngr_enc_loop_dblk,
                    ctb_ctr,
                    dblk_offset,
                    dblk_check_dep_pos,
                    ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                    ps_ctxt->thrd_id);

                if((0 == ps_ctxt->i4_deblock_type))
                {
                    /* Populate Qp-map */
                    if(ctb_start == ctb_ctr)
                    {
                        ihevce_deblk_populate_qp_map(
                            ps_ctxt,
                            &s_deblk_ctb_row_params,
                            ps_ctb_out_dblk,
                            vert_ctr,
                            ps_frm_ctb_prms,
                            ps_tile_params);
                    }
                    ps_ctxt->s_deblk_prms.i4_ctb_size = ctb_size;

                    /* recon pointers and stride */
                    ps_ctxt->s_deblk_prms.pu1_ctb_y = s_cu_prms.pu1_luma_recon;
                    ps_ctxt->s_deblk_prms.pu1_ctb_uv = s_cu_prms.pu1_chrm_recon;
                    ps_ctxt->s_deblk_prms.i4_luma_pic_stride = s_cu_prms.i4_luma_recon_stride;
                    ps_ctxt->s_deblk_prms.i4_chroma_pic_stride = s_cu_prms.i4_chrm_recon_stride;

                    ps_ctxt->s_deblk_prms.i4_deblock_top_ctb_edge = (0 == vert_ctr) ? 0 : 1;
                    {
                        ps_ctxt->s_deblk_prms.i4_deblock_top_ctb_edge =
                            (ps_tile_params->i4_first_ctb_y == vert_ctr) ? 0 : 1;
                    }
                    ps_ctxt->s_deblk_prms.i4_deblock_left_ctb_edge = (ctb_start == ctb_ctr) ? 0 : 1;
                    //or according to slice boundary. Support yet to be added !!!!

                    ihevce_deblk_ctb(
                        &ps_ctxt->s_deblk_prms, last_col_tile, &s_deblk_ctb_row_params);

                    //Increment for storing next CTB info
                    s_deblk_ctb_row_params.pu4_ctb_row_bs_vert +=
                        (ctb_size >> 3);  //one vertical edge per 8x8 block
                    s_deblk_ctb_row_params.pu4_ctb_row_bs_horz +=
                        (ctb_size >> 3);  //one horizontal edge per 8x8 block
                    s_deblk_ctb_row_params.pi1_ctb_row_qp +=
                        (ctb_size >> 2);  //one qp per 4x4 block.
                }
            }  // end of if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)

            /* update the number of ctbs deblocked for this row */
            ihevce_dmgr_set_row_row_sync(
                pv_dep_mngr_enc_loop_dblk,
                (ctb_ctr + 1),
                vert_ctr,
                ps_ctxt->i4_tile_col_idx /* Col Tile No. */);

        }  //end of loop over CTBs in current CTB-row

        /* Apply SAO over the previous CTB-row */
        for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
        {
            if(ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
               ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
            {
                sao_ctxt_t *ps_sao_ctxt = &ps_ctxt->s_sao_ctxt_t;

                if(vert_ctr > ps_tile_params->i4_first_ctb_y)
                {
                    /*For last ctb check top dep only*/
                    if((vert_ctr > 1) && ((ctb_ctr + 1) == ctb_end))
                    {
                        sao_offset = 1;
                    }

                    ihevce_dmgr_chk_row_row_sync(
                        pv_dep_mngr_enc_loop_sao,
                        ctb_ctr,
                        sao_offset,
                        sao_check_dep_pos,
                        ps_ctxt->i4_tile_col_idx, /* Col Tile No. */
                        ps_ctxt->thrd_id);

                    /* Call the sao function to do sao for the current ctb*/

                    /* Register the curr ctb's x pos in sao context*/
                    ps_sao_ctxt->i4_ctb_x = ctb_ctr;

                    /* Register the curr ctb's y pos in sao context*/
                    ps_sao_ctxt->i4_ctb_y = vert_ctr - 1;

                    ps_ctb_out_sao = ps_sao_ctxt->ps_ctb_out +
                                     (vert_ctr - 1) * ps_frm_ctb_prms->i4_num_ctbs_horz + ctb_ctr;
                    ps_sao_ctxt->ps_sao = &ps_ctb_out_sao->s_sao;
                    ps_sao_ctxt->i4_sao_blk_wd = ctb_size;
                    ps_sao_ctxt->i4_sao_blk_ht = ctb_size;

                    ps_sao_ctxt->i4_is_last_ctb_row = 0;
                    ps_sao_ctxt->i4_is_last_ctb_col = 0;

                    if((ctb_ctr + 1) == ctb_end)
                    {
                        ps_sao_ctxt->i4_is_last_ctb_col = 1;
                        ps_sao_ctxt->i4_sao_blk_wd =
                            ctb_size - ((ps_tile_params->i4_curr_tile_wd_in_ctb_unit * ctb_size) -
                                        ps_tile_params->i4_curr_tile_width);
                    }

                    /* Calculate the recon buf pointer and stride for teh current ctb */
                    ps_sao_ctxt->pu1_cur_luma_recon_buf =
                        ps_sao_ctxt->pu1_frm_luma_recon_buf +
                        (ps_sao_ctxt->i4_frm_luma_recon_stride * ps_sao_ctxt->i4_ctb_y * ctb_size) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_luma_recon_stride = ps_sao_ctxt->i4_frm_luma_recon_stride;

                    ps_sao_ctxt->pu1_cur_chroma_recon_buf =
                        ps_sao_ctxt->pu1_frm_chroma_recon_buf +
                        (ps_sao_ctxt->i4_frm_chroma_recon_stride * ps_sao_ctxt->i4_ctb_y *
                         (ctb_size >> (ps_ctxt->u1_chroma_array_type == 1))) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_chroma_recon_stride =
                        ps_sao_ctxt->i4_frm_chroma_recon_stride;

                    ps_sao_ctxt->pu1_cur_luma_src_buf =
                        ps_sao_ctxt->pu1_frm_luma_src_buf +
                        (ps_sao_ctxt->i4_frm_luma_src_stride * ps_sao_ctxt->i4_ctb_y * ctb_size) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_luma_src_stride = ps_sao_ctxt->i4_frm_luma_src_stride;

                    ps_sao_ctxt->pu1_cur_chroma_src_buf =
                        ps_sao_ctxt->pu1_frm_chroma_src_buf +
                        (ps_sao_ctxt->i4_frm_chroma_src_stride * ps_sao_ctxt->i4_ctb_y *
                         (ctb_size >> (ps_ctxt->u1_chroma_array_type == 1))) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_chroma_src_stride = ps_sao_ctxt->i4_frm_chroma_src_stride;

                    /* Calculate the pointer to buff to store the (x,y)th sao
                    * for the top merge of (x,y+1)th ctb
                    */
                    ps_sao_ctxt->ps_top_ctb_sao =
                        &ps_sao_ctxt->aps_frm_top_ctb_sao[ps_ctxt->i4_enc_frm_id]
                                                         [ps_sao_ctxt->i4_ctb_x +
                                                          (ps_sao_ctxt->i4_ctb_y) *
                                                              ps_frm_ctb_prms->i4_num_ctbs_horz +
                                                          (ps_ctxt->i4_bitrate_instance_num *
                                                           ps_sao_ctxt->i4_num_ctb_units)];

                    /* Calculate the pointer to buff to store the top pixels of curr ctb*/
                    ps_sao_ctxt->pu1_curr_sao_src_top_luma =
                        ps_sao_ctxt->apu1_sao_src_frm_top_luma[ps_ctxt->i4_enc_frm_id] +
                        (ps_sao_ctxt->i4_ctb_y - 1) * ps_sao_ctxt->i4_frm_top_luma_buf_stride +
                        ps_sao_ctxt->i4_ctb_x * ctb_size +
                        ps_ctxt->i4_bitrate_instance_num * (ps_sao_ctxt->i4_top_luma_buf_size +
                                                            ps_sao_ctxt->i4_top_chroma_buf_size);

                    /* Calculate the pointer to buff to store the top pixels of curr ctb*/
                    ps_sao_ctxt->pu1_curr_sao_src_top_chroma =
                        ps_sao_ctxt->apu1_sao_src_frm_top_chroma[ps_ctxt->i4_enc_frm_id] +
                        (ps_sao_ctxt->i4_ctb_y - 1) * ps_sao_ctxt->i4_frm_top_chroma_buf_stride +
                        ps_sao_ctxt->i4_ctb_x * ctb_size +
                        ps_ctxt->i4_bitrate_instance_num * (ps_sao_ctxt->i4_top_luma_buf_size +
                                                            ps_sao_ctxt->i4_top_chroma_buf_size);

                    {
                        UWORD32 u4_ctb_sao_bits;

                        ihevce_sao_analyse(
                            &ps_ctxt->s_sao_ctxt_t,
                            ps_ctb_out_sao,
                            &u4_ctb_sao_bits,
                            ps_tile_params);
                        ps_ctxt
                            ->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id]
                                                     [ps_ctxt->i4_bitrate_instance_num]
                            ->u4_frame_rdopt_header_bits += u4_ctb_sao_bits;
                        ps_ctxt
                            ->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id]
                                                     [ps_ctxt->i4_bitrate_instance_num]
                            ->u4_frame_rdopt_bits += u4_ctb_sao_bits;
                    }
                    /** Subpel generation not done for non-ref picture **/
                    if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                    {
                        /* Recon Padding */
                        ihevce_recon_padding(
                            ps_pad_interp_recon,
                            ctb_ctr,
                            vert_ctr - 1,
                            ps_frm_ctb_prms,
                            ps_ctxt->ps_func_selector);
                    }
                    /* update the number of SAO ctbs for this row */
                    ihevce_dmgr_set_row_row_sync(
                        pv_dep_mngr_enc_loop_sao,
                        ctb_ctr + 1,
                        vert_ctr - 1,
                        ps_ctxt->i4_tile_col_idx /* Col Tile No. */);
                }
            }
            else  //SAO Disabled
            {
                if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                {
                    /* Recon Padding */
                    ihevce_recon_padding(
                        ps_pad_interp_recon,
                        ctb_ctr,
                        vert_ctr,
                        ps_frm_ctb_prms,
                        ps_ctxt->ps_func_selector);
                }
            }
        }  // end of SAO for loop

        /* Call the sao function again for the last ctb row of frame */
        if(ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
           ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
        {
            sao_ctxt_t *ps_sao_ctxt = &ps_ctxt->s_sao_ctxt_t;

            if(vert_ctr ==
               (ps_tile_params->i4_first_ctb_y + ps_tile_params->i4_curr_tile_ht_in_ctb_unit - 1))
            {
                for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
                {
                    /* Register the curr ctb's x pos in sao context*/
                    ps_ctxt->s_sao_ctxt_t.i4_ctb_x = ctb_ctr;

                    /* Register the curr ctb's y pos in sao context*/
                    ps_ctxt->s_sao_ctxt_t.i4_ctb_y = vert_ctr;

                    ps_ctb_out_sao = ps_ctxt->s_sao_ctxt_t.ps_ctb_out +
                                     vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz + ctb_ctr;

                    ps_ctxt->s_sao_ctxt_t.ps_sao = &ps_ctb_out_sao->s_sao;

                    ps_ctxt->s_sao_ctxt_t.i4_sao_blk_wd = ps_ctxt->s_sao_ctxt_t.i4_ctb_size;
                    ps_ctxt->s_sao_ctxt_t.i4_is_last_ctb_col = 0;

                    if((ctb_ctr + 1) == ctb_end)
                    {
                        ps_ctxt->s_sao_ctxt_t.i4_is_last_ctb_col = 1;
                        ps_ctxt->s_sao_ctxt_t.i4_sao_blk_wd =
                            ctb_size - ((ps_tile_params->i4_curr_tile_wd_in_ctb_unit * ctb_size) -
                                        ps_tile_params->i4_curr_tile_width);
                    }

                    ps_ctxt->s_sao_ctxt_t.i4_sao_blk_ht =
                        ctb_size - ((ps_tile_params->i4_curr_tile_ht_in_ctb_unit * ctb_size) -
                                    ps_tile_params->i4_curr_tile_height);

                    ps_ctxt->s_sao_ctxt_t.i4_is_last_ctb_row = 1;

                    /* Calculate the recon buf pointer and stride for teh current ctb */
                    ps_sao_ctxt->pu1_cur_luma_recon_buf =
                        ps_sao_ctxt->pu1_frm_luma_recon_buf +
                        (ps_sao_ctxt->i4_frm_luma_recon_stride * ps_sao_ctxt->i4_ctb_y * ctb_size) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_luma_recon_stride = ps_sao_ctxt->i4_frm_luma_recon_stride;

                    ps_sao_ctxt->pu1_cur_chroma_recon_buf =
                        ps_sao_ctxt->pu1_frm_chroma_recon_buf +
                        (ps_sao_ctxt->i4_frm_chroma_recon_stride * ps_sao_ctxt->i4_ctb_y *
                         (ctb_size >> (ps_ctxt->u1_chroma_array_type == 1))) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_chroma_recon_stride =
                        ps_sao_ctxt->i4_frm_chroma_recon_stride;

                    ps_sao_ctxt->pu1_cur_luma_src_buf =
                        ps_sao_ctxt->pu1_frm_luma_src_buf +
                        (ps_sao_ctxt->i4_frm_luma_src_stride * ps_sao_ctxt->i4_ctb_y * ctb_size) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_luma_src_stride = ps_sao_ctxt->i4_frm_luma_src_stride;

                    ps_sao_ctxt->pu1_cur_chroma_src_buf =
                        ps_sao_ctxt->pu1_frm_chroma_src_buf +
                        (ps_sao_ctxt->i4_frm_chroma_src_stride * ps_sao_ctxt->i4_ctb_y *
                         (ctb_size >> (ps_ctxt->u1_chroma_array_type == 1))) +
                        (ps_sao_ctxt->i4_ctb_x * ctb_size);

                    ps_sao_ctxt->i4_cur_chroma_src_stride = ps_sao_ctxt->i4_frm_chroma_src_stride;

                    /* Calculate the pointer to buff to store the (x,y)th sao
                    * for the top merge of (x,y+1)th ctb
                    */
                    ps_sao_ctxt->ps_top_ctb_sao =
                        &ps_sao_ctxt->aps_frm_top_ctb_sao[ps_ctxt->i4_enc_frm_id]
                                                         [ps_sao_ctxt->i4_ctb_x +
                                                          (ps_sao_ctxt->i4_ctb_y) *
                                                              ps_frm_ctb_prms->i4_num_ctbs_horz +
                                                          (ps_ctxt->i4_bitrate_instance_num *
                                                           ps_sao_ctxt->i4_num_ctb_units)];

                    /* Calculate the pointer to buff to store the top pixels of curr ctb*/
                    ps_sao_ctxt->pu1_curr_sao_src_top_luma =
                        ps_sao_ctxt->apu1_sao_src_frm_top_luma[ps_ctxt->i4_enc_frm_id] +
                        (ps_sao_ctxt->i4_ctb_y - 1) * ps_sao_ctxt->i4_frm_top_luma_buf_stride +
                        ps_sao_ctxt->i4_ctb_x * ctb_size +
                        ps_ctxt->i4_bitrate_instance_num * (ps_sao_ctxt->i4_top_luma_buf_size +
                                                            ps_sao_ctxt->i4_top_chroma_buf_size);

                    /* Calculate the pointer to buff to store the top pixels of curr ctb*/
                    ps_sao_ctxt->pu1_curr_sao_src_top_chroma =
                        ps_sao_ctxt->apu1_sao_src_frm_top_chroma[ps_ctxt->i4_enc_frm_id] +
                        (ps_sao_ctxt->i4_ctb_y - 1) * ps_sao_ctxt->i4_frm_top_chroma_buf_stride +
                        ps_sao_ctxt->i4_ctb_x * ctb_size +
                        ps_ctxt->i4_bitrate_instance_num * (ps_sao_ctxt->i4_top_luma_buf_size +
                                                            ps_sao_ctxt->i4_top_chroma_buf_size);

                    {
                        UWORD32 u4_ctb_sao_bits;
                        ihevce_sao_analyse(
                            &ps_ctxt->s_sao_ctxt_t,
                            ps_ctb_out_sao,
                            &u4_ctb_sao_bits,
                            ps_tile_params);
                        ps_ctxt
                            ->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id]
                                                     [ps_ctxt->i4_bitrate_instance_num]
                            ->u4_frame_rdopt_header_bits += u4_ctb_sao_bits;
                        ps_ctxt
                            ->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id]
                                                     [ps_ctxt->i4_bitrate_instance_num]
                            ->u4_frame_rdopt_bits += u4_ctb_sao_bits;
                    }
                    /** Subpel generation not done for non-ref picture **/
                    if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                    {
                        /* Recon Padding */
                        ihevce_recon_padding(
                            ps_pad_interp_recon,
                            ctb_ctr,
                            vert_ctr,
                            ps_frm_ctb_prms,
                            ps_ctxt->ps_func_selector);
                    }
                }
            }  //end of loop over CTBs in current CTB-row
        }

        /* Subpel Plane Generation*/
        for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
        {
            if(ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
               ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
            {
                if(0 != vert_ctr)
                {
                    /** Subpel generation not done for non-ref picture **/
                    if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                    {
                        /* Padding and Subpel Plane Generation */
                        ihevce_pad_interp_recon_ctb(
                            ps_pad_interp_recon,
                            ctb_ctr,
                            vert_ctr - 1,
                            ps_ctxt->i4_quality_preset,
                            ps_frm_ctb_prms,
                            ps_ctxt->ai2_scratch,
                            ps_ctxt->i4_bitrate_instance_num,
                            ps_ctxt->ps_func_selector);
                    }
                }
            }
            else
            {  // SAO Disabled
                if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                {
                    /* Padding and Subpel Plane Generation */
                    ihevce_pad_interp_recon_ctb(
                        ps_pad_interp_recon,
                        ctb_ctr,
                        vert_ctr,
                        ps_ctxt->i4_quality_preset,
                        ps_frm_ctb_prms,
                        ps_ctxt->ai2_scratch,
                        ps_ctxt->i4_bitrate_instance_num,
                        ps_ctxt->ps_func_selector);
                }
            }
        }

        {
            if(!ps_ctxt->i4_bitrate_instance_num)
            {
                if(ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
                   ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
                {
                    /* If SAO is on, then signal completion of previous CTB row */
                    if(0 != vert_ctr)
                    {
                        {
                            WORD32 post_ctb_ctr;

                            for(post_ctb_ctr = ctb_start; post_ctb_ctr < ctb_end; post_ctb_ctr++)
                            {
                                ihevce_dmgr_map_set_sync(
                                    pv_dep_mngr_me_dep_encloop,
                                    post_ctb_ctr,
                                    (vert_ctr - 1),
                                    MAP_CTB_COMPLETE);
                            }
                        }
                    }
                }
                else
                {
                    {
                        WORD32 post_ctb_ctr;

                        for(post_ctb_ctr = ctb_start; post_ctb_ctr < ctb_end; post_ctb_ctr++)
                        {
                            ihevce_dmgr_map_set_sync(
                                pv_dep_mngr_me_dep_encloop,
                                post_ctb_ctr,
                                vert_ctr,
                                MAP_CTB_COMPLETE);
                        }
                    }
                }
            }
        }

        /*process last ctb row*/
        if(ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
           ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
        {
            sao_ctxt_t *ps_sao_ctxt = &ps_ctxt->s_sao_ctxt_t;

            if(vert_ctr ==
               (ps_tile_params->i4_first_ctb_y + ps_tile_params->i4_curr_tile_ht_in_ctb_unit - 1))
            {
                for(ctb_ctr = ctb_start; ctb_ctr < ctb_end; ctb_ctr++)
                {
                    if(ps_ctxt->i4_deblk_pad_hpel_cur_pic)
                    {
                        /* Padding and Subpel Plane Generation */
                        ihevce_pad_interp_recon_ctb(
                            ps_pad_interp_recon,
                            ctb_ctr,
                            vert_ctr,
                            ps_ctxt->i4_quality_preset,
                            ps_frm_ctb_prms,
                            ps_ctxt->ai2_scratch,
                            ps_ctxt->i4_bitrate_instance_num,
                            ps_ctxt->ps_func_selector);
                    }
                }
            }
            /* If SAO is on, then signal completion of the last CTB row of frame */
            {
                if(vert_ctr == (ps_frm_ctb_prms->i4_num_ctbs_vert - 1))
                {
                    if(!ps_ctxt->i4_bitrate_instance_num)
                    {
                        {
                            WORD32 post_ctb_ctr;

                            for(post_ctb_ctr = ctb_start; post_ctb_ctr < ctb_end; post_ctb_ctr++)
                            {
                                ihevce_dmgr_map_set_sync(
                                    pv_dep_mngr_me_dep_encloop,
                                    post_ctb_ctr,
                                    vert_ctr,
                                    MAP_CTB_COMPLETE);
                            }
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
* \if Function name : ihevce_enc_loop_pass \endif
*
* \brief
*    Frame level enc_loop pass function
*
* \param[in] pv_ctxt : pointer to enc_loop module
* \param[in] ps_frm_lamda : Frame level Lambda params
* \param[in] ps_inp  : pointer to input yuv buffer (frame buffer)
* \param[in] ps_ctb_in : pointer CTB structure (output of ME/IPE) (frame buffer)
* \param[out] ps_frm_recon : pointer recon picture structure pointer (frame buffer)
* \param[out] ps_ctb_out : pointer CTB output structure (frame buffer)
* \param[out] ps_cu_out : pointer CU output structure (frame buffer)
* \param[out] ps_tu_out : pointer TU output structure (frame buffer)
* \param[out] pi2_frm_coeffs : pointer coeff output frame buffer)
*
* \return
*    None
*
* Note : Currently the frame level calcualtions done assumes that
*        framewidth of the input /recon are excat multiple of ctbsize
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_process(
    void *pv_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    ctb_analyse_t *ps_ctb_in,
    ipe_l0_ctb_analyse_for_me_t *ps_ipe_analyse,
    recon_pic_buf_t *ps_frm_recon,
    cur_ctb_cu_tree_t *ps_cu_tree_out,
    ctb_enc_loop_out_t *ps_ctb_out,
    cu_enc_loop_out_t *ps_cu_out,
    tu_enc_loop_out_t *ps_tu_out,
    pu_t *ps_pu_out,
    UWORD8 *pu1_frm_ecd_data,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    frm_lambda_ctxt_t *ps_frm_lamda,
    multi_thrd_ctxt_t *ps_multi_thrd_ctxt,
    WORD32 thrd_id,
    WORD32 i4_enc_frm_id,
    WORD32 i4_pass)
{
    WORD32 vert_ctr;
    WORD32 tile_col_idx;
    iv_enc_yuv_buf_t s_curr_src_bufs;
    iv_enc_yuv_buf_t s_curr_recon_bufs;
    iv_enc_yuv_buf_src_t s_curr_recon_bufs_src;
    UWORD32 *pu4_pu_offsets;
    WORD32 end_of_frame;
    UWORD8 *apu1_y_sub_pel_planes[3];
    pad_interp_recon_frm_t s_pad_interp_recon;
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_ctxt;

    ihevce_enc_loop_ctxt_t *ps_ctxt = ps_master_ctxt->aps_enc_loop_thrd_ctxt[thrd_id];

    WORD32 i4_bitrate_instance_num = ps_ctxt->i4_bitrate_instance_num;

    /* initialize the closed loop lambda for the current frame */
    ps_ctxt->i8_cl_ssd_lambda_qf = ps_frm_lamda->i8_cl_ssd_lambda_qf;
    ps_ctxt->i8_cl_ssd_lambda_chroma_qf = ps_frm_lamda->i8_cl_ssd_lambda_chroma_qf;
    ps_ctxt->u4_chroma_cost_weighing_factor = ps_frm_lamda->u4_chroma_cost_weighing_factor;
    ps_ctxt->i4_satd_lamda = ps_frm_lamda->i4_cl_satd_lambda_qf;
    ps_ctxt->i4_sad_lamda = ps_frm_lamda->i4_cl_sad_type2_lambda_qf;
    ps_ctxt->thrd_id = thrd_id;
    ps_ctxt->u1_is_refPic = ps_curr_inp->s_lap_out.i4_is_ref_pic;

#if DISABLE_SAO_WHEN_NOISY
    ps_ctxt->s_sao_ctxt_t.ps_ctb_data = ps_ctb_in;
    ps_ctxt->s_sao_ctxt_t.i4_ctb_data_stride = ps_frm_ctb_prms->i4_num_ctbs_horz;
#endif

#if ENABLE_TU_TREE_DETERMINATION_IN_RDOPT
    ps_ctxt->pv_err_func_selector = ps_func_selector;
#endif

    /*Bit0 -  of this Flag indicates whether current pictute needs to be deblocked,
    padded and hpel planes need to be generated.
    Bit1 - of this flag set to 1 if sao is enabled. This is to enable deblocking when sao is enabled*/
    ps_ctxt->i4_deblk_pad_hpel_cur_pic =
        (ps_frm_recon->i4_deblk_pad_hpel_cur_pic) ||
        ((ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_luma_flag ||
          ps_ctxt->s_sao_ctxt_t.ps_slice_hdr->i1_slice_sao_chroma_flag)
         << 1);

    /* Share all reference pictures with nbr clients. This flag will be used only
    in case of dist-enc mode */
    ps_ctxt->i4_share_flag = (ps_frm_recon->i4_is_reference != 0);
    ps_ctxt->pv_frm_recon = (void *)ps_frm_recon;

    /* Register the frame level ssd lamda for both luma and chroma*/
    ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_qf = ps_frm_lamda->i8_cl_ssd_lambda_qf;
    ps_ctxt->s_sao_ctxt_t.i8_cl_ssd_lambda_chroma_qf = ps_frm_lamda->i8_cl_ssd_lambda_chroma_qf;

    ihevce_populate_cl_cu_lambda_prms(
        ps_ctxt,
        ps_frm_lamda,
        (WORD32)ps_ctxt->i1_slice_type,
        ps_curr_inp->s_lap_out.i4_temporal_lyr_id,
        ENC_LOOP_LAMBDA_TYPE);

    ps_ctxt->u1_disable_intra_eval = DISABLE_INTRA_IN_BPICS &&
                                     (IHEVCE_QUALITY_P6 == ps_ctxt->i4_quality_preset) &&
                                     (ps_ctxt->i4_temporal_layer_id > TEMPORAL_LAYER_DISABLE);

    end_of_frame = 0;

    /* ----------------------------------------------------- */
    /* store the stride and dimensions of source and recon   */
    /* buffer pointers will be over written at every CTB row */
    /* ----------------------------------------------------- */
    memcpy(&s_curr_src_bufs, &ps_curr_inp->s_lap_out.s_input_buf, sizeof(iv_enc_yuv_buf_t));

    memcpy(&s_curr_recon_bufs, &ps_frm_recon->s_yuv_buf_desc, sizeof(iv_enc_yuv_buf_t));

    memcpy(&s_curr_recon_bufs_src, &ps_frm_recon->s_yuv_buf_desc_src, sizeof(iv_enc_yuv_buf_src_t));

    /* get the frame level pu offset pointer*/
    pu4_pu_offsets = ps_frm_recon->pu4_pu_off;

    s_pad_interp_recon.u1_chroma_array_type = ps_ctxt->u1_chroma_array_type;

    /* ------------ Loop over all the CTB rows --------------- */
    while(0 == end_of_frame)
    {
        UWORD8 *pu1_tmp;
        UWORD8 *pu1_row_pu_map;
        UWORD8 *pu1_row_ecd_data;
        ctb_analyse_t *ps_ctb_row_in;
        ctb_enc_loop_out_t *ps_ctb_row_out;
        cu_enc_loop_out_t *ps_row_cu;
        tu_enc_loop_out_t *ps_row_tu;
        pu_t *ps_row_pu;
        pu_col_mv_t *ps_row_col_pu;
        job_queue_t *ps_job;
        UWORD32 *pu4_pu_row_offsets;
        UWORD16 *pu2_num_pu_row;

        ipe_l0_ctb_analyse_for_me_t *ps_row_ipe_analyse;
        cur_ctb_cu_tree_t *ps_row_cu_tree;
        UWORD8 is_inp_422 = (ps_ctxt->u1_chroma_array_type == 2);

        /* Get the current row from the job queue */
        ps_job = (job_queue_t *)ihevce_enc_grp_get_next_job(
            ps_multi_thrd_ctxt, ENC_LOOP_JOB + i4_bitrate_instance_num, 1, i4_enc_frm_id);

        /* Register the pointer to ctb out of the current frame*/
        ps_ctxt->s_sao_ctxt_t.ps_ctb_out = ps_ctb_out;

        /* If all rows are done, set the end of process flag to 1, */
        /* and the current row to -1 */
        if(NULL == ps_job)
        {
            vert_ctr = -1;
            tile_col_idx = -1;
            end_of_frame = 1;
        }
        else
        {
            ihevce_tile_params_t *ps_col_tile_params_temp;
            ihevce_tile_params_t *ps_tile_params;
            WORD32 i4_tile_id;

            ASSERT((ENC_LOOP_JOB + i4_bitrate_instance_num) == ps_job->i4_task_type);
            /* set the output dependency */
            ihevce_enc_grp_job_set_out_dep(ps_multi_thrd_ctxt, ps_job, i4_enc_frm_id);

            /* Obtain the current row's details from the job */
            vert_ctr = ps_job->s_job_info.s_enc_loop_job_info.i4_ctb_row_no;
            {
                /* Obtain the current colum tile index from the job */
                tile_col_idx = ps_job->s_job_info.s_enc_loop_job_info.i4_tile_col_idx;

                /* The tile parameter for the col. idx. Use only the properties
                which is same for all the bottom tiles like width, start_x, etc.
                Don't use height, start_y, etc.                                  */
                ps_col_tile_params_temp =
                    ((ihevce_tile_params_t *)ps_master_ctxt->pv_tile_params_base + tile_col_idx);

                /* Derive actual tile_id based on vert_ctr */
                i4_tile_id =
                    *(ps_frm_ctb_prms->pi4_tile_id_map +
                      vert_ctr * ps_frm_ctb_prms->i4_tile_id_ctb_map_stride +
                      ps_col_tile_params_temp->i4_first_ctb_x);
                /* Derive pointer to current tile prms */
                ps_tile_params =
                    ((ihevce_tile_params_t *)ps_master_ctxt->pv_tile_params_base + i4_tile_id);
            }

            ps_ctxt->i4_tile_col_idx = tile_col_idx;
            /* derive the current ctb row pointers */

            /* luma src */
            pu1_tmp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_y_buf +
                      (ps_curr_inp->s_lap_out.s_input_buf.i4_start_offset_y *
                       ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd) +
                      ps_curr_inp->s_lap_out.s_input_buf.i4_start_offset_x;

            pu1_tmp +=
                (vert_ctr * ps_frm_ctb_prms->i4_ctb_size *
                 ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd);

            s_curr_src_bufs.pv_y_buf = pu1_tmp;

            if(!ps_ctxt->u1_is_input_data_hbd)
            {
                /* cb src */
                pu1_tmp = (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_u_buf;
                pu1_tmp +=
                    (vert_ctr * (ps_frm_ctb_prms->i4_ctb_size >> ((is_inp_422 == 1) ? 0 : 1)) *
                     ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd);

                s_curr_src_bufs.pv_u_buf = pu1_tmp;
            }

            /* luma recon */
            pu1_tmp = (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_y_buf;
            pu1_tmp +=
                (vert_ctr * ps_frm_ctb_prms->i4_ctb_size * ps_frm_recon->s_yuv_buf_desc.i4_y_strd);

            s_curr_recon_bufs.pv_y_buf = pu1_tmp;
            s_pad_interp_recon.pu1_luma_recon = (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_y_buf;
            s_pad_interp_recon.i4_luma_recon_stride = ps_frm_recon->s_yuv_buf_desc.i4_y_strd;
            if(!ps_ctxt->u1_is_input_data_hbd)
            {
                /* cb recon */
                pu1_tmp = (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_u_buf;
                pu1_tmp +=
                    (vert_ctr * (ps_frm_ctb_prms->i4_ctb_size >> ((is_inp_422 == 1) ? 0 : 1)) *
                     ps_frm_recon->s_yuv_buf_desc.i4_uv_strd);

                s_curr_recon_bufs.pv_u_buf = pu1_tmp;
                s_pad_interp_recon.pu1_chrm_recon = (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_u_buf;
                s_pad_interp_recon.i4_chrm_recon_stride = ps_frm_recon->s_yuv_buf_desc.i4_uv_strd;

                s_pad_interp_recon.i4_ctb_size = ps_frm_ctb_prms->i4_ctb_size;

                /* Register the source buffer pointers in sao context*/
                ps_ctxt->s_sao_ctxt_t.pu1_frm_luma_src_buf =
                    (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_y_buf +
                    (ps_curr_inp->s_lap_out.s_input_buf.i4_start_offset_y *
                     ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd) +
                    ps_curr_inp->s_lap_out.s_input_buf.i4_start_offset_x;

                ps_ctxt->s_sao_ctxt_t.i4_frm_luma_src_stride =
                    ps_curr_inp->s_lap_out.s_input_buf.i4_y_strd;

                ps_ctxt->s_sao_ctxt_t.pu1_frm_chroma_src_buf =
                    (UWORD8 *)ps_curr_inp->s_lap_out.s_input_buf.pv_u_buf;

                ps_ctxt->s_sao_ctxt_t.i4_frm_chroma_src_stride =
                    ps_curr_inp->s_lap_out.s_input_buf.i4_uv_strd;
            }

            /* Subpel planes hxfy, fxhy, hxhy*/
            pu1_tmp = ps_frm_recon->apu1_y_sub_pel_planes[0];
            pu1_tmp +=
                (vert_ctr * ps_frm_ctb_prms->i4_ctb_size * ps_frm_recon->s_yuv_buf_desc.i4_y_strd);
            apu1_y_sub_pel_planes[0] = pu1_tmp;
            s_pad_interp_recon.pu1_sbpel_hxfy = ps_frm_recon->apu1_y_sub_pel_planes[0];

            pu1_tmp = ps_frm_recon->apu1_y_sub_pel_planes[1];
            pu1_tmp +=
                (vert_ctr * ps_frm_ctb_prms->i4_ctb_size * ps_frm_recon->s_yuv_buf_desc.i4_y_strd);
            apu1_y_sub_pel_planes[1] = pu1_tmp;
            s_pad_interp_recon.pu1_sbpel_fxhy = ps_frm_recon->apu1_y_sub_pel_planes[1];

            pu1_tmp = ps_frm_recon->apu1_y_sub_pel_planes[2];
            pu1_tmp +=
                (vert_ctr * ps_frm_ctb_prms->i4_ctb_size * ps_frm_recon->s_yuv_buf_desc.i4_y_strd);
            apu1_y_sub_pel_planes[2] = pu1_tmp;
            s_pad_interp_recon.pu1_sbpel_hxhy = ps_frm_recon->apu1_y_sub_pel_planes[2];

            /* row level coeffs buffer */
            pu1_row_ecd_data =
                pu1_frm_ecd_data +
                (vert_ctr *
                 ((is_inp_422 == 1) ? (ps_frm_ctb_prms->i4_max_tus_in_row << 1)
                                    : ((ps_frm_ctb_prms->i4_max_tus_in_row * 3) >> 1)) *
                 MAX_SCAN_COEFFS_BYTES_4x4);

            /* Row level CU buffer */
            ps_row_cu = ps_cu_out + (vert_ctr * ps_frm_ctb_prms->i4_max_cus_in_row);

            /* Row level TU buffer */
            ps_row_tu = ps_tu_out + (vert_ctr * ps_frm_ctb_prms->i4_max_tus_in_row);

            /* Row level PU buffer */
            ps_row_pu = ps_pu_out + (vert_ctr * ps_frm_ctb_prms->i4_max_pus_in_row);

            /* Row level colocated PU buffer */
            /* ps_frm_col_mv has (i4_num_ctbs_horz + 1) CTBs for stride */
            ps_row_col_pu =
                ps_frm_recon->ps_frm_col_mv + (vert_ctr * (ps_frm_ctb_prms->i4_num_ctbs_horz + 1) *
                                               ps_frm_ctb_prms->i4_num_pus_in_ctb);
            /* Row level col PU map buffer */
            /* pu1_frm_pu_map has (i4_num_ctbs_horz + 1) CTBs for stride */
            pu1_row_pu_map =
                ps_frm_recon->pu1_frm_pu_map + (vert_ctr * (ps_frm_ctb_prms->i4_num_ctbs_horz + 1) *
                                                ps_frm_ctb_prms->i4_num_pus_in_ctb);
            /* row ctb in pointer  */
            ps_ctb_row_in = ps_ctb_in + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* row ctb out pointer  */
            ps_ctb_row_out = ps_ctb_out + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* row number of PUs map pointer */
            pu2_num_pu_row =
                ps_frm_recon->pu2_num_pu_map + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;

            /* row pu offsets pointer  */
            pu4_pu_row_offsets = pu4_pu_offsets + vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz;
            /* store the first CTB pu offset pointer */
            *pu4_pu_row_offsets = vert_ctr * ps_frm_ctb_prms->i4_max_pus_in_row;
            /* Initialize ptr to current IPE row */
            ps_row_ipe_analyse = ps_ipe_analyse + (vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz);

            /* Initialize ptr to current row */
            ps_row_cu_tree = ps_cu_tree_out +
                             (vert_ctr * ps_frm_ctb_prms->i4_num_ctbs_horz * MAX_NUM_NODES_CU_TREE);

            /* Get the EncLoop Top-Right CU Dep Mngr */
            ps_ctxt->pv_dep_mngr_enc_loop_cu_top_right =
                ps_master_ctxt->aapv_dep_mngr_enc_loop_cu_top_right[ps_ctxt->i4_enc_frm_id]
                                                                   [i4_bitrate_instance_num];
            /* Get the EncLoop Deblock Dep Mngr */
            ps_ctxt->pv_dep_mngr_enc_loop_dblk =
                ps_master_ctxt
                    ->aapv_dep_mngr_enc_loop_dblk[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num];
            /* Get the EncLoop Sao Dep Mngr */
            ps_ctxt->pv_dep_mngr_enc_loop_sao =
                ps_master_ctxt
                    ->aapv_dep_mngr_enc_loop_sao[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num];

            ps_ctxt->pu1_curr_row_cabac_state = &ps_master_ctxt->au1_ctxt_models[vert_ctr][0];

            {
                /* derive the pointers of top row buffers */
                ps_ctxt->pv_top_row_luma =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_luma[ps_ctxt->i4_enc_frm_id] +
                    (ps_ctxt->i4_frm_top_row_luma_size * ps_ctxt->i4_bitrate_instance_num) +
                    (vert_ctr - 1) * ps_ctxt->i4_top_row_luma_stride;

                ps_ctxt->pv_top_row_chroma =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_chroma[ps_ctxt->i4_enc_frm_id] +
                    (ps_ctxt->i4_frm_top_row_chroma_size * ps_ctxt->i4_bitrate_instance_num) +
                    (vert_ctr - 1) * ps_ctxt->i4_top_row_chroma_stride;

                /* derive the pointers of bottom row buffers to update current row data */
                ps_ctxt->pv_bot_row_luma =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_luma[ps_ctxt->i4_enc_frm_id] +
                    (ps_ctxt->i4_frm_top_row_luma_size * ps_ctxt->i4_bitrate_instance_num) +
                    (vert_ctr)*ps_ctxt->i4_top_row_luma_stride;

                ps_ctxt->pv_bot_row_chroma =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_chroma[ps_ctxt->i4_enc_frm_id] +
                    (ps_ctxt->i4_frm_top_row_chroma_size * ps_ctxt->i4_bitrate_instance_num) +
                    (vert_ctr)*ps_ctxt->i4_top_row_chroma_stride;

                /* Register the buffer pointers in sao context*/
                ps_ctxt->s_sao_ctxt_t.pu1_frm_luma_recon_buf =
                    (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_y_buf;
                ps_ctxt->s_sao_ctxt_t.i4_frm_luma_recon_stride =
                    ps_frm_recon->s_yuv_buf_desc.i4_y_strd;

                ps_ctxt->s_sao_ctxt_t.pu1_frm_chroma_recon_buf =
                    (UWORD8 *)ps_frm_recon->s_yuv_buf_desc.pv_u_buf;
                ps_ctxt->s_sao_ctxt_t.i4_frm_chroma_recon_stride =
                    ps_frm_recon->s_yuv_buf_desc.i4_uv_strd;

                ps_ctxt->s_sao_ctxt_t.ps_rdopt_entropy_ctxt = &ps_ctxt->s_rdopt_entropy_ctxt;

                ps_ctxt->s_sao_ctxt_t.i4_frm_top_luma_buf_stride =
                    ps_ctxt->s_sao_ctxt_t.u4_ctb_aligned_wd + 1;

                ps_ctxt->s_sao_ctxt_t.i4_frm_top_chroma_buf_stride =
                    ps_ctxt->s_sao_ctxt_t.u4_ctb_aligned_wd + 2;
            }

            ps_ctxt->ps_top_row_nbr =
                ps_ctxt->aps_frm_top_row_nbr[ps_ctxt->i4_enc_frm_id] +
                (ps_ctxt->i4_frm_top_row_nbr_size * ps_ctxt->i4_bitrate_instance_num) +
                (vert_ctr - 1) * ps_ctxt->i4_top_row_nbr_stride;

            ps_ctxt->ps_bot_row_nbr =
                ps_ctxt->aps_frm_top_row_nbr[ps_ctxt->i4_enc_frm_id] +
                (ps_ctxt->i4_frm_top_row_nbr_size * ps_ctxt->i4_bitrate_instance_num) +
                (vert_ctr)*ps_ctxt->i4_top_row_nbr_stride;

            if(vert_ctr > 0)
            {
                ps_ctxt->pu1_top_rt_cabac_state = &ps_master_ctxt->au1_ctxt_models[vert_ctr - 1][0];
            }
            else
            {
                ps_ctxt->pu1_top_rt_cabac_state = NULL;
            }

            ASSERT(
                ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[0]
                    .ps_pps->i1_sign_data_hiding_flag ==
                ps_ctxt->s_rdopt_entropy_ctxt.as_cu_entropy_ctxt[1]
                    .ps_pps->i1_sign_data_hiding_flag);

            /* call the row level processing function */
            ihevce_enc_loop_process_row(
                ps_ctxt,
                &s_curr_src_bufs,
                &s_curr_recon_bufs,
                &s_curr_recon_bufs_src,
                &apu1_y_sub_pel_planes[0],
                ps_ctb_row_in,
                ps_ctb_row_out,
                ps_row_ipe_analyse,
                ps_row_cu_tree,
                ps_row_cu,
                ps_row_tu,
                ps_row_pu,
                ps_row_col_pu,
                pu2_num_pu_row,
                pu1_row_pu_map,
                pu1_row_ecd_data,
                pu4_pu_row_offsets,
                ps_frm_ctb_prms,
                vert_ctr,
                ps_frm_recon,
                ps_ctxt->pv_dep_mngr_encloop_dep_me,
                &s_pad_interp_recon,
                i4_pass,
                ps_multi_thrd_ctxt,
                ps_tile_params);
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_dblk_get_prms_dep_mngr \endif
*
* \brief Returns to the caller key attributes relevant for dependency manager,
*        ie, the number of vertical units in l0 layer
*
* \par Description:
*
* \param[in] pai4_ht    : ht
* \param[out] pi4_num_vert_units_in_lyr : Pointer to store num vertical units
*                                         for deblocking
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_dblk_get_prms_dep_mngr(WORD32 i4_ht, WORD32 *pi4_num_vert_units_in_lyr)
{
    /* Blk ht at a given layer*/
    WORD32 unit_ht_c;
    WORD32 ctb_size = 64;

    /* compute blk ht and unit ht */
    unit_ht_c = ctb_size;

    /* set the numebr of vertical units */
    *pi4_num_vert_units_in_lyr = (i4_ht + unit_ht_c - 1) / unit_ht_c;
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for enc_loop module
* Note : Include TOT MEM. req. for ENC.LOOP + TOT MEM. req. for Dep Mngr for Dblk
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
    ihevce_enc_loop_get_num_mem_recs(WORD32 i4_num_bitrate_inst, WORD32 i4_num_enc_loop_frm_pllel)
{
    WORD32 enc_loop_mem_recs = NUM_ENC_LOOP_MEM_RECS;
    WORD32 enc_loop_dblk_dep_mngr_mem_recs =
        i4_num_enc_loop_frm_pllel * i4_num_bitrate_inst * ihevce_dmgr_get_num_mem_recs();
    WORD32 enc_loop_sao_dep_mngr_mem_recs =
        i4_num_enc_loop_frm_pllel * i4_num_bitrate_inst * ihevce_dmgr_get_num_mem_recs();
    WORD32 enc_loop_cu_top_right_dep_mngr_mem_recs =
        i4_num_enc_loop_frm_pllel * i4_num_bitrate_inst * ihevce_dmgr_get_num_mem_recs();
    WORD32 enc_loop_aux_br_dep_mngr_mem_recs =
        i4_num_enc_loop_frm_pllel * (i4_num_bitrate_inst - 1) * ihevce_dmgr_get_num_mem_recs();

    return (
        (enc_loop_mem_recs + enc_loop_dblk_dep_mngr_mem_recs + enc_loop_sao_dep_mngr_mem_recs +
         enc_loop_cu_top_right_dep_mngr_mem_recs + enc_loop_aux_br_dep_mngr_mem_recs));
}
/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for ENC_LOOP.
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
WORD32 ihevce_enc_loop_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    WORD32 i4_num_bitrate_inst,
    WORD32 i4_num_enc_loop_frm_pllel,
    WORD32 i4_mem_space,
    WORD32 i4_resolution_id)
{
    UWORD32 u4_width, u4_height, n_tabs;
    UWORD32 u4_ctb_in_a_row, u4_ctb_rows_in_a_frame;
    WORD32 ctr;
    WORD32 i4_chroma_format = ps_init_prms->s_src_prms.i4_chr_format;

    /* derive frame dimensions */
    /*width of the input YUV to be encoded */
    u4_width = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
    /*making the width a multiple of CTB size*/
    u4_width += SET_CTB_ALIGN(
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, MAX_CTB_SIZE);

    /*height of the input YUV to be encoded */
    u4_height = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
    /*making the height a multiple of CTB size*/
    u4_height += SET_CTB_ALIGN(
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, MAX_CTB_SIZE);
    u4_ctb_in_a_row = (u4_width / MAX_CTB_SIZE);
    u4_ctb_rows_in_a_frame = (u4_height / MAX_CTB_SIZE);
    /* memories should be requested assuming worst case requirememnts */

    /* Module context structure */
    ps_mem_tab[ENC_LOOP_CTXT].i4_mem_size = sizeof(ihevce_enc_loop_master_ctxt_t);

    ps_mem_tab[ENC_LOOP_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_CTXT].i4_mem_alignment = 8;

    /* Thread context structure */
    ps_mem_tab[ENC_LOOP_THRDS_CTXT].i4_mem_size =
        i4_num_proc_thrds * sizeof(ihevce_enc_loop_ctxt_t);

    ps_mem_tab[ENC_LOOP_THRDS_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_THRDS_CTXT].i4_mem_alignment = 16;

    /* Scale matrices */
    ps_mem_tab[ENC_LOOP_SCALE_MAT].i4_mem_size = 2 * MAX_TU_SIZE * MAX_TU_SIZE * sizeof(WORD16);

    ps_mem_tab[ENC_LOOP_SCALE_MAT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_SCALE_MAT].i4_mem_alignment = 8;

    /* Rescale matrices */
    ps_mem_tab[ENC_LOOP_RESCALE_MAT].i4_mem_size = 2 * MAX_TU_SIZE * MAX_TU_SIZE * sizeof(WORD16);

    ps_mem_tab[ENC_LOOP_RESCALE_MAT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_RESCALE_MAT].i4_mem_alignment = 8;

    /* top row luma one row of pixel data per CTB row */
    if(ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8)
    {
        ps_mem_tab[ENC_LOOP_TOP_LUMA].i4_mem_size = (u4_ctb_rows_in_a_frame + 1) *
                                                    (u4_width + MAX_CU_SIZE + 1) * sizeof(UWORD16) *
                                                    i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel;
    }
    else
    {
        ps_mem_tab[ENC_LOOP_TOP_LUMA].i4_mem_size = (u4_ctb_rows_in_a_frame + 1) *
                                                    (u4_width + MAX_CU_SIZE + 1) * sizeof(UWORD8) *
                                                    i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel;
    }

    ps_mem_tab[ENC_LOOP_TOP_LUMA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_TOP_LUMA].i4_mem_alignment = 8;

    /* top row chroma */
    if(ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8)
    {
        ps_mem_tab[ENC_LOOP_TOP_CHROMA].i4_mem_size =
            (u4_ctb_rows_in_a_frame + 1) * (u4_width + MAX_CU_SIZE + 2) * sizeof(UWORD16) *
            i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel;
    }
    else
    {
        ps_mem_tab[ENC_LOOP_TOP_CHROMA].i4_mem_size =
            (u4_ctb_rows_in_a_frame + 1) * (u4_width + MAX_CU_SIZE + 2) * sizeof(UWORD8) *
            i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel;
    }

    ps_mem_tab[ENC_LOOP_TOP_CHROMA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_TOP_CHROMA].i4_mem_alignment = 8;

    /* top row neighbour 4x4 */
    ps_mem_tab[ENC_LOOP_TOP_NBR4X4].i4_mem_size =
        (u4_ctb_rows_in_a_frame + 1) * (((u4_width + MAX_CU_SIZE) >> 2) + 1) * sizeof(nbr_4x4_t) *
        i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel;

    ps_mem_tab[ENC_LOOP_TOP_NBR4X4].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_TOP_NBR4X4].i4_mem_alignment = 8;

    /* memory to dump rate control parameters by each thread for each bit-rate instance */
    /* RC params collated by each thread for each bit-rate instance separately */
    ps_mem_tab[ENC_LOOP_RC_PARAMS].i4_mem_size = i4_num_bitrate_inst * i4_num_enc_loop_frm_pllel *
                                                 i4_num_proc_thrds * sizeof(enc_loop_rc_params_t);

    ps_mem_tab[ENC_LOOP_RC_PARAMS].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_RC_PARAMS].i4_mem_alignment = 8;
    /* Memory required for deblocking */
    {
        /* Memory to store Qp of top4x4 blocks for each CTB row.
        This memory is allocated at frame level and shared across
        all cores. The Qp values are needed to form Qp-map(described
        in the ENC_LOOP_DEBLOCKING section below)*/

        UWORD32 u4_size_bs_memory, u4_size_qp_memory;
        UWORD32 u4_size_top_4x4_qp_memory;

        /*Memory required to store Qp of top4x4 blocks for a CTB row for entire frame*/
        /*Space required per CTB*/
        u4_size_top_4x4_qp_memory = (MAX_CTB_SIZE / 4);
        /*Space required for entire CTB row*/
        u4_size_top_4x4_qp_memory *= u4_ctb_in_a_row;
        /*Space required for entire frame*/
        u4_size_top_4x4_qp_memory *= u4_ctb_rows_in_a_frame;
        /*Space required for multiple bitrate*/
        u4_size_top_4x4_qp_memory *= i4_num_bitrate_inst;
        /*Space required for multiple frames in parallel*/
        u4_size_top_4x4_qp_memory *= i4_num_enc_loop_frm_pllel;

        ps_mem_tab[ENC_LOOP_QP_TOP_4X4].i4_mem_size = u4_size_top_4x4_qp_memory;
        ps_mem_tab[ENC_LOOP_QP_TOP_4X4].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
        ps_mem_tab[ENC_LOOP_QP_TOP_4X4].i4_mem_alignment = 8;

        /* Memory allocation of BS and Qp-map for deblocking at CTB-row level:
        ## Boundary Strength(Vertical):
        BS stored per CTB at one stretch i.e. for a 64x CTB first 8 entries belongs to first CTB
        of the row followed by 8 entries of second CTB and so on.
        8 entries: Includes left edge of current CTB and excludes right edge.
        ## Boundary Strength(Horizontal):
        Same as Vertical.
        8 entries:  Includes top edge of current CTB and excludes bottom edge.

        ## Qp-map storage:
        T0 T1 T2 T3 T4 T5 ..........to the end of the CTB row
        00 01 02 03 04 05 ..........to the end of the CTB row
        10 11 12 13 14 15 ..........to the end of the CTB row
        20 21 22 23 24 25 ..........to the end of the CTB row
        30 31 32 33 34 35 ..........to the end of the CTB row
        40 41 42 43 44 45 ..........to the end of the CTB row
        ............................to the end of the CTB row
        upto height_of_CTB..........to the end of the CTB row

        Qp is stored for each "4x4 block" in a proper 2-D array format (One entry for each 4x4).
        A 2-D array of height= (height_of_CTB +1), and width = (width_of_CTB).
        where,
        => height_of_CTB = number of 4x4 blocks in a CTB  vertically,
        => +1 is done to store Qp of lowest 4x4-block layer of top-CTB
        in order to deblock top edge of current CTB.
        => width_of_CTB  = number of 4x4 blocks in a CTB  horizontally,
        */

        /*Memory(in bytes) required for storing Boundary Strength for entire CTB row*/
        /*1 vertical edge per 8 pixel*/
        u4_size_bs_memory = (MAX_CTB_SIZE >> 3);
        /*Vertical edges for entire width of CTB row*/
        u4_size_bs_memory *= u4_ctb_in_a_row;
        /*Each vertical edge of CTB row is 4 bytes*/
        u4_size_bs_memory = u4_size_bs_memory << 2;
        /*Adding Memory required for storing horizontal BS by doubling*/
        u4_size_bs_memory = u4_size_bs_memory << 1;

        /*Memory(in bytes) required for storing Qp at 4x4 level for entire CTB row*/
        /*Number of 4x4 blocks in the width of a CTB*/
        u4_size_qp_memory = (MAX_CTB_SIZE >> 2);
        /*Number of 4x4 blocks in the height of a CTB. Adding 1 to store Qp of lowest
        4x4-block layer of top-CTB in order to deblock top edge of current CTB*/
        u4_size_qp_memory *= ((MAX_CTB_SIZE >> 2) + 1);
        /*Storage for entire CTB row*/
        u4_size_qp_memory *= u4_ctb_in_a_row;

        /*Multiplying by i4_num_proc_thrds to assign memory for each core*/
        ps_mem_tab[ENC_LOOP_DEBLOCKING].i4_mem_size =
            i4_num_proc_thrds * (u4_size_bs_memory + u4_size_qp_memory);

        ps_mem_tab[ENC_LOOP_DEBLOCKING].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_DEBLOCKING].i4_mem_alignment = 8;
    }

    /* Memory required to store pred for 422 chroma */
    ps_mem_tab[ENC_LOOP_422_CHROMA_INTRA_PRED].i4_mem_size =
        i4_num_proc_thrds * MAX_CTB_SIZE * MAX_CTB_SIZE * 2 *
        (i4_chroma_format == IV_YUV_422SP_UV) *
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

    ps_mem_tab[ENC_LOOP_422_CHROMA_INTRA_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_422_CHROMA_INTRA_PRED].i4_mem_alignment = 8;

    /* Memory for inter pred buffers */
    {
        WORD32 i4_num_bufs_per_thread = 0;

        WORD32 i4_buf_size_per_cand =
            (MAX_CTB_SIZE) * (MAX_CTB_SIZE) *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);
        WORD32 i4_quality_preset =
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;
        switch(i4_quality_preset)
        {
        case IHEVCE_QUALITY_P0:
        {
            i4_num_bufs_per_thread = MAX_NUM_INTER_CANDS_PQ;
            break;
        }
        case IHEVCE_QUALITY_P2:
        {
            i4_num_bufs_per_thread = MAX_NUM_INTER_CANDS_HQ;
            break;
        }
        case IHEVCE_QUALITY_P3:
        {
            i4_num_bufs_per_thread = MAX_NUM_INTER_CANDS_MS;
            break;
        }
        case IHEVCE_QUALITY_P4:
        {
            i4_num_bufs_per_thread = MAX_NUM_INTER_CANDS_HS;
            break;
        }
        case IHEVCE_QUALITY_P5:
        case IHEVCE_QUALITY_P6:
        case IHEVCE_QUALITY_P7:
        {
            i4_num_bufs_per_thread = MAX_NUM_INTER_CANDS_ES;
            break;
        }
        default:
        {
            ASSERT(0);
        }
        }

        i4_num_bufs_per_thread += 4;

        ps_mem_tab[ENC_LOOP_INTER_PRED].i4_mem_size =
            i4_num_bufs_per_thread * i4_num_proc_thrds * i4_buf_size_per_cand;

        ps_mem_tab[ENC_LOOP_INTER_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_INTER_PRED].i4_mem_alignment = 8;
    }

    /* Memory required to store chroma intra pred */
    ps_mem_tab[ENC_LOOP_CHROMA_PRED_INTRA].i4_mem_size =
        i4_num_proc_thrds * (MAX_TU_SIZE) * (MAX_TU_SIZE)*2 * NUM_POSSIBLE_TU_SIZES_CHR_INTRA_SATD *
        ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1) *
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

    ps_mem_tab[ENC_LOOP_CHROMA_PRED_INTRA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_CHROMA_PRED_INTRA].i4_mem_alignment = 8;

    /* Memory required to store pred for reference substitution output */
    /* While (MAX_TU_SIZE * 2 * 2) + 1 is the actual size needed,
       allocate 16 bytes to the left and 7 bytes to the right to facilitate
       SIMD access */
    ps_mem_tab[ENC_LOOP_REF_SUB_OUT].i4_mem_size =
        i4_num_proc_thrds * (((MAX_TU_SIZE * 2 * 2) + INTRAPRED_SIMD_RIGHT_PADDING)
        + INTRAPRED_SIMD_LEFT_PADDING)*
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

    ps_mem_tab[ENC_LOOP_REF_SUB_OUT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_REF_SUB_OUT].i4_mem_alignment = 8;

    /* Memory required to store pred for reference filtering output */
    /* While (MAX_TU_SIZE * 2 * 2) + 1 is the actual size needed,
       allocate 16 bytes to the left and 7 bytes to the right to facilitate
       SIMD access */
    ps_mem_tab[ENC_LOOP_REF_FILT_OUT].i4_mem_size =
        i4_num_proc_thrds * (((MAX_TU_SIZE * 2 * 2) + INTRAPRED_SIMD_RIGHT_PADDING)
        + INTRAPRED_SIMD_LEFT_PADDING)*
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

    ps_mem_tab[ENC_LOOP_REF_FILT_OUT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_REF_FILT_OUT].i4_mem_alignment = 8;

#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    if(ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset == 0)
#endif
    {
        /* Memory assignments for recon storage during CU Recursion */
        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].i4_mem_size =
            i4_num_proc_thrds * (MAX_CU_SIZE * MAX_CU_SIZE) *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].i4_mem_alignment = 8;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].i4_mem_size =
            i4_num_proc_thrds * (MAX_CU_SIZE * (MAX_CU_SIZE >> 1)) *
            ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1) *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].i4_mem_alignment = 8;
    }
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    else
    {
        /* Memory assignments for recon storage during CU Recursion */
        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].i4_mem_size = 0;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].i4_mem_alignment = 8;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].i4_mem_size = 0;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].i4_mem_alignment = 8;
    }
#endif

#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    if(ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset == 0)
#endif
    {
        /* Memory assignments for pred storage during CU Recursion */
        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].i4_mem_size =
            i4_num_proc_thrds * (MAX_CU_SIZE * MAX_CU_SIZE) *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].i4_mem_alignment = 8;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].i4_mem_size =
            i4_num_proc_thrds * (MAX_CU_SIZE * (MAX_CU_SIZE >> 1)) *
            ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1) *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].i4_mem_alignment = 8;
    }
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
    else
    {
        /* Memory assignments for pred storage during CU Recursion */
        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].i4_mem_size = 0;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].i4_mem_alignment = 8;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].i4_mem_size = 0;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].i4_mem_alignment = 8;
    }
#endif

    /* Memory assignments for CTB left luma data storage */
    ps_mem_tab[ENC_LOOP_LEFT_LUMA_DATA].i4_mem_size =
        i4_num_proc_thrds * (MAX_CTB_SIZE + MAX_TU_SIZE) *
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

    ps_mem_tab[ENC_LOOP_LEFT_LUMA_DATA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_LEFT_LUMA_DATA].i4_mem_alignment = 8;

    /* Memory assignments for CTB left chroma data storage */
    ps_mem_tab[ENC_LOOP_LEFT_CHROMA_DATA].i4_mem_size =
        i4_num_proc_thrds * (MAX_CTB_SIZE + MAX_TU_SIZE) *
        ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);
    ps_mem_tab[ENC_LOOP_LEFT_CHROMA_DATA].i4_mem_size <<=
        ((i4_chroma_format == IV_YUV_422SP_UV) ? 1 : 0);

    ps_mem_tab[ENC_LOOP_LEFT_CHROMA_DATA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

    ps_mem_tab[ENC_LOOP_LEFT_CHROMA_DATA].i4_mem_alignment = 8;

    /* Memory required for SAO */
    {
        WORD32 num_vert_units;
        WORD32 num_horz_units;
        WORD32 ctb_aligned_ht, ctb_aligned_wd;
        WORD32 luma_buf, chroma_buf;

        num_vert_units = u4_height / MAX_CTB_SIZE;
        num_horz_units = u4_width / MAX_CTB_SIZE;

        ctb_aligned_ht = u4_height;
        ctb_aligned_wd = u4_width;

        /* Memory for top buffer. 1 extra width is required for top buf ptr for row 0
        * and 1 extra location is required for top left buf ptr for row 0
        * Also 1 extra byte is required for every row for top left pixel if
        * the top left ptr is to be passed to leaf level unconditionally
        */
        luma_buf = (ctb_aligned_ht + (ctb_aligned_wd + 1) * (num_vert_units + 1)) *
                   ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);
        chroma_buf = (ctb_aligned_ht + (ctb_aligned_wd + 2) * (num_vert_units + 1)) *
                     ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

        ps_mem_tab[ENC_LOOP_SAO].i4_mem_size =
            (luma_buf + chroma_buf) * (i4_num_bitrate_inst) * (i4_num_enc_loop_frm_pllel);

        /* Add the memory required to store the sao information of top ctb for top merge
        * This is frame level buffer.
        */
        ps_mem_tab[ENC_LOOP_SAO].i4_mem_size +=
            ((num_horz_units * sizeof(sao_enc_t)) * num_vert_units) * (i4_num_bitrate_inst) *
            (i4_num_enc_loop_frm_pllel);

        ps_mem_tab[ENC_LOOP_SAO].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_SAO].i4_mem_alignment = 8;
    }

    /* Memory for CU level Coeff data buffer */
    {
        /* 16 additional bytes are required to ensure alignment */
        {
            ps_mem_tab[ENC_LOOP_CU_COEFF_DATA].i4_mem_size =
                i4_num_proc_thrds *
                (((MAX_LUMA_COEFFS_CTB +
                   (MAX_CHRM_COEFFS_CTB << ((i4_chroma_format == IV_YUV_422SP_UV) ? 1 : 0))) +
                  16) *
                 (2) * sizeof(UWORD8));
        }

        ps_mem_tab[ENC_LOOP_CU_COEFF_DATA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_COEFF_DATA].i4_mem_alignment = 16;

        ps_mem_tab[ENC_LOOP_CU_RECUR_COEFF_DATA].i4_mem_size =
            i4_num_proc_thrds *
            (MAX_LUMA_COEFFS_CTB +
             (MAX_CHRM_COEFFS_CTB << ((i4_chroma_format == IV_YUV_422SP_UV) ? 1 : 0))) *
            sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_CU_RECUR_COEFF_DATA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_RECUR_COEFF_DATA].i4_mem_alignment = 16;
    }

    /* Memory for CU dequant data buffer */
    {
        /* 16 additional bytes are required to ensure alignment */
        {
            ps_mem_tab[ENC_LOOP_CU_DEQUANT_DATA].i4_mem_size =
                i4_num_proc_thrds *
                (((i4_chroma_format == IV_YUV_422SP_UV) ? (MAX_CU_SIZE * (MAX_CU_SIZE << 1))
                                                        : (MAX_CU_SIZE * (MAX_CU_SIZE >> 1) * 3)) +
                 8) *
                (2) * sizeof(WORD16);
        }

        ps_mem_tab[ENC_LOOP_CU_DEQUANT_DATA].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_CU_DEQUANT_DATA].i4_mem_alignment = 16;
    }

    /* Memory for Recon Datastore (Used around and within the RDOPT loop) */
    {
        WORD32 i4_memSize_perThread;

        WORD32 i4_chroma_memSize_perThread = 0;
        /* 2 bufs each allocated to the two 'enc_loop_cu_final_prms_t' structs */
        /* used in RDOPT to store cur and best modes' data */
        WORD32 i4_luma_memSize_perThread =
            4 * MAX_CU_SIZE * MAX_CU_SIZE *
            ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

        /* 'Glossary' for comments in the following codeBlock */
        /* 1 - 2 Bufs for storing recons of the best modes determined in the */
        /* function 'ihevce_intra_chroma_pred_mode_selector' */
        /* 2 - 1 buf each allocated to the two 'enc_loop_cu_final_prms_t' structs */
        /* used in RDOPT to store cur and best modes' data */
        if(i4_chroma_format == IV_YUV_422SP_UV)
        {
            WORD32 i4_quality_preset =
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;
            switch(i4_quality_preset)
            {
            case IHEVCE_QUALITY_P0:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_PQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_PQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P2:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_HQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P3:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_MS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_MS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P4:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_HS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P5:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_XS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P6:
            case IHEVCE_QUALITY_P7:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_CHROMA_RDOPT_EVAL_IN_XS6 *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * MAX_CU_SIZE * ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS6 *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            }
        }
        else
        {
            WORD32 i4_quality_preset =
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;
            switch(i4_quality_preset)
            {
            case IHEVCE_QUALITY_P0:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_PQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_PQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P2:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_HQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HQ *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P3:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_MS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_MS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P4:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_HS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P5:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_XS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            case IHEVCE_QUALITY_P6:
            case IHEVCE_QUALITY_P7:
            {
                /* 1 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ENABLE_CHROMA_RDOPT_EVAL_IN_XS6 *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                /* 2 */
                i4_chroma_memSize_perThread +=
                    2 * MAX_CU_SIZE * (MAX_CU_SIZE / 2) *
                    ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS6 *
                    ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1);

                break;
            }
            }
        }

        i4_memSize_perThread = i4_luma_memSize_perThread + i4_chroma_memSize_perThread;

        ps_mem_tab[ENC_LOOP_RECON_DATA_STORE].i4_mem_size =
            i4_num_proc_thrds * i4_memSize_perThread * sizeof(UWORD8);

        ps_mem_tab[ENC_LOOP_RECON_DATA_STORE].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;

        ps_mem_tab[ENC_LOOP_RECON_DATA_STORE].i4_mem_alignment = 16;
    }

    n_tabs = NUM_ENC_LOOP_MEM_RECS;

    /*************************************************************************/
    /* --- EncLoop Deblock and SAO sync Dep Mngr Mem requests --                     */
    /*************************************************************************/

    /* Fill the memtabs for  EncLoop Deblock Dep Mngr */
    {
        WORD32 count;
        WORD32 num_vert_units;
        WORD32 ht = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;

        ihevce_enc_loop_dblk_get_prms_dep_mngr(ht, &num_vert_units);
        ASSERT(num_vert_units > 0);
        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(ctr = 0; ctr < i4_num_bitrate_inst; ctr++)
            {
                n_tabs += ihevce_dmgr_get_mem_recs(
                    &ps_mem_tab[n_tabs],
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    ps_init_prms->s_app_tile_params.i4_num_tile_cols,
                    i4_num_proc_thrds,
                    i4_mem_space);
            }
        }

        /* Fill the memtabs for  EncLoop SAO Dep Mngr */
        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(ctr = 0; ctr < i4_num_bitrate_inst; ctr++)
            {
                n_tabs += ihevce_dmgr_get_mem_recs(
                    &ps_mem_tab[n_tabs],
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    ps_init_prms->s_app_tile_params.i4_num_tile_cols,
                    i4_num_proc_thrds,
                    i4_mem_space);
            }
        }
    }

    /*************************************************************************/
    /* --- EncLoop Top-Right CU sync Dep Mngr Mem requests --                */
    /*************************************************************************/

    /* Fill the memtabs for  Top-Right CU sync Dep Mngr */
    {
        WORD32 count;
        WORD32 num_vert_units;
        WORD32 ht = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
        ihevce_enc_loop_dblk_get_prms_dep_mngr(ht, &num_vert_units);
        ASSERT(num_vert_units > 0);

        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(ctr = 0; ctr < i4_num_bitrate_inst; ctr++)
            {
                n_tabs += ihevce_dmgr_get_mem_recs(
                    &ps_mem_tab[n_tabs],
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    ps_init_prms->s_app_tile_params.i4_num_tile_cols,
                    i4_num_proc_thrds,
                    i4_mem_space);
            }
        }
    }

    /*************************************************************************/
    /* --- EncLoop Aux. on Ref. bitrate sync Dep Mngr Mem requests --        */
    /*************************************************************************/

    /* Fill the memtabs for  EncLoop Aux. on Ref. bitrate Dep Mngr */
    {
        WORD32 count;
        WORD32 num_vert_units;
        WORD32 ht = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;

        ihevce_enc_loop_dblk_get_prms_dep_mngr(ht, &num_vert_units);
        ASSERT(num_vert_units > 0);

        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(ctr = 1; ctr < i4_num_bitrate_inst; ctr++)
            {
                n_tabs += ihevce_dmgr_get_mem_recs(
                    &ps_mem_tab[n_tabs],
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    ps_init_prms->s_app_tile_params.i4_num_tile_cols,
                    i4_num_proc_thrds,
                    i4_mem_space);
            }
        }
    }

    return (n_tabs);
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_init \endif
*
* \brief
*    Intialization for ENC_LOOP context state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
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
void *ihevce_enc_loop_init(
    iv_mem_rec_t *ps_mem_tab,
    ihevce_static_cfg_params_t *ps_init_prms,
    WORD32 i4_num_proc_thrds,
    void *pv_osal_handle,
    func_selector_t *ps_func_selector,
    rc_quant_t *ps_rc_quant_ctxt,
    ihevce_tile_params_t *ps_tile_params_base,
    WORD32 i4_resolution_id,
    WORD32 i4_num_enc_loop_frm_pllel,
    UWORD8 u1_is_popcnt_available)
{
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt;
    ihevce_enc_loop_ctxt_t *ps_ctxt;
    WORD32 ctr, n_tabs;
    UWORD32 u4_width, u4_height;
    UWORD32 u4_ctb_in_a_row, u4_ctb_rows_in_a_frame;
    UWORD32 u4_size_bs_memory, u4_size_qp_memory;
    UWORD8 *pu1_deblk_base; /*Store the base address of deblcoking memory*/
    WORD32 i;
    WORD32 i4_num_bitrate_inst =
        ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_num_bitrate_instances;
    enc_loop_rc_params_t *ps_enc_loop_rc_params;
    UWORD8 *pu1_sao_base; /* store the base address of sao*/
    UWORD32 u4_ctb_aligned_wd, ctb_size, u4_ctb_aligned_ht, num_vert_units;
    WORD32 i4_chroma_format = ps_init_prms->s_src_prms.i4_chr_format;
    WORD32 is_hbd_mode = (ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8);
    WORD32 i4_enc_frm_id;
    WORD32 num_cu_in_ctb;
    WORD32 i4_num_tile_cols = 1;  //Default value is 1

    /* ENC_LOOP state structure */
    ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)ps_mem_tab[ENC_LOOP_CTXT].pv_base;

    ps_master_ctxt->i4_num_proc_thrds = i4_num_proc_thrds;

    ps_ctxt = (ihevce_enc_loop_ctxt_t *)ps_mem_tab[ENC_LOOP_THRDS_CTXT].pv_base;
    ps_enc_loop_rc_params = (enc_loop_rc_params_t *)ps_mem_tab[ENC_LOOP_RC_PARAMS].pv_base;
    ps_ctxt->ps_rc_quant_ctxt = ps_rc_quant_ctxt;
    /*Calculation of memory sizes for deblocking*/
    {
        /*width of the input YUV to be encoded. */
        u4_width = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
        /*making the width a multiple of CTB size*/
        u4_width += SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width, MAX_CTB_SIZE);

        u4_ctb_in_a_row = (u4_width / MAX_CTB_SIZE);

        /*height of the input YUV to be encoded */
        u4_height = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
        /*making the height a multiple of CTB size*/
        u4_height += SET_CTB_ALIGN(
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height, MAX_CTB_SIZE);

        u4_ctb_rows_in_a_frame = (u4_height / MAX_CTB_SIZE);

        /*Memory(in bytes) required for storing Boundary Strength for entire CTB row*/
        /*1 vertical edge per 8 pixel*/
        u4_size_bs_memory = (MAX_CTB_SIZE >> 3);
        /*Vertical edges for entire width of CTB row*/
        u4_size_bs_memory *= u4_ctb_in_a_row;
        /*Each vertical edge of CTB row is 4 bytes*/
        u4_size_bs_memory = u4_size_bs_memory << 2;
        /*Adding Memory required for storing horizontal BS by doubling*/
        u4_size_bs_memory = u4_size_bs_memory << 1;

        /*Memory(in bytes) required for storing Qp at 4x4 level for entire CTB row*/
        /*Number of 4x4 blocks in the width of a CTB*/
        u4_size_qp_memory = (MAX_CTB_SIZE >> 2);
        /*Number of 4x4 blocks in the height of a CTB. Adding 1 to store Qp of lowest
        4x4-block layer of top-CTB in order to deblock top edge of current CTB*/
        u4_size_qp_memory *= ((MAX_CTB_SIZE >> 2) + 1);
        /*Storage for entire CTB row*/
        u4_size_qp_memory *= u4_ctb_in_a_row;

        pu1_deblk_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_DEBLOCKING].pv_base;
    }

    /*Derive the base pointer of sao*/
    pu1_sao_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_SAO].pv_base;
    ctb_size = (1 << ps_init_prms->s_config_prms.i4_max_log2_cu_size);
    u4_ctb_aligned_wd = u4_width;
    u4_ctb_aligned_ht = u4_height;
    num_vert_units = (u4_height) / ctb_size;

    for(ctr = 0; ctr < ps_master_ctxt->i4_num_proc_thrds; ctr++)
    {
        ps_master_ctxt->aps_enc_loop_thrd_ctxt[ctr] = ps_ctxt;
        /* Store Tile params base into EncLoop context */
        ps_ctxt->pv_tile_params_base = (void *)ps_tile_params_base;
        ihevce_cmn_utils_instr_set_router(
            &ps_ctxt->s_cmn_opt_func, u1_is_popcnt_available, ps_init_prms->e_arch_type);
        ihevce_sifter_sad_fxn_assigner(
            (FT_SAD_EVALUATOR **)(&ps_ctxt->pv_evalsad_pt_npu_mxn_8bit), ps_init_prms->e_arch_type);
        ps_ctxt->i4_max_search_range_horizontal =
            ps_init_prms->s_config_prms.i4_max_search_range_horz;
        ps_ctxt->i4_max_search_range_vertical =
            ps_init_prms->s_config_prms.i4_max_search_range_vert;

        ps_ctxt->i4_quality_preset =
            ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;

        if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P7)
        {
            ps_ctxt->i4_quality_preset = IHEVCE_QUALITY_P6;
        }

        ps_ctxt->i4_num_proc_thrds = ps_master_ctxt->i4_num_proc_thrds;

        ps_ctxt->i4_rc_pass = ps_init_prms->s_pass_prms.i4_pass;

        ps_ctxt->u1_chroma_array_type = (i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1;

        ps_ctxt->s_deblk_prms.u1_chroma_array_type = ps_ctxt->u1_chroma_array_type;

        ps_ctxt->pi2_scal_mat = (WORD16 *)ps_mem_tab[ENC_LOOP_SCALE_MAT].pv_base;

        ps_ctxt->pi2_rescal_mat = (WORD16 *)ps_mem_tab[ENC_LOOP_RESCALE_MAT].pv_base;

        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P2)
        {
            ps_ctxt->i4_use_ctb_level_lamda = 0;
        }
        else
        {
            ps_ctxt->i4_use_ctb_level_lamda = 0;
        }

        /** Register the function selector pointer*/
        ps_ctxt->ps_func_selector = ps_func_selector;

        ps_ctxt->s_mc_ctxt.ps_func_selector = ps_func_selector;

        /* Initiallization for non-distributed mode */
        ps_ctxt->s_mc_ctxt.ai4_tile_xtra_pel[0] = 0;
        ps_ctxt->s_mc_ctxt.ai4_tile_xtra_pel[1] = 0;
        ps_ctxt->s_mc_ctxt.ai4_tile_xtra_pel[2] = 0;
        ps_ctxt->s_mc_ctxt.ai4_tile_xtra_pel[3] = 0;

        ps_ctxt->s_deblk_prms.ps_func_selector = ps_func_selector;
        ps_ctxt->i4_top_row_luma_stride = (u4_width + MAX_CU_SIZE + 1);

        ps_ctxt->i4_frm_top_row_luma_size =
            ps_ctxt->i4_top_row_luma_stride * (u4_ctb_rows_in_a_frame + 1);

        ps_ctxt->i4_top_row_chroma_stride = (u4_width + MAX_CU_SIZE + 2);

        ps_ctxt->i4_frm_top_row_chroma_size =
            ps_ctxt->i4_top_row_chroma_stride * (u4_ctb_rows_in_a_frame + 1);

        {
            for(i4_enc_frm_id = 0; i4_enc_frm_id < i4_num_enc_loop_frm_pllel; i4_enc_frm_id++)
            {
                /* +1 is to provision top left pel */
                ps_ctxt->apv_frm_top_row_luma[i4_enc_frm_id] =
                    (UWORD8 *)ps_mem_tab[ENC_LOOP_TOP_LUMA].pv_base + 1 +
                    (ps_ctxt->i4_frm_top_row_luma_size * i4_enc_frm_id * i4_num_bitrate_inst);

                /* pointer incremented by 1 row to avoid OOB access in 0th row */
                ps_ctxt->apv_frm_top_row_luma[i4_enc_frm_id] =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_luma[i4_enc_frm_id] +
                    ps_ctxt->i4_top_row_luma_stride;

                /* +2 is to provision top left pel */
                ps_ctxt->apv_frm_top_row_chroma[i4_enc_frm_id] =
                    (UWORD8 *)ps_mem_tab[ENC_LOOP_TOP_CHROMA].pv_base + 2 +
                    (ps_ctxt->i4_frm_top_row_chroma_size * i4_enc_frm_id * i4_num_bitrate_inst);

                /* pointer incremented by 1 row to avoid OOB access in 0th row */
                ps_ctxt->apv_frm_top_row_chroma[i4_enc_frm_id] =
                    (UWORD8 *)ps_ctxt->apv_frm_top_row_chroma[i4_enc_frm_id] +
                    ps_ctxt->i4_top_row_chroma_stride;
            }
        }

        /* +1 is to provision top left nbr */
        ps_ctxt->i4_top_row_nbr_stride = (((u4_width + MAX_CU_SIZE) >> 2) + 1);
        ps_ctxt->i4_frm_top_row_nbr_size =
            ps_ctxt->i4_top_row_nbr_stride * (u4_ctb_rows_in_a_frame + 1);
        for(i4_enc_frm_id = 0; i4_enc_frm_id < i4_num_enc_loop_frm_pllel; i4_enc_frm_id++)
        {
            ps_ctxt->aps_frm_top_row_nbr[i4_enc_frm_id] =
                (nbr_4x4_t *)ps_mem_tab[ENC_LOOP_TOP_NBR4X4].pv_base + 1 +
                (ps_ctxt->i4_frm_top_row_nbr_size * i4_enc_frm_id * i4_num_bitrate_inst);
            ps_ctxt->aps_frm_top_row_nbr[i4_enc_frm_id] += ps_ctxt->i4_top_row_nbr_stride;
        }

        num_cu_in_ctb = ctb_size / MIN_CU_SIZE;
        num_cu_in_ctb *= num_cu_in_ctb;

        /* pointer incremented by 1 row to avoid OOB access in 0th row */

        /* Memory for CU level Coeff data buffer */
        {
            WORD32 i4_16byte_boundary_overshoot;
            WORD32 buf_size_per_cu;
            WORD32 buf_size_per_thread_wo_alignment_req;
            WORD32 buf_size_per_thread;

            buf_size_per_cu =
                ((MAX_LUMA_COEFFS_CTB +
                  (MAX_CHRM_COEFFS_CTB << ((i4_chroma_format == IV_YUV_422SP_UV) ? 1 : 0))) +
                 16) *
                sizeof(UWORD8);
            buf_size_per_thread_wo_alignment_req = buf_size_per_cu - 16 * sizeof(UWORD8);

            {
                buf_size_per_thread = buf_size_per_cu * (2);

                for(i = 0; i < 2; i++)
                {
                    ps_ctxt->as_cu_prms[i].pu1_cu_coeffs =
                        (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_COEFF_DATA].pv_base +
                        (ctr * buf_size_per_thread) + (i * buf_size_per_cu);

                    i4_16byte_boundary_overshoot =
                        ((LWORD64)ps_ctxt->as_cu_prms[i].pu1_cu_coeffs & 0xf);

                    ps_ctxt->as_cu_prms[i].pu1_cu_coeffs += (16 - i4_16byte_boundary_overshoot);
                }
            }

            ps_ctxt->pu1_cu_recur_coeffs =
                (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_RECUR_COEFF_DATA].pv_base +
                (ctr * buf_size_per_thread_wo_alignment_req);
        }

        /* Memory for CU dequant data buffer */
        {
            WORD32 buf_size_per_thread;
            WORD32 i4_16byte_boundary_overshoot;

            WORD32 buf_size_per_cu =
                (((i4_chroma_format == IV_YUV_422SP_UV) ? (MAX_CU_SIZE * (MAX_CU_SIZE << 1))
                                                        : (MAX_CU_SIZE * (MAX_CU_SIZE >> 1) * 3)) +
                 8) *
                sizeof(WORD16);

            {
                buf_size_per_thread = buf_size_per_cu * 2;

                for(i = 0; i < 2; i++)
                {
                    ps_ctxt->as_cu_prms[i].pi2_cu_deq_coeffs =
                        (WORD16
                             *)((UWORD8 *)ps_mem_tab[ENC_LOOP_CU_DEQUANT_DATA].pv_base + (ctr * buf_size_per_thread) + (i * buf_size_per_cu));

                    i4_16byte_boundary_overshoot =
                        ((LWORD64)ps_ctxt->as_cu_prms[i].pi2_cu_deq_coeffs & 0xf);

                    ps_ctxt->as_cu_prms[i].pi2_cu_deq_coeffs =
                        (WORD16
                             *)((UWORD8 *)ps_ctxt->as_cu_prms[i].pi2_cu_deq_coeffs + (16 - i4_16byte_boundary_overshoot));
                }
            }
        }

        /*------ Deblocking memory's pointers assignements starts ------*/

        /*Assign stride = 4x4 blocks in horizontal edge*/
        ps_ctxt->s_deblk_ctbrow_prms.u4_qp_top_4x4_buf_strd = (MAX_CTB_SIZE / 4) * u4_ctb_in_a_row;

        ps_ctxt->s_deblk_ctbrow_prms.u4_qp_top_4x4_buf_size =
            ps_ctxt->s_deblk_ctbrow_prms.u4_qp_top_4x4_buf_strd * u4_ctb_rows_in_a_frame;

        /*Assign frame level memory to store the Qp of
        top 4x4 neighbours of each CTB row*/
        for(i4_enc_frm_id = 0; i4_enc_frm_id < i4_num_enc_loop_frm_pllel; i4_enc_frm_id++)
        {
            ps_ctxt->s_deblk_ctbrow_prms.api1_qp_top_4x4_ctb_row[i4_enc_frm_id] =
                (WORD8 *)ps_mem_tab[ENC_LOOP_QP_TOP_4X4].pv_base +
                (ps_ctxt->s_deblk_ctbrow_prms.u4_qp_top_4x4_buf_size * i4_num_bitrate_inst *
                 i4_enc_frm_id);
        }

        ps_ctxt->s_deblk_ctbrow_prms.pu4_ctb_row_bs_vert = (UWORD32 *)pu1_deblk_base;

        ps_ctxt->s_deblk_ctbrow_prms.pu4_ctb_row_bs_horz =
            (UWORD32 *)(pu1_deblk_base + (u4_size_bs_memory >> 1));

        ps_ctxt->s_deblk_ctbrow_prms.pi1_ctb_row_qp = (WORD8 *)pu1_deblk_base + u4_size_bs_memory;

        /*Assign stride = 4x4 blocks in horizontal edge*/
        ps_ctxt->s_deblk_ctbrow_prms.u4_qp_buffer_stride = (MAX_CTB_SIZE / 4) * u4_ctb_in_a_row;

        pu1_deblk_base += (u4_size_bs_memory + u4_size_qp_memory);

        /*------Deblocking memory's pointers assignements ends ------*/

        /*------SAO memory's pointer assignment starts------------*/
        if(!is_hbd_mode)
        {
            /* 2 is added to allocate top left pixel */
            ps_ctxt->s_sao_ctxt_t.i4_top_luma_buf_size =
                u4_ctb_aligned_ht + (u4_ctb_aligned_wd + 1) * (num_vert_units + 1);
            ps_ctxt->s_sao_ctxt_t.i4_top_chroma_buf_size =
                u4_ctb_aligned_ht + (u4_ctb_aligned_wd + 2) * (num_vert_units + 1);
            ps_ctxt->s_sao_ctxt_t.i4_num_ctb_units =
                num_vert_units * (u4_ctb_aligned_wd / MAX_CTB_SIZE);

            for(i4_enc_frm_id = 0; i4_enc_frm_id < i4_num_enc_loop_frm_pllel; i4_enc_frm_id++)
            {
                ps_ctxt->s_sao_ctxt_t.apu1_sao_src_frm_top_luma[i4_enc_frm_id] =
                    pu1_sao_base +
                    ((ps_ctxt->s_sao_ctxt_t.i4_top_luma_buf_size +
                      ps_ctxt->s_sao_ctxt_t.i4_top_chroma_buf_size) *
                     i4_num_bitrate_inst * i4_enc_frm_id) +  // move to the next frame_id
                    u4_ctb_aligned_wd +
                    2;

                ps_ctxt->s_sao_ctxt_t.apu1_sao_src_frm_top_chroma[i4_enc_frm_id] =
                    pu1_sao_base +
                    ((ps_ctxt->s_sao_ctxt_t.i4_top_luma_buf_size +
                      ps_ctxt->s_sao_ctxt_t.i4_top_chroma_buf_size) *
                     i4_num_bitrate_inst * i4_enc_frm_id) +
                    +u4_ctb_aligned_ht + (u4_ctb_aligned_wd + 1) * (num_vert_units + 1) +
                    u4_ctb_aligned_wd + 4;

                ps_ctxt->s_sao_ctxt_t.aps_frm_top_ctb_sao[i4_enc_frm_id] = (sao_enc_t *) (pu1_sao_base +
                    ((ps_ctxt->s_sao_ctxt_t.i4_top_luma_buf_size + ps_ctxt->s_sao_ctxt_t.i4_top_chroma_buf_size)
                    *i4_num_bitrate_inst*i4_num_enc_loop_frm_pllel) +
                    (ps_ctxt->s_sao_ctxt_t.i4_num_ctb_units * sizeof(sao_enc_t) *i4_num_bitrate_inst * i4_enc_frm_id));
            }
            ps_ctxt->s_sao_ctxt_t.i4_ctb_size =
                (1 << ps_init_prms->s_config_prms.i4_max_log2_cu_size);
            ps_ctxt->s_sao_ctxt_t.u4_ctb_aligned_wd = u4_ctb_aligned_wd;
        }

        /*------SAO memory's pointer assignment ends------------*/

        /* perform all one time initialisation here */
        ps_ctxt->i4_nbr_map_strd = MAX_PU_IN_CTB_ROW + 1 + 8;

        ps_ctxt->pu1_ctb_nbr_map = ps_ctxt->au1_nbr_ctb_map[0];

        ps_ctxt->i4_deblock_type = ps_init_prms->s_coding_tools_prms.i4_deblocking_type;

        /* move the pointer to 1,2 location */
        ps_ctxt->pu1_ctb_nbr_map += ps_ctxt->i4_nbr_map_strd;
        ps_ctxt->pu1_ctb_nbr_map++;

        ps_ctxt->i4_cu_csbf_strd = MAX_TU_IN_CTB_ROW;

        CREATE_SUBBLOCK2CSBFID_MAP(gai4_subBlock2csbfId_map4x4TU, 1, 4, ps_ctxt->i4_cu_csbf_strd);

        CREATE_SUBBLOCK2CSBFID_MAP(gai4_subBlock2csbfId_map8x8TU, 4, 8, ps_ctxt->i4_cu_csbf_strd);

        CREATE_SUBBLOCK2CSBFID_MAP(
            gai4_subBlock2csbfId_map16x16TU, 16, 16, ps_ctxt->i4_cu_csbf_strd);

        CREATE_SUBBLOCK2CSBFID_MAP(
            gai4_subBlock2csbfId_map32x32TU, 64, 32, ps_ctxt->i4_cu_csbf_strd);

        /* For both instance initialise the chroma dequant start idx */
        ps_ctxt->as_cu_prms[0].i4_chrm_deq_coeff_strt_idx = (MAX_CU_SIZE * MAX_CU_SIZE);
        ps_ctxt->as_cu_prms[1].i4_chrm_deq_coeff_strt_idx = (MAX_CU_SIZE * MAX_CU_SIZE);

        /* initialise all the function pointer tables */
        {
            ps_ctxt->pv_inter_rdopt_cu_mc_mvp =
                (pf_inter_rdopt_cu_mc_mvp)ihevce_inter_rdopt_cu_mc_mvp;

            ps_ctxt->pv_inter_rdopt_cu_ntu = (pf_inter_rdopt_cu_ntu)ihevce_inter_rdopt_cu_ntu;

#if ENABLE_RDO_BASED_TU_RECURSION
            if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P0)
            {
                ps_ctxt->pv_inter_rdopt_cu_ntu =
                    (pf_inter_rdopt_cu_ntu)ihevce_inter_tu_tree_selector_and_rdopt_cost_computer;
            }
#endif
            ps_ctxt->pv_intra_chroma_pred_mode_selector =
                (pf_intra_chroma_pred_mode_selector)ihevce_intra_chroma_pred_mode_selector;
            ps_ctxt->pv_intra_rdopt_cu_ntu = (pf_intra_rdopt_cu_ntu)ihevce_intra_rdopt_cu_ntu;
            ps_ctxt->pv_final_rdopt_mode_prcs =
                (pf_final_rdopt_mode_prcs)ihevce_final_rdopt_mode_prcs;
            ps_ctxt->pv_store_cu_results = (pf_store_cu_results)ihevce_store_cu_results;
            ps_ctxt->pv_enc_loop_cu_bot_copy = (pf_enc_loop_cu_bot_copy)ihevce_enc_loop_cu_bot_copy;
            ps_ctxt->pv_enc_loop_ctb_left_copy =
                (pf_enc_loop_ctb_left_copy)ihevce_enc_loop_ctb_left_copy;

            /* Memory assignments for chroma intra pred buffer */
            {
                WORD32 pred_buf_size =
                    MAX_TU_SIZE * MAX_TU_SIZE * 2 * ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1);
                WORD32 pred_buf_size_per_thread =
                    NUM_POSSIBLE_TU_SIZES_CHR_INTRA_SATD * pred_buf_size;
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_CHROMA_PRED_INTRA].pv_base +
                                   (ctr * pred_buf_size_per_thread);

                for(i = 0; i < NUM_POSSIBLE_TU_SIZES_CHR_INTRA_SATD; i++)
                {
                    ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[i].pv_pred_data = pu1_base;
                    pu1_base += pred_buf_size;
                }
            }

            /* Memory assignments for reference substitution output */
            {
                WORD32 pred_buf_size = ((MAX_TU_SIZE * 2 * 2) + INTRAPRED_SIMD_RIGHT_PADDING
                                       + INTRAPRED_SIMD_LEFT_PADDING);
                WORD32 pred_buf_size_per_thread = pred_buf_size;
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_REF_SUB_OUT].pv_base +
                                   (ctr * pred_buf_size_per_thread);

                ps_ctxt->pv_ref_sub_out = pu1_base + INTRAPRED_SIMD_LEFT_PADDING;
            }

            /* Memory assignments for reference filtering output */
            {
                WORD32 pred_buf_size = ((MAX_TU_SIZE * 2 * 2) + INTRAPRED_SIMD_RIGHT_PADDING
                                       + INTRAPRED_SIMD_LEFT_PADDING);
                WORD32 pred_buf_size_per_thread = pred_buf_size;
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_REF_FILT_OUT].pv_base +
                                   (ctr * pred_buf_size_per_thread);

                ps_ctxt->pv_ref_filt_out = pu1_base + INTRAPRED_SIMD_LEFT_PADDING;
            }

            /* Memory assignments for recon storage during CU Recursion */
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
            if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P0)
#endif
            {
                {
                    WORD32 pred_buf_size = (MAX_CU_SIZE * MAX_CU_SIZE);
                    WORD32 pred_buf_size_per_thread = pred_buf_size;
                    UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_RECON].pv_base +
                                       (ctr * pred_buf_size_per_thread);

                    ps_ctxt->pv_cu_luma_recon = pu1_base;
                }

                {
                    WORD32 pred_buf_size = ((MAX_CU_SIZE * MAX_CU_SIZE) >> 1) *
                                           ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1);
                    WORD32 pred_buf_size_per_thread = pred_buf_size;
                    UWORD8 *pu1_base =
                        (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_RECON].pv_base +
                        (ctr * pred_buf_size_per_thread);

                    ps_ctxt->pv_cu_chrma_recon = pu1_base;
                }
            }

            /* Memory assignments for pred storage during CU Recursion */
#if !PROCESS_GT_1CTB_VIA_CU_RECUR_IN_FAST_PRESETS
            if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P0)
#endif
            {
                {
                    WORD32 pred_buf_size = (MAX_CU_SIZE * MAX_CU_SIZE);
                    WORD32 pred_buf_size_per_thread = pred_buf_size;
                    UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_RECUR_LUMA_PRED].pv_base +
                                       (ctr * pred_buf_size_per_thread);

                    ps_ctxt->pv_CTB_pred_luma = pu1_base;
                }

                {
                    WORD32 pred_buf_size = ((MAX_CU_SIZE * MAX_CU_SIZE) >> 1) *
                                           ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1);
                    WORD32 pred_buf_size_per_thread = pred_buf_size;
                    UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_CU_RECUR_CHROMA_PRED].pv_base +
                                       (ctr * pred_buf_size_per_thread);

                    ps_ctxt->pv_CTB_pred_chroma = pu1_base;
                }
            }

            /* Memory assignments for CTB left luma data storage */
            {
                WORD32 pred_buf_size = (MAX_CTB_SIZE + MAX_TU_SIZE);
                WORD32 pred_buf_size_per_thread = pred_buf_size;
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_LEFT_LUMA_DATA].pv_base +
                                   (ctr * pred_buf_size_per_thread);

                ps_ctxt->pv_left_luma_data = pu1_base;
            }

            /* Memory assignments for CTB left chroma data storage */
            {
                WORD32 pred_buf_size =
                    (MAX_CTB_SIZE + MAX_TU_SIZE) * ((i4_chroma_format == IV_YUV_422SP_UV) ? 2 : 1);
                WORD32 pred_buf_size_per_thread = pred_buf_size;
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_LEFT_CHROMA_DATA].pv_base +
                                   (ctr * pred_buf_size_per_thread);

                ps_ctxt->pv_left_chrm_data = pu1_base;
            }
        }

        /* Memory for inter pred buffers */
        {
            WORD32 i4_num_bufs_per_thread;

            WORD32 i4_buf_size_per_cand =
                (MAX_CTB_SIZE) * (MAX_CTB_SIZE) *
                ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) * sizeof(UWORD8);

            i4_num_bufs_per_thread =
                (ps_mem_tab[ENC_LOOP_INTER_PRED].i4_mem_size / i4_num_proc_thrds) /
                i4_buf_size_per_cand;

            ps_ctxt->i4_max_num_inter_rdopt_cands = i4_num_bufs_per_thread - 4;

            ps_ctxt->s_pred_buf_data.u4_is_buf_in_use = UINT_MAX;

            {
                UWORD8 *pu1_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_INTER_PRED].pv_base +
                                   +(ctr * i4_buf_size_per_cand * i4_num_bufs_per_thread);

                for(i = 0; i < i4_num_bufs_per_thread; i++)
                {
                    ps_ctxt->s_pred_buf_data.apv_inter_pred_data[i] =
                        pu1_base + i * i4_buf_size_per_cand;
                    ps_ctxt->s_pred_buf_data.u4_is_buf_in_use ^= (1 << i);
                }
            }
        }

        /* Memory required to store pred for 422 chroma */
        if(i4_chroma_format == IV_YUV_422SP_UV)
        {
            WORD32 pred_buf_size = MAX_CTB_SIZE * MAX_CTB_SIZE * 2;
            WORD32 pred_buf_size_per_thread =
                pred_buf_size * ((ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth > 8) ? 2 : 1) *
                sizeof(UWORD8);
            void *pv_base = (UWORD8 *)ps_mem_tab[ENC_LOOP_422_CHROMA_INTRA_PRED].pv_base +
                            (ctr * pred_buf_size_per_thread);

            ps_ctxt->pv_422_chroma_intra_pred_buf = pv_base;
        }
        else
        {
            ps_ctxt->pv_422_chroma_intra_pred_buf = NULL;
        }

        /* Memory for Recon Datastore (Used around and within the RDOPT loop) */
        {
            WORD32 i4_lumaBufSize = MAX_CU_SIZE * MAX_CU_SIZE;
            WORD32 i4_chromaBufSize =
                MAX_CU_SIZE * (MAX_CU_SIZE / 2) * ((i4_chroma_format == IV_YUV_422SP_UV) + 1);
            WORD32 i4_memSize_perThread = ps_mem_tab[ENC_LOOP_RECON_DATA_STORE].i4_mem_size /
                                          (i4_num_proc_thrds * sizeof(UWORD8) * (is_hbd_mode + 1));
            WORD32 i4_quality_preset = ps_ctxt->i4_quality_preset;
            {
                UWORD8 *pu1_mem_base =
                    (((UWORD8 *)ps_mem_tab[ENC_LOOP_RECON_DATA_STORE].pv_base) +
                     ctr * i4_memSize_perThread);

                ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_luma_recon_bufs[0] =
                    pu1_mem_base + i4_lumaBufSize * 0;
                ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_luma_recon_bufs[1] =
                    pu1_mem_base + i4_lumaBufSize * 1;
                ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_luma_recon_bufs[0] =
                    pu1_mem_base + i4_lumaBufSize * 2;
                ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_luma_recon_bufs[1] =
                    pu1_mem_base + i4_lumaBufSize * 3;

                pu1_mem_base += i4_lumaBufSize * 4;

                switch(i4_quality_preset)
                {
                case IHEVCE_QUALITY_P0:
                {
#if ENABLE_CHROMA_RDOPT_EVAL_IN_PQ
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 0;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 1;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
#endif

#if ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_PQ
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
#endif

                    break;
                }
                case IHEVCE_QUALITY_P2:
                {
#if ENABLE_CHROMA_RDOPT_EVAL_IN_HQ
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 0;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 1;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
#endif

#if ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HQ
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
#endif

                    break;
                }
                case IHEVCE_QUALITY_P3:
                {
#if ENABLE_CHROMA_RDOPT_EVAL_IN_MS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 0;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 1;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
#endif

#if ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_MS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
#endif

                    break;
                }
                case IHEVCE_QUALITY_P4:
                {
#if ENABLE_CHROMA_RDOPT_EVAL_IN_HS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 0;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 1;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
#endif

#if ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
#endif

                    break;
                }
                case IHEVCE_QUALITY_P5:
                {
#if ENABLE_CHROMA_RDOPT_EVAL_IN_XS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 0;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] =
                        pu1_mem_base + i4_chromaBufSize * 1;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[0] = NULL;
#endif

#if ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] =
                        pu1_mem_base + i4_chromaBufSize * 2;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] =
                        pu1_mem_base + i4_chromaBufSize * 3;
#else
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[0].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[1] = NULL;
                    ps_ctxt->as_cu_prms[1].s_recon_datastore.apv_chroma_recon_bufs[2] = NULL;
#endif

                    break;
                }
                }
            }

            ps_ctxt->as_cu_prms[0].s_recon_datastore.i4_lumaRecon_stride = MAX_CU_SIZE;
            ps_ctxt->as_cu_prms[1].s_recon_datastore.i4_lumaRecon_stride = MAX_CU_SIZE;
            ps_ctxt->as_cu_prms[0].s_recon_datastore.i4_chromaRecon_stride = MAX_CU_SIZE;
            ps_ctxt->as_cu_prms[1].s_recon_datastore.i4_chromaRecon_stride = MAX_CU_SIZE;

        } /* Recon Datastore */

        /****************************************************/
        /****************************************************/
        /* ps_pps->i1_sign_data_hiding_flag  == UNHIDDEN    */
        /* when NO_SBH. else HIDDEN                         */
        /****************************************************/
        /****************************************************/
        /* Zero cbf tool is enabled by default for all presets */
        ps_ctxt->i4_zcbf_rdo_level = ZCBF_ENABLE;

        if(ps_ctxt->i4_quality_preset < IHEVCE_QUALITY_P3)
        {
            ps_ctxt->i4_quant_rounding_level = CU_LEVEL_QUANT_ROUNDING;
            ps_ctxt->i4_chroma_quant_rounding_level = CHROMA_QUANT_ROUNDING;
            ps_ctxt->i4_rdoq_level = ALL_CAND_RDOQ;
            ps_ctxt->i4_sbh_level = ALL_CAND_SBH;
        }
        else if(ps_ctxt->i4_quality_preset == IHEVCE_QUALITY_P3)
        {
            ps_ctxt->i4_quant_rounding_level = FIXED_QUANT_ROUNDING;
            ps_ctxt->i4_chroma_quant_rounding_level = FIXED_QUANT_ROUNDING;
            ps_ctxt->i4_rdoq_level = NO_RDOQ;
            ps_ctxt->i4_sbh_level = NO_SBH;
        }
        else
        {
            ps_ctxt->i4_quant_rounding_level = FIXED_QUANT_ROUNDING;
            ps_ctxt->i4_chroma_quant_rounding_level = FIXED_QUANT_ROUNDING;
            ps_ctxt->i4_rdoq_level = NO_RDOQ;
            ps_ctxt->i4_sbh_level = NO_SBH;
        }

#if DISABLE_QUANT_ROUNDING
        ps_ctxt->i4_quant_rounding_level = FIXED_QUANT_ROUNDING;
        ps_ctxt->i4_chroma_quant_rounding_level = FIXED_QUANT_ROUNDING;
#endif
        /*Disabling RDOQ only when spatial modulation is enabled
                as RDOQ degrades visual quality*/
        if(ps_init_prms->s_config_prms.i4_cu_level_rc & 1)
        {
            ps_ctxt->i4_rdoq_level = NO_RDOQ;
        }

#if DISABLE_RDOQ
        ps_ctxt->i4_rdoq_level = NO_RDOQ;
#endif

#if DISABLE_SBH
        ps_ctxt->i4_sbh_level = NO_SBH;
#endif

        /*Rounding factor calc based on previous cabac states */

        ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[0] = &ps_ctxt->i4_quant_round_4x4[0][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[1] = &ps_ctxt->i4_quant_round_8x8[0][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[2] = &ps_ctxt->i4_quant_round_16x16[0][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_0_1[4] = &ps_ctxt->i4_quant_round_32x32[0][0];

        ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[0] = &ps_ctxt->i4_quant_round_4x4[1][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[1] = &ps_ctxt->i4_quant_round_8x8[1][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[2] = &ps_ctxt->i4_quant_round_16x16[1][0];
        ps_ctxt->pi4_quant_round_factor_cu_ctb_1_2[4] = &ps_ctxt->i4_quant_round_32x32[1][0];

        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[0] = &ps_ctxt->i4_quant_round_cr_4x4[0][0];
        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[1] = &ps_ctxt->i4_quant_round_cr_8x8[0][0];
        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_0_1[2] = &ps_ctxt->i4_quant_round_cr_16x16[0][0];

        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[0] = &ps_ctxt->i4_quant_round_cr_4x4[1][0];
        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[1] = &ps_ctxt->i4_quant_round_cr_8x8[1][0];
        ps_ctxt->pi4_quant_round_factor_cr_cu_ctb_1_2[2] = &ps_ctxt->i4_quant_round_cr_16x16[1][0];

        /****************************************************************************************/
        /* Setting the perform rdoq and sbh flags appropriately                                 */
        /****************************************************************************************/
        {
            /******************************************/
            /* For best cand rdoq and/or sbh          */
            /******************************************/
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq =
                (ps_ctxt->i4_rdoq_level == BEST_CAND_RDOQ);
            /* To do SBH we need the quant and iquant data. This would mean we need to do quantization again, which would mean
            we would have to do RDOQ again.*/
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq =
                ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_rdoq ||
                ((BEST_CAND_SBH == ps_ctxt->i4_sbh_level) &&
                 (ALL_CAND_RDOQ == ps_ctxt->i4_rdoq_level));

            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh =
                (ps_ctxt->i4_sbh_level == BEST_CAND_SBH);

            /* SBH should be performed if
            a) i4_sbh_level is BEST_CAND_SBH.
            b) For all quality presets above medium speed(i.e. high speed and extreme speed) and
            if SBH has to be done because for these presets the quant, iquant and scan coeff
            data are calculated in this function and not during the RDOPT stage*/

            /* RDOQ will change the coefficients. If coefficients are changed, we will have to do sbh again*/
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh =
                ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_best_cand_sbh ||
                ((BEST_CAND_RDOQ == ps_ctxt->i4_rdoq_level) &&
                 (ALL_CAND_SBH == ps_ctxt->i4_sbh_level));

            /******************************************/
            /* For all cand rdoq and/or sbh          */
            /******************************************/
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq =
                (ps_ctxt->i4_rdoq_level == ALL_CAND_RDOQ);
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh =
                (ps_ctxt->i4_sbh_level == ALL_CAND_SBH);
            ps_ctxt->s_rdoq_sbh_ctxt.i4_bit_depth =
                ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth;
        }

        if(!is_hbd_mode)
        {
            if(ps_init_prms->s_coding_tools_prms.i4_use_default_sc_mtx == 1)
            {
                if(ps_ctxt->i4_rdoq_level == NO_RDOQ)
                {
                    ps_ctxt->apf_quant_iquant_ssd[0] =
                        ps_func_selector->ihevc_quant_iquant_ssd_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[2] = ps_func_selector->ihevc_quant_iquant_fptr;
                }
                else
                {
                    ps_ctxt->apf_quant_iquant_ssd[0] =
                        ps_func_selector->ihevc_quant_iquant_ssd_rdoq_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[2] =
                        ps_func_selector->ihevc_quant_iquant_rdoq_fptr;
                }

                /*If coef level RDOQ is enabled, quantization based on corr. error to be done */
                if(ps_ctxt->i4_quant_rounding_level != FIXED_QUANT_ROUNDING)
                {
                    ps_ctxt->apf_quant_iquant_ssd[1] =
                        ps_func_selector->ihevc_q_iq_ssd_var_rnd_fact_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[3] =
                        ps_func_selector->ihevc_q_iq_var_rnd_fact_fptr;
                }
                else
                {
                    ps_ctxt->apf_quant_iquant_ssd[1] =
                        ps_func_selector->ihevc_quant_iquant_ssd_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[3] = ps_func_selector->ihevc_quant_iquant_fptr;
                }
            }
            else if(ps_init_prms->s_coding_tools_prms.i4_use_default_sc_mtx == 0)
            {
                if(ps_ctxt->i4_rdoq_level == NO_RDOQ)
                {
                    ps_ctxt->apf_quant_iquant_ssd[0] =
                        ps_func_selector->ihevc_quant_iquant_ssd_flat_scale_mat_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[2] =
                        ps_func_selector->ihevc_quant_iquant_flat_scale_mat_fptr;
                }
                else
                {
                    ps_ctxt->apf_quant_iquant_ssd[0] =
                        ps_func_selector->ihevc_quant_iquant_ssd_flat_scale_mat_rdoq_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[2] =
                        ps_func_selector->ihevc_quant_iquant_flat_scale_mat_rdoq_fptr;
                }

                /*If coef level RDOQ is enabled, quantization based on corr. error to be done */
                if(ps_ctxt->i4_quant_rounding_level != FIXED_QUANT_ROUNDING)
                {
                    ps_ctxt->apf_quant_iquant_ssd[1] =
                        ps_func_selector->ihevc_q_iq_ssd_flat_scale_mat_var_rnd_fact_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[3] =
                        ps_func_selector->ihevc_q_iq_flat_scale_mat_var_rnd_fact_fptr;
                }
                else
                {
                    ps_ctxt->apf_quant_iquant_ssd[1] =
                        ps_func_selector->ihevc_quant_iquant_ssd_flat_scale_mat_fptr;
                    ps_ctxt->apf_quant_iquant_ssd[3] =
                        ps_func_selector->ihevc_quant_iquant_flat_scale_mat_fptr;
                }
            }

            ps_ctxt->s_sao_ctxt_t.apf_sao_luma[0] =
                ps_func_selector->ihevc_sao_edge_offset_class0_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_luma[1] =
                ps_func_selector->ihevc_sao_edge_offset_class1_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_luma[2] =
                ps_func_selector->ihevc_sao_edge_offset_class2_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_luma[3] =
                ps_func_selector->ihevc_sao_edge_offset_class3_fptr;

            ps_ctxt->s_sao_ctxt_t.apf_sao_chroma[0] =
                ps_func_selector->ihevc_sao_edge_offset_class0_chroma_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_chroma[1] =
                ps_func_selector->ihevc_sao_edge_offset_class1_chroma_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_chroma[2] =
                ps_func_selector->ihevc_sao_edge_offset_class2_chroma_fptr;
            ps_ctxt->s_sao_ctxt_t.apf_sao_chroma[3] =
                ps_func_selector->ihevc_sao_edge_offset_class3_chroma_fptr;

            ps_ctxt->apf_it_recon[0] = ps_func_selector->ihevc_itrans_recon_4x4_ttype1_fptr;
            ps_ctxt->apf_it_recon[1] = ps_func_selector->ihevc_itrans_recon_4x4_fptr;
            ps_ctxt->apf_it_recon[2] = ps_func_selector->ihevc_itrans_recon_8x8_fptr;
            ps_ctxt->apf_it_recon[3] = ps_func_selector->ihevc_itrans_recon_16x16_fptr;
            ps_ctxt->apf_it_recon[4] = ps_func_selector->ihevc_itrans_recon_32x32_fptr;

            ps_ctxt->apf_chrm_it_recon[0] = ps_func_selector->ihevc_chroma_itrans_recon_4x4_fptr;
            ps_ctxt->apf_chrm_it_recon[1] = ps_func_selector->ihevc_chroma_itrans_recon_8x8_fptr;
            ps_ctxt->apf_chrm_it_recon[2] = ps_func_selector->ihevc_chroma_itrans_recon_16x16_fptr;

            ps_ctxt->apf_resd_trns[0] = ps_func_selector->ihevc_resi_trans_4x4_ttype1_fptr;
            ps_ctxt->apf_resd_trns[1] = ps_func_selector->ihevc_resi_trans_4x4_fptr;
            ps_ctxt->apf_resd_trns[2] = ps_func_selector->ihevc_resi_trans_8x8_fptr;
            ps_ctxt->apf_resd_trns[3] = ps_func_selector->ihevc_resi_trans_16x16_fptr;
            ps_ctxt->apf_resd_trns[4] = ps_func_selector->ihevc_resi_trans_32x32_fptr;

            ps_ctxt->apf_chrm_resd_trns[0] = ps_func_selector->ihevc_resi_trans_4x4_fptr;
            ps_ctxt->apf_chrm_resd_trns[1] = ps_func_selector->ihevc_resi_trans_8x8_fptr;
            ps_ctxt->apf_chrm_resd_trns[2] = ps_func_selector->ihevc_resi_trans_16x16_fptr;

            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_0] =
                ps_func_selector->ihevc_intra_pred_luma_planar_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_1] = ps_func_selector->ihevc_intra_pred_luma_dc_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_2] =
                ps_func_selector->ihevc_intra_pred_luma_mode2_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_3TO9] =
                ps_func_selector->ihevc_intra_pred_luma_mode_3_to_9_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_10] =
                ps_func_selector->ihevc_intra_pred_luma_horz_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_11TO17] =
                ps_func_selector->ihevc_intra_pred_luma_mode_11_to_17_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_18_34] =
                ps_func_selector->ihevc_intra_pred_luma_mode_18_34_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_19TO25] =
                ps_func_selector->ihevc_intra_pred_luma_mode_19_to_25_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_26] = ps_func_selector->ihevc_intra_pred_luma_ver_fptr;
            ps_ctxt->apf_lum_ip[IP_FUNC_MODE_27TO33] =
                ps_func_selector->ihevc_intra_pred_luma_mode_27_to_33_fptr;

            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_0] =
                ps_func_selector->ihevc_intra_pred_chroma_planar_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_1] =
                ps_func_selector->ihevc_intra_pred_chroma_dc_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_2] =
                ps_func_selector->ihevc_intra_pred_chroma_mode2_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_3TO9] =
                ps_func_selector->ihevc_intra_pred_chroma_mode_3_to_9_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_10] =
                ps_func_selector->ihevc_intra_pred_chroma_horz_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_11TO17] =
                ps_func_selector->ihevc_intra_pred_chroma_mode_11_to_17_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_18_34] =
                ps_func_selector->ihevc_intra_pred_chroma_mode_18_34_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_19TO25] =
                ps_func_selector->ihevc_intra_pred_chroma_mode_19_to_25_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_26] =
                ps_func_selector->ihevc_intra_pred_chroma_ver_fptr;
            ps_ctxt->apf_chrm_ip[IP_FUNC_MODE_27TO33] =
                ps_func_selector->ihevc_intra_pred_chroma_mode_27_to_33_fptr;

            ps_ctxt->apf_chrm_resd_trns_had[0] =
                (pf_res_trans_luma_had_chroma)ps_ctxt->s_cmn_opt_func.pf_chroma_HAD_4x4_8bit;
            ps_ctxt->apf_chrm_resd_trns_had[1] =
                (pf_res_trans_luma_had_chroma)ps_ctxt->s_cmn_opt_func.pf_chroma_HAD_8x8_8bit;
            ps_ctxt->apf_chrm_resd_trns_had[2] =
                (pf_res_trans_luma_had_chroma)ps_ctxt->s_cmn_opt_func.pf_chroma_HAD_16x16_8bit;
        }

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

        /* Not recomputing Luma pred-data and header data for any preset now */
        ps_ctxt->s_cu_final_recon_flags.u1_eval_header_data = 0;
        ps_ctxt->s_cu_final_recon_flags.u1_eval_luma_pred_data = 0;
        ps_ctxt->s_cu_final_recon_flags.u1_eval_recon_data = 1;

        switch(ps_ctxt->i4_quality_preset)
        {
        case IHEVCE_QUALITY_P0:
        {
            ps_ctxt->i4_max_merge_candidates = 5;
            ps_ctxt->i4_use_satd_for_merge_eval = 1;
            ps_ctxt->u1_use_top_at_ctb_boundary = 1;
            ps_ctxt->u1_use_early_cbf_data = 0;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt = ENABLE_CHROMA_RDOPT_EVAL_IN_PQ;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd =
                ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_PQ;

            break;
        }
        case IHEVCE_QUALITY_P2:
        {
            ps_ctxt->i4_max_merge_candidates = 5;
            ps_ctxt->i4_use_satd_for_merge_eval = 1;
            ps_ctxt->u1_use_top_at_ctb_boundary = 1;
            ps_ctxt->u1_use_early_cbf_data = 0;

            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt = ENABLE_CHROMA_RDOPT_EVAL_IN_HQ;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd =
                ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HQ;

            break;
        }
        case IHEVCE_QUALITY_P3:
        {
            ps_ctxt->i4_max_merge_candidates = 3;
            ps_ctxt->i4_use_satd_for_merge_eval = 1;
            ps_ctxt->u1_use_top_at_ctb_boundary = 0;

            ps_ctxt->u1_use_early_cbf_data = 0;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt = ENABLE_CHROMA_RDOPT_EVAL_IN_MS;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd =
                ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_MS;

            break;
        }
        case IHEVCE_QUALITY_P4:
        {
            ps_ctxt->i4_max_merge_candidates = 2;
            ps_ctxt->i4_use_satd_for_merge_eval = 1;
            ps_ctxt->u1_use_top_at_ctb_boundary = 0;
            ps_ctxt->u1_use_early_cbf_data = 0;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt = ENABLE_CHROMA_RDOPT_EVAL_IN_HS;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd =
                ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_HS;

            break;
        }
        case IHEVCE_QUALITY_P5:
        {
            ps_ctxt->i4_max_merge_candidates = 2;
            ps_ctxt->i4_use_satd_for_merge_eval = 0;
            ps_ctxt->u1_use_top_at_ctb_boundary = 0;
            ps_ctxt->u1_use_early_cbf_data = 0;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt = ENABLE_CHROMA_RDOPT_EVAL_IN_XS;
            ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd =
                ENABLE_ADDITIONAL_CHROMA_MODES_EVAL_IN_XS;

            break;
        }
        case IHEVCE_QUALITY_P6:
        {
            ps_ctxt->i4_max_merge_candidates = 2;
            ps_ctxt->i4_use_satd_for_merge_eval = 0;
            ps_ctxt->u1_use_top_at_ctb_boundary = 0;
            ps_ctxt->u1_use_early_cbf_data = EARLY_CBF_ON;
            break;
        }
        default:
        {
            ASSERT(0);
        }
        }

#if DISABLE_SKIP_AND_MERGE_EVAL
        ps_ctxt->i4_max_merge_candidates = 0;
#endif

        ps_ctxt->s_cu_final_recon_flags.u1_eval_chroma_pred_data =
            !ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_rdopt;

        /*initialize memory for RC related parameters required/populated by enc_loop */
        /* the allocated memory is distributed as follows assuming encoder is running for 3 bit-rate instnaces
        |-------|-> Thread 0, instance 0
        |       |
        |       |
        |       |
        |-------|-> thread 0, instance 1
        |       |
        |       |
        |       |
        |-------|-> thread 0, intance 2
        |       |
        |       |
        |       |
        |-------|-> thread 1, instance 0
        |       |
        |       |
        |       |
        |-------|-> thread 1, instance 1
        |       |
        |       |
        |       |
        |-------|-> thread 1, instance 2
        ...         ...

        Each theard will collate the data corresponding to the bit-rate instnace it's running at the appropriate place.
        Finally, one thread will become master and collate the data from all the threads */
        for(i4_enc_frm_id = 0; i4_enc_frm_id < i4_num_enc_loop_frm_pllel; i4_enc_frm_id++)
        {
            for(i = 0; i < i4_num_bitrate_inst; i++)
            {
                ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i] = ps_enc_loop_rc_params;
                ps_enc_loop_rc_params++;
            }
        }
        /* Non-Luma modes for Chroma are evaluated only in HIGH QUALITY preset */

#if !ENABLE_SEPARATE_LUMA_CHROMA_INTRA_MODE
        ps_ctxt->s_chroma_rdopt_ctxt.u1_eval_chrm_satd = 0;
#endif

        ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[TU_EQ_CU].i4_iq_buff_stride =
            MAX_TU_SIZE;
        ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[TU_EQ_CU_DIV2].i4_iq_buff_stride =
            MAX_TU_SIZE;
        /*Multiplying by two to account for interleaving of cb and cr*/
        ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[TU_EQ_CU].i4_pred_stride = MAX_TU_SIZE
                                                                                       << 1;
        ps_ctxt->s_chroma_rdopt_ctxt.as_chr_intra_satd_ctxt[TU_EQ_CU_DIV2].i4_pred_stride =
            MAX_TU_SIZE << 1;

        /*     Memory for a frame level memory to store tile-id                  */
        /*              corresponding to each CTB of frame                       */
        ps_ctxt->pi4_offset_for_last_cu_qp = &ps_master_ctxt->ai4_offset_for_last_cu_qp[0];

        ps_ctxt->i4_qp_mod = ps_init_prms->s_config_prms.i4_cu_level_rc & 1;
        /* psy rd strength is a run time parametr control by bit field 5-7 in the VQET field.*/
        /* we disable psyrd if the the psy strength is zero or the BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER field is not set */
        if(ps_init_prms->s_coding_tools_prms.i4_vqet &
           (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER))
        {
            UWORD32 psy_strength;
            UWORD32 psy_strength_mask =
                224;  // only bits 5,6,7 are ones. These three bits represent the psy strength
            psy_strength = ps_init_prms->s_coding_tools_prms.i4_vqet & psy_strength_mask;
            ps_ctxt->u1_enable_psyRDOPT = 1;
            ps_ctxt->u4_psy_strength = psy_strength >> BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1;
            if(psy_strength == 0)
            {
                ps_ctxt->u1_enable_psyRDOPT = 0;
                ps_ctxt->u4_psy_strength = 0;
            }
        }

        ps_ctxt->u1_is_stasino_enabled =
            ((ps_init_prms->s_coding_tools_prms.i4_vqet &
              (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
             (ps_init_prms->s_coding_tools_prms.i4_vqet &
              (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)));

        ps_ctxt->u1_max_inter_tr_depth = ps_init_prms->s_config_prms.i4_max_tr_tree_depth_nI;
        ps_ctxt->u1_max_intra_tr_depth = ps_init_prms->s_config_prms.i4_max_tr_tree_depth_I;
        ps_ctxt++;
    }
    /* Store Tile params base into EncLoop Master context */
    ps_master_ctxt->pv_tile_params_base = (void *)ps_tile_params_base;

    if(1 == ps_tile_params_base->i4_tiles_enabled_flag)
    {
        i4_num_tile_cols = ps_tile_params_base->i4_num_tile_cols;
    }

    /* Updating  ai4_offset_for_last_cu_qp[] array for all tile-colums of frame */
    /* Loop over all tile-cols in frame */
    for(ctr = 0; ctr < i4_num_tile_cols; ctr++)
    {
        WORD32 i4_tile_col_wd_in_ctb_unit =
            (ps_tile_params_base + ctr)->i4_curr_tile_wd_in_ctb_unit;
        WORD32 offset_x;

        if(ctr == (i4_num_tile_cols - 1))
        { /* Last tile-row of frame */
            WORD32 min_cu_size = 1 << ps_init_prms->s_config_prms.i4_min_log2_cu_size;

            WORD32 cu_aligned_pic_wd =
                ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
                SET_CTB_ALIGN(
                    ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width,
                    min_cu_size);

            WORD32 last_hz_ctb_wd = MAX_CTB_SIZE - (u4_width - cu_aligned_pic_wd);

            offset_x = (i4_tile_col_wd_in_ctb_unit - 1) * MAX_CTB_SIZE;
            offset_x += last_hz_ctb_wd;
        }
        else
        { /* Not the last tile-row of frame */
            offset_x = (i4_tile_col_wd_in_ctb_unit)*MAX_CTB_SIZE;
        }

        offset_x /= 4;
        offset_x -= 1;

        ps_master_ctxt->ai4_offset_for_last_cu_qp[ctr] = offset_x;
    }

    n_tabs = NUM_ENC_LOOP_MEM_RECS;

    /*store num bit-rate instances in the master context */
    ps_master_ctxt->i4_num_bitrates = i4_num_bitrate_inst;
    ps_master_ctxt->i4_num_enc_loop_frm_pllel = i4_num_enc_loop_frm_pllel;
    /*************************************************************************/
    /* --- EncLoop Deblock and SAO sync Dep Mngr Mem init --                         */
    /*************************************************************************/
    {
        WORD32 count;
        WORD32 num_vert_units, num_blks_in_row;
        WORD32 ht = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
        WORD32 wd = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;

        ihevce_enc_loop_dblk_get_prms_dep_mngr(ht, &num_vert_units);
        ihevce_enc_loop_dblk_get_prms_dep_mngr(wd, &num_blks_in_row);
        ASSERT(num_vert_units > 0);
        ASSERT(num_blks_in_row > 0);

        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(i = 0; i < i4_num_bitrate_inst; i++)
            {
                ps_master_ctxt->aapv_dep_mngr_enc_loop_dblk[count][i] = ihevce_dmgr_init(
                    &ps_mem_tab[n_tabs],
                    pv_osal_handle,
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    num_blks_in_row,
                    i4_num_tile_cols, /* Number of Col Tiles */
                    i4_num_proc_thrds,
                    0 /*Sem Disabled*/
                );

                n_tabs += ihevce_dmgr_get_num_mem_recs();
            }
        }

        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(i = 0; i < i4_num_bitrate_inst; i++)
            {
                ps_master_ctxt->aapv_dep_mngr_enc_loop_sao[count][i] = ihevce_dmgr_init(
                    &ps_mem_tab[n_tabs],
                    pv_osal_handle,
                    DEP_MNGR_ROW_ROW_SYNC,
                    num_vert_units,
                    num_blks_in_row,
                    i4_num_tile_cols, /* Number of Col Tiles */
                    i4_num_proc_thrds,
                    0 /*Sem Disabled*/
                );

                n_tabs += ihevce_dmgr_get_num_mem_recs();
            }
        }
    }
    /*************************************************************************/
    /* --- EncLoop Top-Right CU synnc Dep Mngr Mem init --                   */
    /*************************************************************************/
    {
        WORD32 count;
        WORD32 num_vert_units, num_blks_in_row;
        WORD32 ht = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;
        WORD32 wd = ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;

        WORD32 i4_sem = 0;

        if(ps_init_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset >=
           IHEVCE_QUALITY_P4)
            i4_sem = 0;
        else
            i4_sem = 1;
        ihevce_enc_loop_dblk_get_prms_dep_mngr(ht, &num_vert_units);
        /* For Top-Right CU sync, adding one more CTB since value updation */
        /* happens in that way for the last CTB in the row                 */
        num_blks_in_row = wd + SET_CTB_ALIGN(wd, MAX_CU_SIZE);
        num_blks_in_row += MAX_CTB_SIZE;

        ASSERT(num_vert_units > 0);
        ASSERT(num_blks_in_row > 0);

        for(count = 0; count < i4_num_enc_loop_frm_pllel; count++)
        {
            for(i = 0; i < i4_num_bitrate_inst; i++)
            {
                /* For ES/HS, CU level updates uses spin-locks than semaphore */
                {
                    ps_master_ctxt->aapv_dep_mngr_enc_loop_cu_top_right[count][i] =
                        ihevce_dmgr_init(
                            &ps_mem_tab[n_tabs],
                            pv_osal_handle,
                            DEP_MNGR_ROW_ROW_SYNC,
                            num_vert_units,
                            num_blks_in_row,
                            i4_num_tile_cols, /* Number of Col Tiles */
                            i4_num_proc_thrds,
                            i4_sem /*Sem Disabled*/
                        );
                }
                n_tabs += ihevce_dmgr_get_num_mem_recs();
            }
        }
    }

    for(i = 1; i < 5; i++)
    {
        WORD32 i4_log2_trans_size = i + 1;
        WORD32 i4_bit_depth = ps_init_prms->s_tgt_lyr_prms.i4_internal_bit_depth;

        ga_trans_shift[i] = (MAX_TR_DYNAMIC_RANGE - i4_bit_depth - i4_log2_trans_size) << 1;
    }

    ga_trans_shift[0] = ga_trans_shift[1];

    /* return the handle to caller */
    return ((void *)ps_master_ctxt);
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_reg_sem_hdls \endif
*
* \brief
*    Intialization for ENC_LOOP context state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] ppv_sem_hdls : Array of semaphore handles
* \param[in] i4_num_proc_thrds : Number of processing threads
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_reg_sem_hdls(
    void *pv_enc_loop_ctxt, void **ppv_sem_hdls, WORD32 i4_num_proc_thrds)
{
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt;
    WORD32 i, enc_frm_id;

    ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_enc_loop_ctxt;

    /*************************************************************************/
    /* --- EncLoop Deblock and SAO sync Dep Mngr reg Semaphores --                   */
    /*************************************************************************/
    for(enc_frm_id = 0; enc_frm_id < ps_master_ctxt->i4_num_enc_loop_frm_pllel; enc_frm_id++)
    {
        for(i = 0; i < ps_master_ctxt->i4_num_bitrates; i++)
        {
            ihevce_dmgr_reg_sem_hdls(
                ps_master_ctxt->aapv_dep_mngr_enc_loop_dblk[enc_frm_id][i],
                ppv_sem_hdls,
                i4_num_proc_thrds);
        }
    }

    for(enc_frm_id = 0; enc_frm_id < ps_master_ctxt->i4_num_enc_loop_frm_pllel; enc_frm_id++)
    {
        for(i = 0; i < ps_master_ctxt->i4_num_bitrates; i++)
        {
            ihevce_dmgr_reg_sem_hdls(
                ps_master_ctxt->aapv_dep_mngr_enc_loop_sao[enc_frm_id][i],
                ppv_sem_hdls,
                i4_num_proc_thrds);
        }
    }

    /*************************************************************************/
    /* --- EncLoop Top-Right CU synnc Dep Mngr reg Semaphores --             */
    /*************************************************************************/
    for(enc_frm_id = 0; enc_frm_id < ps_master_ctxt->i4_num_enc_loop_frm_pllel; enc_frm_id++)
    {
        for(i = 0; i < ps_master_ctxt->i4_num_bitrates; i++)
        {
            ihevce_dmgr_reg_sem_hdls(
                ps_master_ctxt->aapv_dep_mngr_enc_loop_cu_top_right[enc_frm_id][i],
                ppv_sem_hdls,
                i4_num_proc_thrds);
        }
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_delete \endif
*
* \brief
*    Destroy EncLoop module
* Note : Only Destroys the resources allocated in the module like
*   semaphore,etc. Memory free is done Separately using memtabs
*
* \param[in] pv_me_ctxt : pointer to EncLoop ctxt
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_delete(void *pv_enc_loop_ctxt)
{
    ihevce_enc_loop_master_ctxt_t *ps_enc_loop_ctxt;
    WORD32 ctr, enc_frm_id;

    ps_enc_loop_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_enc_loop_ctxt;

    for(enc_frm_id = 0; enc_frm_id < ps_enc_loop_ctxt->i4_num_enc_loop_frm_pllel; enc_frm_id++)
    {
        for(ctr = 0; ctr < ps_enc_loop_ctxt->i4_num_bitrates; ctr++)
        {
            /* --- EncLoop Deblock sync Dep Mngr Delete --*/
            ihevce_dmgr_del(ps_enc_loop_ctxt->aapv_dep_mngr_enc_loop_dblk[enc_frm_id][ctr]);
            /* --- EncLoop Sao sync Dep Mngr Delete --*/
            ihevce_dmgr_del(ps_enc_loop_ctxt->aapv_dep_mngr_enc_loop_sao[enc_frm_id][ctr]);
            /* --- EncLoop Top-Right CU sync Dep Mngr Delete --*/
            ihevce_dmgr_del(ps_enc_loop_ctxt->aapv_dep_mngr_enc_loop_cu_top_right[enc_frm_id][ctr]);
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_dep_mngr_frame_reset \endif
*
* \brief
*    Frame level Reset for the Dependency Mngrs local to EncLoop.,
*    ie CU_TopRight and Dblk
*
* \param[in] pv_enc_loop_ctxt       : Enc_loop context pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_dep_mngr_frame_reset(void *pv_enc_loop_ctxt, WORD32 enc_frm_id)
{
    WORD32 ctr, frame_id;
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt;

    ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_enc_loop_ctxt;

    if(1 == ps_master_ctxt->i4_num_enc_loop_frm_pllel)
    {
        frame_id = 0;
    }
    else
    {
        frame_id = enc_frm_id;
    }

    for(ctr = 0; ctr < ps_master_ctxt->i4_num_bitrates; ctr++)
    {
        /* Dep. Mngr : Reset the num ctb Deblocked in every row  for ENC sync */
        ihevce_dmgr_rst_row_row_sync(ps_master_ctxt->aapv_dep_mngr_enc_loop_dblk[frame_id][ctr]);

        /* Dep. Mngr : Reset the num SAO ctb in every row  for ENC sync */
        ihevce_dmgr_rst_row_row_sync(ps_master_ctxt->aapv_dep_mngr_enc_loop_sao[frame_id][ctr]);

        /* Dep. Mngr : Reset the TopRight CU Processed in every row  for ENC sync */
        ihevce_dmgr_rst_row_row_sync(
            ps_master_ctxt->aapv_dep_mngr_enc_loop_cu_top_right[frame_id][ctr]);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_enc_loop_frame_init \endif
*
* \brief
*    Frame level init of enocde loop function .
*
* \param[in] pv_enc_loop_ctxt           : Enc_loop context pointer
* \param[in] pi4_cu_processed           : ptr to cur frame cu process in pix.
* \param[in] aps_ref_list               : ref pic list for the current frame
* \param[in] ps_slice_hdr               : ptr to current slice header params
* \param[in] ps_pps                     : ptr to active pps params
* \param[in] ps_sps                     : ptr to active sps params
* \param[in] ps_vps                     : ptr to active vps params


* \param[in] i1_weighted_pred_flag      : weighted pred enable flag (unidir)
* \param[in] i1_weighted_bipred_flag    : weighted pred enable flag (bidir)
* \param[in] log2_luma_wght_denom       : down shift factor for weighted pred of luma
* \param[in] log2_chroma_wght_denom       : down shift factor for weighted pred of chroma
* \param[in] cur_poc                    : currennt frame poc
* \param[in] i4_bitrate_instance_num    : number indicating the instance of bit-rate for multi-rate encoder
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_frame_init(
    void *pv_enc_loop_ctxt,
    WORD32 i4_frm_qp,
    recon_pic_buf_t *(*aps_ref_list)[HEVCE_MAX_REF_PICS * 2],
    recon_pic_buf_t *ps_frm_recon,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    vps_t *ps_vps,
    WORD8 i1_weighted_pred_flag,
    WORD8 i1_weighted_bipred_flag,
    WORD32 log2_luma_wght_denom,
    WORD32 log2_chroma_wght_denom,
    WORD32 cur_poc,
    WORD32 i4_display_num,
    enc_ctxt_t *ps_enc_ctxt,
    me_enc_rdopt_ctxt_t *ps_curr_inp_prms,
    WORD32 i4_bitrate_instance_num,
    WORD32 i4_thrd_id,
    WORD32 i4_enc_frm_id,
    WORD32 i4_num_bitrates,
    WORD32 i4_quality_preset,
    void *pv_dep_mngr_encloop_dep_me)
{
    /* local variables */
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt;
    ihevce_enc_loop_ctxt_t *ps_ctxt;
    WORD32 chroma_qp_offset, i4_div_factor;
    WORD8 i1_slice_type = ps_slice_hdr->i1_slice_type;
    WORD8 i1_strong_intra_smoothing_enable_flag = ps_sps->i1_strong_intra_smoothing_enable_flag;

    /* ENC_LOOP master state structure */
    ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_enc_loop_ctxt;

    /* Nithya: Store the current POC in the slice header */
    ps_slice_hdr->i4_abs_pic_order_cnt = cur_poc;

    /* Update the POC list of the current frame to the recon buffer */
    if(ps_slice_hdr->i1_num_ref_idx_l0_active != 0)
    {
        int i4_i;
        for(i4_i = 0; i4_i < ps_slice_hdr->i1_num_ref_idx_l0_active; i4_i++)
        {
            ps_frm_recon->ai4_col_l0_poc[i4_i] = aps_ref_list[0][i4_i]->i4_poc;
        }
    }
    if(ps_slice_hdr->i1_num_ref_idx_l1_active != 0)
    {
        int i4_i;
        for(i4_i = 0; i4_i < ps_slice_hdr->i1_num_ref_idx_l1_active; i4_i++)
        {
            ps_frm_recon->ai4_col_l1_poc[i4_i] = aps_ref_list[1][i4_i]->i4_poc;
        }
    }

    /* loop over all the threads */
    // for(ctr = 0; ctr < ps_master_ctxt->i4_num_proc_thrds; ctr++)
    {
        /* ENC_LOOP state structure */
        ps_ctxt = ps_master_ctxt->aps_enc_loop_thrd_ctxt[i4_thrd_id];

        /* SAO ctxt structure initialization*/
        ps_ctxt->s_sao_ctxt_t.ps_pps = ps_pps;
        ps_ctxt->s_sao_ctxt_t.ps_sps = ps_sps;
        ps_ctxt->s_sao_ctxt_t.ps_slice_hdr = ps_slice_hdr;

        /*bit-rate instance number for Multi-bitrate (MBR) encode */
        ps_ctxt->i4_bitrate_instance_num = i4_bitrate_instance_num;
        ps_ctxt->i4_num_bitrates = i4_num_bitrates;
        ps_ctxt->i4_chroma_format = ps_enc_ctxt->ps_stat_prms->s_src_prms.i4_chr_format;
        ps_ctxt->i4_is_first_query = 1;
        ps_ctxt->i4_is_ctb_qp_modified = 0;

        /* enc_frm_id for multiframe encode */

        if(1 == ps_enc_ctxt->s_multi_thrd.i4_num_enc_loop_frm_pllel)
        {
            ps_ctxt->i4_enc_frm_id = 0;
            i4_enc_frm_id = 0;
        }
        else
        {
            ps_ctxt->i4_enc_frm_id = i4_enc_frm_id;
        }

        /*Initialize the sub pic rc buf appropriately */

        /*Set the thrd id flag */
        ps_enc_ctxt->s_multi_thrd
            .ai4_thrd_id_valid_flag[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 1;

        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_ipe_sad[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_me_sad[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;

        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_l0_ipe_sad[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_act_factor[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;

        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_bits_consumed[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_acc_bits_consumed[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_acc_bits_mul_qs_consumed[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_hdr_bits_consumed[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd
            .ai8_nctb_mpm_bits_consumed[i4_enc_frm_id][i4_bitrate_instance_num][i4_thrd_id] = 0;
        ps_enc_ctxt->s_multi_thrd.ai4_prev_chunk_qp[i4_enc_frm_id][i4_bitrate_instance_num] =
            i4_frm_qp;

        /*Frame level data for Sub Pic rc is initalized here */
        /*Can be sent once per frame*/
        {
            WORD32 i4_tot_frame_ctb = ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_vert *
                                      ps_enc_ctxt->s_frm_ctb_prms.i4_num_ctbs_horz;

            /*Accumalated bits of all cu for required CTBS estimated during RDO evaluation*/
            ps_ctxt->u4_total_cu_bits = 0;
            ps_ctxt->u4_total_cu_hdr_bits = 0;

            ps_ctxt->u4_cu_tot_bits_into_qscale = 0;
            ps_ctxt->u4_cu_tot_bits = 0;
            ps_ctxt->u4_total_cu_bits_mul_qs = 0;
            ps_ctxt->i4_display_num = i4_display_num;
            ps_ctxt->i4_sub_pic_level_rc = ps_enc_ctxt->s_multi_thrd.i4_in_frame_rc_enabled;
            /*The Qscale is to be generated every 10th of total frame ctb is completed */
            //ps_ctxt->i4_num_ctb_for_out_scale = (10 * i4_tot_frame_ctb)/100 ;
            ps_ctxt->i4_num_ctb_for_out_scale = (UPDATE_QP_AT_CTB * i4_tot_frame_ctb) / 100;

            ps_ctxt->i4_cu_qp_sub_pic_rc = (1 << QP_LEVEL_MOD_ACT_FACTOR);
            /*Sub Pic RC frame level params */
            ps_ctxt->i8_frame_l1_ipe_sad =
                ps_curr_inp_prms->ps_curr_inp->s_rc_lap_out.i8_raw_pre_intra_sad;
            ps_ctxt->i8_frame_l0_ipe_satd =
                ps_curr_inp_prms->ps_curr_inp->s_lap_out.i8_frame_l0_acc_satd;
            ps_ctxt->i8_frame_l1_me_sad =
                ps_curr_inp_prms->ps_curr_inp->s_rc_lap_out.i8_raw_l1_coarse_me_sad;
            ps_ctxt->i8_frame_l1_activity_fact =
                ps_curr_inp_prms->ps_curr_inp->s_lap_out.i8_frame_level_activity_fact;
            if(ps_ctxt->i4_sub_pic_level_rc)
            {
                ASSERT(
                    ps_curr_inp_prms->ps_curr_inp->s_lap_out
                        .ai4_frame_bits_estimated[ps_ctxt->i4_bitrate_instance_num] != 0);

                ps_ctxt->ai4_frame_bits_estimated[ps_ctxt->i4_enc_frm_id]
                                                 [ps_ctxt->i4_bitrate_instance_num] =
                    ps_curr_inp_prms->ps_curr_inp->s_lap_out
                        .ai4_frame_bits_estimated[ps_ctxt->i4_bitrate_instance_num];
            }
            //ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_scene_type = 1;

            ps_ctxt->i4_is_I_scenecut =
                ((ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_scene_type == SCENE_TYPE_SCENE_CUT) &&
                 (ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_pic_type == IV_IDR_FRAME ||
                  ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_pic_type == IV_I_FRAME));

            ps_ctxt->i4_is_non_I_scenecut =
                ((ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_scene_type == SCENE_TYPE_SCENE_CUT) &&
                 (ps_ctxt->i4_is_I_scenecut == 0));

            /*ps_ctxt->i4_is_I_only_scd = ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_is_I_only_scd;
            ps_ctxt->i4_is_non_I_scd = ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_is_non_I_scd;*/
            ps_ctxt->i4_is_model_valid =
                ps_curr_inp_prms->ps_curr_inp->s_rc_lap_out.i4_is_model_valid;
        }
        /* cb and cr offsets are assumed to be same */
        chroma_qp_offset = ps_slice_hdr->i1_slice_cb_qp_offset + ps_pps->i1_pic_cb_qp_offset;

        /* assumption of cb = cr qp */
        ASSERT(ps_slice_hdr->i1_slice_cb_qp_offset == ps_slice_hdr->i1_slice_cr_qp_offset);
        ASSERT(ps_pps->i1_pic_cb_qp_offset == ps_pps->i1_pic_cr_qp_offset);

        ps_ctxt->u1_is_input_data_hbd = (ps_sps->i1_bit_depth_luma_minus8 > 0);

        ps_ctxt->u1_bit_depth = ps_sps->i1_bit_depth_luma_minus8 + 8;

        ps_ctxt->s_mc_ctxt.i4_bit_depth = ps_ctxt->u1_bit_depth;
        ps_ctxt->s_mc_ctxt.u1_chroma_array_type = ps_ctxt->u1_chroma_array_type;

        /*remember chroma qp offset as qp related parameters are calculated at CU level*/
        ps_ctxt->i4_chroma_qp_offset = chroma_qp_offset;
        ps_ctxt->i1_cu_qp_delta_enable = ps_pps->i1_cu_qp_delta_enabled_flag;
        ps_ctxt->i1_entropy_coding_sync_enabled_flag = ps_pps->i1_entropy_coding_sync_enabled_flag;

        ps_ctxt->i4_is_ref_pic = ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_is_ref_pic;
        ps_ctxt->i4_temporal_layer = ps_curr_inp_prms->ps_curr_inp->s_lap_out.i4_temporal_lyr_id;
        ps_ctxt->i4_use_const_lamda_modifier = USE_CONSTANT_LAMBDA_MODIFIER;
        ps_ctxt->i4_use_const_lamda_modifier =
            ps_ctxt->i4_use_const_lamda_modifier ||
            ((ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
              (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
             ((ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
               (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)) ||
              (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
               (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1)) ||
              (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
               (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_2)) ||
              (ps_enc_ctxt->ps_stat_prms->s_coding_tools_prms.i4_vqet &
               (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_3))));

        {
            ps_ctxt->f_i_pic_lamda_modifier =
                ps_curr_inp_prms->ps_curr_inp->s_lap_out.f_i_pic_lamda_modifier;
        }

        ps_ctxt->i4_frame_qp = i4_frm_qp;
        ps_ctxt->i4_frame_mod_qp = i4_frm_qp;
        ps_ctxt->i4_cu_qp = i4_frm_qp;
        ps_ctxt->i4_prev_cu_qp = i4_frm_qp;
        ps_ctxt->i4_chrm_cu_qp =
            (ps_ctxt->u1_chroma_array_type == 2)
                ? MIN(i4_frm_qp + chroma_qp_offset, 51)
                : gai1_ihevc_chroma_qp_scale[i4_frm_qp + chroma_qp_offset + MAX_QP_BD_OFFSET];

        ps_ctxt->i4_cu_qp_div6 = (i4_frm_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) / 6;
        i4_div_factor = (i4_frm_qp + 3) / 6;
        i4_div_factor = CLIP3(i4_div_factor, 3, 6);
        ps_ctxt->i4_cu_qp_mod6 = (i4_frm_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) % 6;

        ps_ctxt->i4_chrm_cu_qp_div6 =
            (ps_ctxt->i4_chrm_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) / 6;
        ps_ctxt->i4_chrm_cu_qp_mod6 =
            (ps_ctxt->i4_chrm_cu_qp + (6 * (ps_ctxt->u1_bit_depth - 8))) % 6;

#define INTER_RND_QP_BY_6
#ifdef INTER_RND_QP_BY_6

        { /*1/6 rounding for 8 bit b frames*/
            ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER] = 85
                /*((1 << QUANT_ROUND_FACTOR_Q) / 6)*/;
        }
#else
        /* quant factor without RDOQ is 1/6th of shift for inter : like in H264 */
        ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER] = (1 << QUANT_ROUND_FACTOR_Q) / 3;
#endif

        if(ISLICE == i1_slice_type)
        {
            /* quant factor without RDOQ is 1/3rd of shift for intra : like in H264 */
            ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTRA] = 171
                /*((1 << QUANT_ROUND_FACTOR_Q) / 6)*/;
        }
        else
        {
            /* quant factor without RDOQ is 1/6th of shift for intra in inter pic */
            ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTRA] =
                ps_ctxt->i4_quant_rnd_factor[PRED_MODE_INTER];
            /* (1 << QUANT_ROUND_FACTOR_Q) / 6; */
        }

        ps_ctxt->i1_strong_intra_smoothing_enable_flag = i1_strong_intra_smoothing_enable_flag;

        ps_ctxt->i1_slice_type = i1_slice_type;

        /* intialize the inter pred (MC) context at frame level */
        ps_ctxt->s_mc_ctxt.ps_ref_list = aps_ref_list;
        ps_ctxt->s_mc_ctxt.i1_weighted_pred_flag = i1_weighted_pred_flag;
        ps_ctxt->s_mc_ctxt.i1_weighted_bipred_flag = i1_weighted_bipred_flag;
        ps_ctxt->s_mc_ctxt.i4_log2_luma_wght_denom = log2_luma_wght_denom;
        ps_ctxt->s_mc_ctxt.i4_log2_chroma_wght_denom = log2_chroma_wght_denom;

        /* intialize the MV pred context at frame level */
        ps_ctxt->s_mv_pred_ctxt.ps_ref_list = aps_ref_list;
        ps_ctxt->s_mv_pred_ctxt.ps_slice_hdr = ps_slice_hdr;
        ps_ctxt->s_mv_pred_ctxt.ps_sps = ps_sps;
        ps_ctxt->s_mv_pred_ctxt.i4_log2_parallel_merge_level_minus2 =
            ps_pps->i1_log2_parallel_merge_level - 2;

#if ADAPT_COLOCATED_FROM_L0_FLAG
        if(ps_ctxt->s_mv_pred_ctxt.ps_slice_hdr->i1_slice_temporal_mvp_enable_flag)
        {
            if((ps_ctxt->s_mv_pred_ctxt.ps_slice_hdr->i1_num_ref_idx_l1_active > 0) &&
               (ps_ctxt->s_mv_pred_ctxt.ps_ref_list[1][0]->i4_frame_qp <
                ps_ctxt->s_mv_pred_ctxt.ps_ref_list[0][0]->i4_frame_qp))
            {
                ps_ctxt->s_mv_pred_ctxt.ps_slice_hdr->i1_collocated_from_l0_flag = 1;
            }
        }
#endif
        /* Initialization of deblocking params */
        ps_ctxt->s_deblk_prms.i4_beta_offset_div2 = ps_slice_hdr->i1_beta_offset_div2;
        ps_ctxt->s_deblk_prms.i4_tc_offset_div2 = ps_slice_hdr->i1_tc_offset_div2;

        ps_ctxt->s_deblk_prms.i4_cb_qp_indx_offset = ps_pps->i1_pic_cb_qp_offset;

        ps_ctxt->s_deblk_prms.i4_cr_qp_indx_offset = ps_pps->i1_pic_cr_qp_offset;
        /*init frame level stat accumualtion parameters */
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_sad_acc = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_intra_sad_acc = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_open_loop_intra_sad = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_frame_open_loop_ssd = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_inter_sad_acc = 0;

        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_frame_cost_acc = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_frame_intra_cost_acc = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_frame_inter_cost_acc = 0;

        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_intra_sad = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_rdopt_bits = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->u4_frame_rdopt_header_bits = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i4_qp_normalized_8x8_cu_sum[0] = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i4_qp_normalized_8x8_cu_sum[1] = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i4_8x8_cu_sum[0] = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i4_8x8_cu_sum[1] = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_sad_by_qscale[0] = 0;
        ps_ctxt->aaps_enc_loop_rc_params[ps_ctxt->i4_enc_frm_id][i4_bitrate_instance_num]
            ->i8_sad_by_qscale[1] = 0;
        /* Compute the frame_qstep */
        GET_FRAME_QSTEP_FROM_QP(ps_ctxt->i4_frame_qp, ps_ctxt->i4_frame_qstep);

        ps_ctxt->u1_max_tr_depth = ps_sps->i1_max_transform_hierarchy_depth_inter;

        ps_ctxt->ps_rc_quant_ctxt = &ps_enc_ctxt->s_rc_quant;
        /* intialize the cabac rdopt context at frame level */
        ihevce_entropy_rdo_frame_init(
            &ps_ctxt->s_rdopt_entropy_ctxt,
            ps_slice_hdr,
            ps_pps,
            ps_sps,
            ps_vps,
            ps_master_ctxt->au1_cu_skip_top_row,
            &ps_enc_ctxt->s_rc_quant);

        /* register the dep mngr instance for forward ME sync */
        ps_ctxt->pv_dep_mngr_encloop_dep_me = pv_dep_mngr_encloop_dep_me;
    }
}
/*
******************************************************************************
* \if Function name : ihevce_enc_loop_get_frame_rc_prms \endif
*
* \brief
*    returns Nil
*
* \param[in] pv_enc_loop_ctxt : pointer to encode loop context
* \param[out]ps_rc_prms       : ptr to frame level info structure
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_enc_loop_get_frame_rc_prms(
    void *pv_enc_loop_ctxt,
    rc_bits_sad_t *ps_rc_prms,
    WORD32 i4_br_id,  //bitrate instance id
    WORD32 i4_enc_frm_id)  // frame id
{
    /*Get the master thread pointer*/
    ihevce_enc_loop_master_ctxt_t *ps_master_ctxt;
    ihevce_enc_loop_ctxt_t *ps_ctxt;
    UWORD32 total_frame_intra_sad = 0, total_frame_open_loop_intra_sad = 0;
    LWORD64 i8_total_ssd_frame = 0;
    UWORD32 total_frame_sad = 0;
    UWORD32 total_frame_rdopt_bits = 0;
    UWORD32 total_frame_rdopt_header_bits = 0;
    WORD32 i4_qp_normalized_8x8_cu_sum[2] = { 0, 0 };
    WORD32 i4_8x8_cu_sum[2] = { 0, 0 };
    LWORD64 i8_sad_by_qscale[2] = { 0, 0 };
    WORD32 i4_curr_qp_acc = 0;
    WORD32 i;

    /* ENC_LOOP master state structure */
    ps_master_ctxt = (ihevce_enc_loop_master_ctxt_t *)pv_enc_loop_ctxt;

    if(1 == ps_master_ctxt->i4_num_enc_loop_frm_pllel)
    {
        i4_enc_frm_id = 0;
    }
    /*loop through all threads and accumulate intra sad across all threads*/
    for(i = 0; i < ps_master_ctxt->i4_num_proc_thrds; i++)
    {
        /* ENC_LOOP state structure */
        ps_ctxt = ps_master_ctxt->aps_enc_loop_thrd_ctxt[i];
        total_frame_open_loop_intra_sad +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_open_loop_intra_sad;
        i8_total_ssd_frame +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_frame_open_loop_ssd;
        total_frame_intra_sad +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_intra_sad;
        total_frame_sad +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_sad_acc;
        total_frame_rdopt_bits +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_rdopt_bits;
        total_frame_rdopt_header_bits +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->u4_frame_rdopt_header_bits;
        i4_qp_normalized_8x8_cu_sum[0] += ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]
                                              ->i4_qp_normalized_8x8_cu_sum[0];
        i4_qp_normalized_8x8_cu_sum[1] += ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]
                                              ->i4_qp_normalized_8x8_cu_sum[1];
        i4_8x8_cu_sum[0] +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i4_8x8_cu_sum[0];
        i4_8x8_cu_sum[1] +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i4_8x8_cu_sum[1];
        i8_sad_by_qscale[0] +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_sad_by_qscale[0];
        i8_sad_by_qscale[1] +=
            ps_ctxt->aaps_enc_loop_rc_params[i4_enc_frm_id][i4_br_id]->i8_sad_by_qscale[1];
    }

    ps_rc_prms->u4_open_loop_intra_sad = total_frame_open_loop_intra_sad;
    ps_rc_prms->i8_total_ssd_frame = i8_total_ssd_frame;
    ps_rc_prms->u4_total_sad = total_frame_sad;
    ps_rc_prms->u4_total_texture_bits = total_frame_rdopt_bits - total_frame_rdopt_header_bits;
    ps_rc_prms->u4_total_header_bits = total_frame_rdopt_header_bits;
    /*This accumulation of intra frame sad is not intact. This can only be a temp change*/
    ps_rc_prms->u4_total_intra_sad = total_frame_intra_sad;
    ps_rc_prms->i4_qp_normalized_8x8_cu_sum[0] = i4_qp_normalized_8x8_cu_sum[0];
    ps_rc_prms->i4_qp_normalized_8x8_cu_sum[1] = i4_qp_normalized_8x8_cu_sum[1];
    ps_rc_prms->i4_8x8_cu_sum[0] = i4_8x8_cu_sum[0];
    ps_rc_prms->i4_8x8_cu_sum[1] = i4_8x8_cu_sum[1];
    ps_rc_prms->i8_sad_by_qscale[0] = i8_sad_by_qscale[0];
    ps_rc_prms->i8_sad_by_qscale[1] = i8_sad_by_qscale[1];
}
