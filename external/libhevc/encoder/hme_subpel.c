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
* @file hme_subpel.c
*
* @brief
*    Subpel refinement modules for ME algo
*
* @author
*    Ittiam
*
*
* List of Functions
* hme_qpel_interp_avg()
* hme_subpel_refine_ctblist_bck()
* hme_subpel_refine_ctblist_fwd()
* hme_refine_bidirect()
* hme_subpel_refinement()
* hme_subpel_refine_ctb_fwd()
* hme_subpel_refine_ctb_bck()
* hme_create_bck_inp()
* hme_subpel_refine_search_node()
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
#include "hme_globals.h"
#include "hme_utils.h"
#include "hme_coarse.h"
#include "hme_fullpel.h"
#include "hme_subpel.h"
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"
#include "ihevce_stasino_helpers.h"
#include "ihevce_common_utils.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
void hme_qpel_interp_avg(interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, S32 i4_buf_id)
{
    U08 *pu1_src1, *pu1_src2, *pu1_dst;
    qpel_input_buf_cfg_t *ps_inp_cfg;
    S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;

    /*************************************************************************/
    /* For a given QPEL pt, we need to determine the 2 source pts that are   */
    /* needed to do the QPEL averaging. The logic to do this is as follows   */
    /* i4_mv_x and i4_mv_y are the motion vectors in QPEL units that are     */
    /* pointing to the pt of interest. Obviously, they are w.r.t. the 0,0    */
    /* pt of th reference blk that is colocated to the inp blk.              */
    /*    A j E k B                                                          */
    /*    l m n o p                                                          */
    /*    F q G r H                                                          */
    /*    s t u v w                                                          */
    /*    C x I y D                                                          */
    /* In above diagram, A. B, C, D are full pts at offsets (0,0),(1,0),(0,1)*/
    /* and (1,1) respectively in the fpel buffer (id = 0)                    */
    /* E and I are hxfy pts in offsets (0,0),(0,1) respectively in hxfy buf  */
    /* F and H are fxhy pts in offsets (0,0),(1,0) respectively in fxhy buf  */
    /* G is hxhy pt in offset 0,0 in hxhy buf                                */
    /* All above offsets are computed w.r.t. motion displaced pt in          */
    /* respective bufs. This means that A corresponds to (i4_mv_x >> 2) and  */
    /* (i4_mv_y >> 2) in fxfy buf. Ditto with E, F and G                     */
    /* fxfy buf is buf id 0, hxfy is buf id 1, fxhy is buf id 2, hxhy is 3   */
    /* If we consider pt v to be derived. v has a fractional comp of 3, 3    */
    /* v is avg of H and I. So the table look up of v should give following  */
    /* buf 1 (H) : offset = (1, 0) buf id = 2.                               */
    /* buf 2 (I) : offset = 0 , 1) buf id = 1.                               */
    /* NOTE: For pts that are fxfy/hxfy/fxhy/hxhy, bufid 1 will be -1.       */
    /*************************************************************************/
    i4_mv_x_frac = i4_mv_x & 3;
    i4_mv_y_frac = i4_mv_y & 3;

    i4_offset = (i4_mv_x >> 2) + (i4_mv_y >> 2) * ps_prms->i4_ref_stride;

    /* Derive the descriptor that has all offset and size info */
    ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

    if(ps_inp_cfg->i1_buf_id1 == ps_inp_cfg->i1_buf_id2)
    {
        /* This is case for fxfy/hxfy/fxhy/hxhy */
        ps_prms->pu1_final_out = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ps_prms->pu1_final_out += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ps_prms->pu1_final_out += (ps_inp_cfg->i1_buf_yoff1 * ps_prms->i4_ref_stride);
        ps_prms->i4_final_out_stride = ps_prms->i4_ref_stride;

        return;
    }

    pu1_src1 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
    pu1_src1 += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
    pu1_src1 += (ps_inp_cfg->i1_buf_yoff1 * ps_prms->i4_ref_stride);

    pu1_src2 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id2];
    pu1_src2 += ps_inp_cfg->i1_buf_xoff2 + i4_offset;
    pu1_src2 += (ps_inp_cfg->i1_buf_yoff2 * ps_prms->i4_ref_stride);

    pu1_dst = ps_prms->apu1_interp_out[i4_buf_id];
    hevc_avg_2d(
        pu1_src1,
        pu1_src2,
        ps_prms->i4_ref_stride,
        ps_prms->i4_ref_stride,
        ps_prms->i4_blk_wd,
        ps_prms->i4_blk_ht,
        pu1_dst,
        ps_prms->i4_out_stride);
    ps_prms->pu1_final_out = pu1_dst;
    ps_prms->i4_final_out_stride = ps_prms->i4_out_stride;
}

static __inline void hme_qpel_interp_avg_2pt_vert_no_reuse(
    interp_prms_t *ps_prms,
    S32 i4_mv_x,
    S32 i4_mv_y,
    U08 **ppu1_final,
    S32 *pi4_final_stride,
    FT_QPEL_INTERP_AVG_1PT *pf_qpel_interp_avg_1pt)
{
    pf_qpel_interp_avg_1pt(ps_prms, i4_mv_x, i4_mv_y + 1, 3, ppu1_final, pi4_final_stride);

    pf_qpel_interp_avg_1pt(ps_prms, i4_mv_x, i4_mv_y - 1, 1, ppu1_final, pi4_final_stride);
}

static __inline void hme_qpel_interp_avg_2pt_horz_no_reuse(
    interp_prms_t *ps_prms,
    S32 i4_mv_x,
    S32 i4_mv_y,
    U08 **ppu1_final,
    S32 *pi4_final_stride,
    FT_QPEL_INTERP_AVG_1PT *pf_qpel_interp_avg_1pt)
{
    pf_qpel_interp_avg_1pt(ps_prms, i4_mv_x + 1, i4_mv_y, 2, ppu1_final, pi4_final_stride);

    pf_qpel_interp_avg_1pt(ps_prms, i4_mv_x - 1, i4_mv_y, 0, ppu1_final, pi4_final_stride);
}

/********************************************************************************
*  @fn     hme_qpel_interp_comprehensive
*
*  @brief  Interpolates 2 qpel points by hpel averaging
*
*  @param[in,out]  ps_prms: Both input buffer ptrs and location of output
*
*  @param[in]  i4_mv_x : x component of motion vector in QPEL units
*
*  @param[in]  i4_mv_y : y component of motion vector in QPEL units
*
*  @param[in]  i4_grid_mask : mask which determines qpels to be computed
*
*  @param[out]  ppu1_final : storage for final buffer pointers
*
*  @param[out]  pi4_final_stride : storage for final buffer strides
*
*  @return None
********************************************************************************
*/
static __inline void hme_qpel_interp_comprehensive(
    interp_prms_t *ps_prms,
    U08 **ppu1_final,
    S32 *pi4_final_stride,
    S32 i4_mv_x,
    S32 i4_mv_y,
    S32 i4_grid_mask,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    S32 pt_select_for_TB, pt_select_for_LR;
    S32 dx, dy, dydx;
    S32 vert_func_selector, horz_func_selector;

    S32 i4_ref_stride = ps_prms->i4_ref_stride;

    pt_select_for_TB =
        ((i4_grid_mask & (1 << PT_B)) >> PT_B) + ((i4_grid_mask & (1 << PT_T)) >> (PT_T - 1));

    pt_select_for_LR =
        ((i4_grid_mask & (1 << PT_R)) >> PT_R) + ((i4_grid_mask & (1 << PT_L)) >> (PT_L - 1));

    dx = (i4_mv_x & 3);
    dy = (i4_mv_y & 3);
    dydx = (dx + (dy << 2));

    vert_func_selector = gai4_select_qpel_function_vert[pt_select_for_TB][dydx];
    horz_func_selector = gai4_select_qpel_function_horz[pt_select_for_LR][dydx];

    /* case descriptions */
    /* Let T = (gridmask & T) & B = (gridmask & B) */
    /* & hp = pt is an hpel or an fpel */
    /* & r = reuse possible */
    /* 0 => T || B = 0 */
    /* 1 => (!T) && (B) && hp */
    /* 2 => (T) && (!B) && hp */
    /* 3 => (!T) && (B) && !hp */
    /* 4 => (T) && (!B) && !hp */
    /* 5 => (T) && (B) && !hp && r */
    /* 6 => (T) && (B) && !hp && !r */
    /* 7 => (T) && (B) && hp */

    switch(vert_func_selector)
    {
    case 0:
    {
        break;
    }
    case 1:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;
        S32 i4_mvyp1 = (i4_mv_y + 1);

        i4_mv_x_frac = dx;
        i4_mv_y_frac = i4_mvyp1 & 3;

        i4_offset = (i4_mv_x >> 2) + (i4_mvyp1 >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[3] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[3] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[3] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[3] = i4_ref_stride;

        break;
    }
    case 2:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;
        S32 i4_mvym1 = (i4_mv_y - 1);

        i4_mv_x_frac = dx;
        i4_mv_y_frac = i4_mvym1 & 3;

        i4_offset = (i4_mv_x >> 2) + (i4_mvym1 >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[1] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[1] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[1] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[1] = i4_ref_stride;

        break;
    }
    case 3:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_1pt(
            ps_prms, i4_mv_x, i4_mv_y + 1, 3, ppu1_final, pi4_final_stride);

        break;
    }
    case 4:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_1pt(
            ps_prms, i4_mv_x, i4_mv_y - 1, 1, ppu1_final, pi4_final_stride);

        break;
    }
    case 5:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_2pt_vert_with_reuse(
            ps_prms, i4_mv_x, i4_mv_y, ppu1_final, pi4_final_stride);
        break;
    }
    case 6:
    {
        hme_qpel_interp_avg_2pt_vert_no_reuse(
            ps_prms,
            i4_mv_x,
            i4_mv_y,
            ppu1_final,
            pi4_final_stride,
            ps_me_optimised_function_list->pf_qpel_interp_avg_1pt);
        break;
    }
    case 7:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;

        S32 i4_mvyp1 = (i4_mv_y + 1);
        S32 i4_mvym1 = (i4_mv_y - 1);

        i4_mv_x_frac = dx;
        i4_mv_y_frac = i4_mvyp1 & 3;

        i4_offset = (i4_mv_x >> 2) + (i4_mvyp1 >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[3] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[3] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[3] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[3] = i4_ref_stride;

        i4_mv_y_frac = i4_mvym1 & 3;

        i4_offset = (i4_mv_x >> 2) + (i4_mvym1 >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[1] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[1] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[1] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[1] = i4_ref_stride;

        break;
    }
    }

    /* case descriptions */
    /* Let L = (gridmask & L) & R = (gridmask & R) */
    /* & hp = pt is an hpel or an fpel */
    /* & r = reuse possible */
    /* 0 => L || R = 0 */
    /* 1 => (!L) && (R) && hp */
    /* 2 => (L) && (!R) && hp */
    /* 3 => (!L) && (R) && !hp */
    /* 4 => (L) && (!R) && !hp */
    /* 5 => (L) && (R) && !hp && r */
    /* 6 => (L) && (R) && !hp && !r */
    /* 7 => (L) && (R) && hp */

    switch(horz_func_selector)
    {
    case 0:
    {
        break;
    }
    case 1:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;
        S32 i4_mvxp1 = (i4_mv_x + 1);

        i4_mv_x_frac = i4_mvxp1 & 3;
        i4_mv_y_frac = dy;

        i4_offset = (i4_mvxp1 >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[2] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[2] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[2] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[2] = i4_ref_stride;

        break;
    }
    case 2:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;
        S32 i4_mvxm1 = (i4_mv_x - 1);

        i4_mv_x_frac = i4_mvxm1 & 3;
        i4_mv_y_frac = dy;

        i4_offset = (i4_mvxm1 >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[0] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[0] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[0] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[0] = i4_ref_stride;

        break;
    }
    case 3:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_1pt(
            ps_prms, i4_mv_x + 1, i4_mv_y, 2, ppu1_final, pi4_final_stride);

        break;
    }
    case 4:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_1pt(
            ps_prms, i4_mv_x - 1, i4_mv_y, 0, ppu1_final, pi4_final_stride);

        break;
    }
    case 5:
    {
        ps_me_optimised_function_list->pf_qpel_interp_avg_2pt_horz_with_reuse(
            ps_prms, i4_mv_x, i4_mv_y, ppu1_final, pi4_final_stride);
        break;
    }
    case 6:
    {
        hme_qpel_interp_avg_2pt_horz_no_reuse(
            ps_prms,
            i4_mv_x,
            i4_mv_y,
            ppu1_final,
            pi4_final_stride,
            ps_me_optimised_function_list->pf_qpel_interp_avg_1pt);
        break;
    }
    case 7:
    {
        S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
        qpel_input_buf_cfg_t *ps_inp_cfg;

        S32 i4_mvxp1 = (i4_mv_x + 1);
        S32 i4_mvxm1 = (i4_mv_x - 1);

        i4_mv_x_frac = i4_mvxp1 & 3;
        i4_mv_y_frac = dy;

        i4_offset = (i4_mvxp1 >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[2] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[2] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[2] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[2] = i4_ref_stride;

        i4_mv_x_frac = i4_mvxm1 & 3;

        i4_offset = (i4_mvxm1 >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

        /* Derive the descriptor that has all offset and size info */
        ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

        ppu1_final[0] = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ppu1_final[0] += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ppu1_final[0] += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);
        pi4_final_stride[0] = i4_ref_stride;

        break;
    }
    }
}

/**
********************************************************************************
*  @fn     S32 hme_compute_pred_and_evaluate_bi(hme_subpel_prms_t *ps_prms,
*                                   search_results_t *ps_search_results,
*                                   layer_ctxt_t *ps_curr_layer,
*                                   U08 **ppu1_pred)
*
*
*  @brief  Evaluates the best bipred cost as avg(P0, P1) where P0 and P1 are
*          best L0 and L1 bufs respectively for the entire CU
*
*  @param[in]  ps_prms: subpel prms input to this function
*
*  @param[in] ps_curr_layer: points to the current layer ctxt
*
*  @return The best BI cost of best uni cost, whichever better
********************************************************************************
*/
void hme_compute_pred_and_evaluate_bi(
    inter_cu_results_t *ps_cu_results,
    inter_pu_results_t *ps_pu_results,
    inter_ctb_prms_t *ps_inter_ctb_prms,
    part_type_results_t *ps_part_type_result,
    ULWORD64 *pu8_winning_pred_sigmaXSquare,
    ULWORD64 *pu8_winning_pred_sigmaX,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    /* Idx0 - Uni winner */
    /* Idx1 - Uni runner-up */
    /* Idx2 - Bi winner */
    hme_pred_buf_info_t as_pred_buf_data[3][NUM_INTER_PU_PARTS];
    err_prms_t s_err_prms;
    interp_prms_t s_interp_prms;

    PF_SAD_FXN_T pf_err_compute;

    S32 i, j;
    S32 x_off, y_off, x_pic, y_pic;
    S32 i4_sad_grid;
    U08 e_cu_size;
    S32 i4_part_type;
    U08 u1_cu_size;
    S32 shift;
    S32 x_part, y_part, num_parts;
    S32 inp_stride, ref_stride;
    U08 au1_pred_buf_array_indixes[3];
    S32 cur_iter_best_cost;
    S32 uni_cost, bi_cost, best_cost, tot_cost;
    /* Idx0 - Uni winner */
    /* Idx1 - Bi winner */
    ULWORD64 au8_sigmaX[2][NUM_INTER_PU_PARTS];
    ULWORD64 au8_sigmaXSquared[2][NUM_INTER_PU_PARTS];
#if USE_NOISE_TERM_DURING_BICAND_SEARCH
    S32 i4_noise_term;
#endif

    interp_prms_t *ps_interp_prms = &s_interp_prms;

    S32 best_cand_in_opp_dir_idx = 0;
    S32 is_best_cand_an_intra = 0;
    U08 u1_is_cu_noisy = ps_inter_ctb_prms->u1_is_cu_noisy;
#if USE_NOISE_TERM_DURING_BICAND_SEARCH
    const S32 i4_default_src_wt = ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;
#endif
    tot_cost = 0;

    /* Start of the CU w.r.t. CTB */
    x_off = ps_cu_results->u1_x_off;
    y_off = ps_cu_results->u1_y_off;

    inp_stride = ps_inter_ctb_prms->i4_inp_stride;
    ref_stride = ps_inter_ctb_prms->i4_rec_stride;

    ps_interp_prms->i4_ref_stride = ref_stride;

    /* Start of the CU w.r.t. Pic 0,0 */
    x_pic = x_off + ps_inter_ctb_prms->i4_ctb_x_off;
    y_pic = y_off + ps_inter_ctb_prms->i4_ctb_y_off;

    u1_cu_size = ps_cu_results->u1_cu_size;
    e_cu_size = u1_cu_size;
    shift = (S32)e_cu_size;
    i4_part_type = ps_part_type_result->u1_part_type;
    num_parts = gau1_num_parts_in_part_type[i4_part_type];

    for(i = 0; i < 3; i++)
    {
        hme_init_pred_buf_info(
            &as_pred_buf_data[i],
            &ps_inter_ctb_prms->s_pred_buf_mngr,
            (ps_part_type_result->as_pu_results->pu.b4_wd + 1) << 2,
            (ps_part_type_result->as_pu_results->pu.b4_ht + 1) << 2,
            (PART_TYPE_T)i4_part_type);

        au1_pred_buf_array_indixes[i] = as_pred_buf_data[i][0].u1_pred_buf_array_id;
    }

    for(j = 0; j < num_parts; j++)
    {
        UWORD8 *apu1_hpel_ref[2][4];
        PART_ID_T e_part_id;
        BLK_SIZE_T e_blk_size;
        WORD8 i1_ref_idx;
        UWORD8 pred_dir;
        WORD32 ref_offset, inp_offset, wd, ht;
        pu_result_t *ps_pu_node1, *ps_pu_node2, *ps_pu_result;
        mv_t *aps_mv[2];
        UWORD8 num_active_ref_opp;
        UWORD8 num_results_per_part;
        WORD32 luma_weight_ref1, luma_offset_ref1;
        WORD32 luma_weight_ref2, luma_offset_ref2;
        WORD32 pu_node2_found = 0;

        e_part_id = ge_part_type_to_part_id[i4_part_type][j];
        e_blk_size = ge_part_id_to_blk_size[e_cu_size][e_part_id];

        x_part = gas_part_attr_in_cu[e_part_id].u1_x_start << shift;
        y_part = gas_part_attr_in_cu[e_part_id].u1_y_start << shift;

        ref_offset = (x_part + x_pic) + (y_pic + y_part) * ref_stride;
        inp_offset = (x_part + y_part * inp_stride) + ps_cu_results->i4_inp_offset;

        pred_dir = ps_part_type_result->as_pu_results[j].pu.b2_pred_mode;

        ps_pu_node1 = &(ps_part_type_result->as_pu_results[j]);

        if(PRED_L0 == pred_dir)
        {
            i1_ref_idx = ps_pu_node1->pu.mv.i1_l0_ref_idx;
            aps_mv[0] = &(ps_pu_node1->pu.mv.s_l0_mv);

            num_active_ref_opp =
                ps_inter_ctb_prms->u1_num_active_ref_l1 * (ps_inter_ctb_prms->i4_bidir_enabled);
            num_results_per_part = ps_pu_results->u1_num_results_per_part_l0[e_part_id];

            ps_pu_result = ps_pu_results->aps_pu_results[PRED_L0][e_part_id];

            ASSERT(i1_ref_idx >= 0);

            apu1_hpel_ref[0][0] =
                (UWORD8 *)(ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->s_yuv_buf_desc.pv_y_buf) +
                ref_offset;
            apu1_hpel_ref[0][1] =
                ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[0] +
                ref_offset;
            apu1_hpel_ref[0][2] =
                ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[1] +
                ref_offset;
            apu1_hpel_ref[0][3] =
                ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[2] +
                ref_offset;

            luma_weight_ref1 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]
                                   ->s_weight_offset.i2_luma_weight;
            luma_offset_ref1 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]
                                   ->s_weight_offset.i2_luma_offset;
        }
        else
        {
            i1_ref_idx = ps_pu_node1->pu.mv.i1_l1_ref_idx;
            aps_mv[0] = &(ps_pu_node1->pu.mv.s_l1_mv);

            ASSERT(i1_ref_idx >= 0);

            num_active_ref_opp =
                ps_inter_ctb_prms->u1_num_active_ref_l0 * (ps_inter_ctb_prms->i4_bidir_enabled);
            num_results_per_part = ps_pu_results->u1_num_results_per_part_l1[e_part_id];

            ps_pu_result = ps_pu_results->aps_pu_results[PRED_L1][e_part_id];

            apu1_hpel_ref[0][0] =
                (UWORD8 *)(ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->s_yuv_buf_desc.pv_y_buf) +
                ref_offset;
            apu1_hpel_ref[0][1] =
                ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[0] +
                ref_offset;
            apu1_hpel_ref[0][2] =
                ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[1] +
                ref_offset;
            apu1_hpel_ref[0][3] =
                ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[2] +
                ref_offset;

            luma_weight_ref1 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]
                                   ->s_weight_offset.i2_luma_weight;
            luma_offset_ref1 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]
                                   ->s_weight_offset.i2_luma_offset;
        }

        if(aps_mv[0]->i2_mvx == INTRA_MV)
        {
            uni_cost = ps_pu_node1->i4_tot_cost;
            cur_iter_best_cost = ps_pu_node1->i4_tot_cost;
            best_cost = MIN(uni_cost, cur_iter_best_cost);
            tot_cost += best_cost;
            continue;
        }

        ps_interp_prms->i4_blk_wd = wd = gau1_blk_size_to_wd[e_blk_size];
        ps_interp_prms->i4_blk_ht = ht = gau1_blk_size_to_ht[e_blk_size];
        ps_interp_prms->i4_out_stride = MAX_CU_SIZE;

        if(num_active_ref_opp)
        {
            if(PRED_L0 == pred_dir)
            {
                if(ps_pu_results->u1_num_results_per_part_l1[e_part_id])
                {
                    ps_pu_node2 = ps_pu_results->aps_pu_results[1][e_part_id];
                    pu_node2_found = 1;
                }
            }
            else
            {
                if(ps_pu_results->u1_num_results_per_part_l0[e_part_id])
                {
                    ps_pu_node2 = ps_pu_results->aps_pu_results[0][e_part_id];
                    pu_node2_found = 1;
                }
            }
        }

        if(!pu_node2_found)
        {
            bi_cost = INT_MAX >> 1;

            s_interp_prms.apu1_interp_out[0] = as_pred_buf_data[0][j].pu1_pred;
            ps_interp_prms->ppu1_ref = &apu1_hpel_ref[0][0];

            ps_me_optimised_function_list->pf_qpel_interp_avg_generic(
                ps_interp_prms, aps_mv[0]->i2_mvx, aps_mv[0]->i2_mvy, 0);

            if(ps_interp_prms->pu1_final_out != s_interp_prms.apu1_interp_out[0])
            {
                as_pred_buf_data[0][j].u1_pred_buf_array_id = UCHAR_MAX;
                as_pred_buf_data[0][j].pu1_pred = ps_interp_prms->pu1_final_out;
                as_pred_buf_data[0][j].i4_pred_stride = ps_interp_prms->i4_final_out_stride;
            }

            if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                hme_compute_sigmaX_and_sigmaXSquared(
                    as_pred_buf_data[0][j].pu1_pred,
                    as_pred_buf_data[0][j].i4_pred_stride,
                    &au8_sigmaX[0][j],
                    &au8_sigmaXSquared[0][j],
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    0,
                    1);
            }
        }
        else
        {
            i = 0;
            bi_cost = MAX_32BIT_VAL;
            is_best_cand_an_intra = 0;
            best_cand_in_opp_dir_idx = 0;

            pred_dir = ps_pu_node2[i].pu.b2_pred_mode;

            if(PRED_L0 == pred_dir)
            {
                i1_ref_idx = ps_pu_node2[i].pu.mv.i1_l0_ref_idx;
                aps_mv[1] = &(ps_pu_node2[i].pu.mv.s_l0_mv);

                ASSERT(i1_ref_idx >= 0);

                apu1_hpel_ref[1][0] =
                    (UWORD8 *)(ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]
                                   ->s_yuv_buf_desc.pv_y_buf) +
                    ref_offset;  //>ppu1_list_rec_fxfy[0][i1_ref_idx] + ref_offset;
                apu1_hpel_ref[1][1] =
                    ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[0] +
                    ref_offset;
                apu1_hpel_ref[1][2] =
                    ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[1] +
                    ref_offset;
                apu1_hpel_ref[1][3] =
                    ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]->apu1_y_sub_pel_planes[2] +
                    ref_offset;

                luma_weight_ref2 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]
                                       ->s_weight_offset.i2_luma_weight;
                luma_offset_ref2 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l0[i1_ref_idx]
                                       ->s_weight_offset.i2_luma_offset;
            }
            else
            {
                i1_ref_idx = ps_pu_node2[i].pu.mv.i1_l1_ref_idx;
                aps_mv[1] = &(ps_pu_node2[i].pu.mv.s_l1_mv);

                ASSERT(i1_ref_idx >= 0);

                apu1_hpel_ref[1][0] =
                    (UWORD8 *)(ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]
                                   ->s_yuv_buf_desc.pv_y_buf) +
                    ref_offset;  //>ppu1_list_rec_fxfy[0][i1_ref_idx] + ref_offset;
                apu1_hpel_ref[1][1] =
                    ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[0] +
                    ref_offset;
                apu1_hpel_ref[1][2] =
                    ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[1] +
                    ref_offset;
                apu1_hpel_ref[1][3] =
                    ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]->apu1_y_sub_pel_planes[2] +
                    ref_offset;

                luma_weight_ref2 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]
                                       ->s_weight_offset.i2_luma_weight;
                luma_offset_ref2 = (WORD32)ps_inter_ctb_prms->pps_rec_list_l1[i1_ref_idx]
                                       ->s_weight_offset.i2_luma_offset;
            }

            if(aps_mv[1]->i2_mvx == INTRA_MV)
            {
                uni_cost = ps_pu_node1->i4_tot_cost;
                cur_iter_best_cost = ps_pu_node2[i].i4_tot_cost;

                if(cur_iter_best_cost < bi_cost)
                {
                    bi_cost = cur_iter_best_cost;
                    best_cand_in_opp_dir_idx = i;
                    is_best_cand_an_intra = 1;
                }

                best_cost = MIN(uni_cost, bi_cost);
                tot_cost += best_cost;
                continue;
            }

            s_interp_prms.apu1_interp_out[0] = as_pred_buf_data[0][j].pu1_pred;
            ps_interp_prms->ppu1_ref = &apu1_hpel_ref[0][0];

            ps_me_optimised_function_list->pf_qpel_interp_avg_generic(
                ps_interp_prms, aps_mv[0]->i2_mvx, aps_mv[0]->i2_mvy, 0);

            if(ps_interp_prms->pu1_final_out != s_interp_prms.apu1_interp_out[0])
            {
                as_pred_buf_data[0][j].u1_pred_buf_array_id = UCHAR_MAX;
                as_pred_buf_data[0][j].pu1_pred = ps_interp_prms->pu1_final_out;
                as_pred_buf_data[0][j].i4_pred_stride = ps_interp_prms->i4_final_out_stride;
            }

            if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                hme_compute_sigmaX_and_sigmaXSquared(
                    as_pred_buf_data[0][j].pu1_pred,
                    as_pred_buf_data[0][j].i4_pred_stride,
                    &au8_sigmaX[0][j],
                    &au8_sigmaXSquared[0][j],
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    0,
                    1);
            }

            s_interp_prms.apu1_interp_out[0] = as_pred_buf_data[1][j].pu1_pred;
            ps_interp_prms->ppu1_ref = &apu1_hpel_ref[1][0];

            ps_me_optimised_function_list->pf_qpel_interp_avg_generic(
                ps_interp_prms, aps_mv[1]->i2_mvx, aps_mv[1]->i2_mvy, 0);

            if(ps_interp_prms->pu1_final_out != s_interp_prms.apu1_interp_out[0])
            {
                as_pred_buf_data[1][j].u1_pred_buf_array_id = UCHAR_MAX;
                as_pred_buf_data[1][j].pu1_pred = ps_interp_prms->pu1_final_out;
                as_pred_buf_data[1][j].i4_pred_stride = ps_interp_prms->i4_final_out_stride;
            }

            ps_cmn_utils_optimised_function_list->pf_wt_avg_2d(
                as_pred_buf_data[0][j].pu1_pred,
                as_pred_buf_data[1][j].pu1_pred,
                as_pred_buf_data[0][j].i4_pred_stride,
                as_pred_buf_data[1][j].i4_pred_stride,
                wd,
                ht,
                as_pred_buf_data[2][j].pu1_pred,
                as_pred_buf_data[2][j].i4_pred_stride,
                luma_weight_ref1,
                luma_weight_ref2,
                luma_offset_ref1,
                luma_offset_ref2,
                ps_inter_ctb_prms->wpred_log_wdc);

            if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                hme_compute_sigmaX_and_sigmaXSquared(
                    as_pred_buf_data[2][j].pu1_pred,
                    as_pred_buf_data[2][j].i4_pred_stride,
                    &au8_sigmaX[1][j],
                    &au8_sigmaXSquared[1][j],
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    ps_interp_prms->i4_blk_wd,
                    ps_interp_prms->i4_blk_ht,
                    0,
                    1);
            }

            s_err_prms.pu1_inp = (U08 *)ps_inter_ctb_prms->pu1_non_wt_inp + inp_offset;
            s_err_prms.i4_inp_stride = inp_stride;
            s_err_prms.i4_ref_stride = as_pred_buf_data[2][j].i4_pred_stride;
            s_err_prms.i4_part_mask = (ENABLE_2Nx2N);
            s_err_prms.i4_grid_mask = 1;
            s_err_prms.pi4_sad_grid = &i4_sad_grid;
            s_err_prms.i4_blk_wd = wd;
            s_err_prms.i4_blk_ht = ht;
            s_err_prms.pu1_ref = as_pred_buf_data[2][j].pu1_pred;
            s_err_prms.ps_cmn_utils_optimised_function_list = ps_cmn_utils_optimised_function_list;

            if(ps_inter_ctb_prms->u1_use_satd)
            {
                pf_err_compute = compute_satd_8bit;
            }
            else
            {
                pf_err_compute = ps_me_optimised_function_list->pf_evalsad_pt_npu_mxn_8bit;
            }

            pf_err_compute(&s_err_prms);

#if USE_NOISE_TERM_DURING_BICAND_SEARCH
            if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                unsigned long u4_shift_val;
                ULWORD64 u8_src_variance, u8_pred_variance, u8_pred_sigmaSquareX;
                ULWORD64 u8_temp_var, u8_temp_var1;
                S32 i4_bits_req;

                S32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

                u8_pred_sigmaSquareX = (au8_sigmaX[1][j] * au8_sigmaX[1][j]);
                u8_pred_variance = au8_sigmaXSquared[1][j] - u8_pred_sigmaSquareX;

                if(e_cu_size == CU_8x8)
                {
                    PART_ID_T e_part_id =
                        (PART_ID_T)((PART_ID_NxN_TL) + (x_off & 1) + ((y_off & 1) << 1));

                    u4_shift_val = ihevce_calc_stim_injected_variance(
                        ps_inter_ctb_prms->pu8_part_src_sigmaX,
                        ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                        &u8_src_variance,
                        i4_default_src_wt,
                        0,
                        ps_inter_ctb_prms->wpred_log_wdc,
                        e_part_id);
                }
                else
                {
                    u4_shift_val = ihevce_calc_stim_injected_variance(
                        ps_inter_ctb_prms->pu8_part_src_sigmaX,
                        ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                        &u8_src_variance,
                        i4_default_src_wt,
                        0,
                        ps_inter_ctb_prms->wpred_log_wdc,
                        e_part_id);
                }

                u8_pred_variance = u8_pred_variance >> u4_shift_val;

                GETRANGE64(i4_bits_req, u8_pred_variance);

                if(i4_bits_req > 27)
                {
                    u8_pred_variance = u8_pred_variance >> (i4_bits_req - 27);
                    u8_src_variance = u8_src_variance >> (i4_bits_req - 27);
                }

                if(u8_src_variance == u8_pred_variance)
                {
                    u8_temp_var = (1 << STIM_Q_FORMAT);
                }
                else
                {
                    u8_temp_var = (2 * u8_src_variance * u8_pred_variance);
                    u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                    u8_temp_var1 =
                        (u8_src_variance * u8_src_variance) + (u8_pred_variance * u8_pred_variance);
                    u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                    u8_temp_var = (u8_temp_var / u8_temp_var1);
                }

                i4_noise_term = (UWORD32)u8_temp_var;

                i4_noise_term *= ps_inter_ctb_prms->i4_alpha_stim_multiplier;

                ASSERT(i4_noise_term >= 0);

                u8_temp_var = i4_sad_grid;
                u8_temp_var *= ((1 << (i4_q_level)) - (i4_noise_term));
                u8_temp_var += (1 << ((i4_q_level)-1));
                i4_sad_grid = (UWORD32)(u8_temp_var >> (i4_q_level));
            }
#endif

            cur_iter_best_cost = i4_sad_grid;
            cur_iter_best_cost += ps_pu_node1->i4_mv_cost;
            cur_iter_best_cost += ps_pu_node2[i].i4_mv_cost;

            if(cur_iter_best_cost < bi_cost)
            {
                bi_cost = cur_iter_best_cost;
                best_cand_in_opp_dir_idx = i;
                is_best_cand_an_intra = 0;
            }
        }

        uni_cost = ps_pu_node1->i4_tot_cost;

#if USE_NOISE_TERM_DURING_BICAND_SEARCH
        if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
        {
            unsigned long u4_shift_val;
            ULWORD64 u8_src_variance, u8_pred_variance, u8_pred_sigmaSquareX;
            ULWORD64 u8_temp_var, u8_temp_var1;
            S32 i4_bits_req;

            S32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

            S08 i1_ref_idx =
                (PRED_L0 == ps_pu_node1->pu.b2_pred_mode)
                    ? ps_inter_ctb_prms->pi1_past_list[ps_pu_node1->pu.mv.i1_l0_ref_idx]
                    : ps_inter_ctb_prms->pi1_future_list[ps_pu_node1->pu.mv.i1_l1_ref_idx];
            S32 i4_sad = ps_pu_node1->i4_tot_cost - ps_pu_node1->i4_mv_cost;

            u8_pred_sigmaSquareX = (au8_sigmaX[0][j] * au8_sigmaX[0][j]);
            u8_pred_variance = au8_sigmaXSquared[0][j] - u8_pred_sigmaSquareX;

            if(e_cu_size == CU_8x8)
            {
                PART_ID_T e_part_id =
                    (PART_ID_T)((PART_ID_NxN_TL) + (x_off & 1) + ((y_off & 1) << 1));

                u4_shift_val = ihevce_calc_stim_injected_variance(
                    ps_inter_ctb_prms->pu8_part_src_sigmaX,
                    ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                    &u8_src_variance,
                    ps_inter_ctb_prms->pi4_inv_wt[i1_ref_idx],
                    ps_inter_ctb_prms->pi4_inv_wt_shift_val[i1_ref_idx],
                    ps_inter_ctb_prms->wpred_log_wdc,
                    e_part_id);
            }
            else
            {
                u4_shift_val = ihevce_calc_stim_injected_variance(
                    ps_inter_ctb_prms->pu8_part_src_sigmaX,
                    ps_inter_ctb_prms->pu8_part_src_sigmaXSquared,
                    &u8_src_variance,
                    ps_inter_ctb_prms->pi4_inv_wt[i1_ref_idx],
                    ps_inter_ctb_prms->pi4_inv_wt_shift_val[i1_ref_idx],
                    ps_inter_ctb_prms->wpred_log_wdc,
                    e_part_id);
            }

            u8_pred_variance = u8_pred_variance >> (u4_shift_val);

            GETRANGE64(i4_bits_req, u8_pred_variance);

            if(i4_bits_req > 27)
            {
                u8_pred_variance = u8_pred_variance >> (i4_bits_req - 27);
                u8_src_variance = u8_src_variance >> (i4_bits_req - 27);
            }

            if(u8_src_variance == u8_pred_variance)
            {
                u8_temp_var = (1 << STIM_Q_FORMAT);
            }
            else
            {
                u8_temp_var = (2 * u8_src_variance * u8_pred_variance);
                u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                u8_temp_var1 =
                    (u8_src_variance * u8_src_variance) + (u8_pred_variance * u8_pred_variance);
                u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                u8_temp_var = (u8_temp_var / u8_temp_var1);
            }

            i4_noise_term = (UWORD32)u8_temp_var;

            i4_noise_term *= ps_inter_ctb_prms->i4_alpha_stim_multiplier;

            ASSERT(i4_noise_term >= 0);

            u8_temp_var = i4_sad;
            u8_temp_var *= ((1 << (i4_q_level)) - (i4_noise_term));
            u8_temp_var += (1 << ((i4_q_level)-1));
            i4_sad = (UWORD32)(u8_temp_var >> (i4_q_level));

            uni_cost = i4_sad + ps_pu_node1->i4_mv_cost;

            pu8_winning_pred_sigmaX[j] = au8_sigmaX[0][j];
            pu8_winning_pred_sigmaXSquare[j] = au8_sigmaXSquared[0][j];
        }
#endif

        if((bi_cost < uni_cost) && (!is_best_cand_an_intra))
        {
            if(u1_is_cu_noisy && ps_inter_ctb_prms->i4_alpha_stim_multiplier)
            {
                pu8_winning_pred_sigmaX[j] = au8_sigmaX[1][j];
                pu8_winning_pred_sigmaXSquare[j] = au8_sigmaXSquared[1][j];
            }

            if(PRED_L0 == ps_pu_node1->pu.b2_pred_mode)
            {
                ps_pu_node1->pu.b2_pred_mode = PRED_BI;

                if(PRED_L0 == ps_pu_node2[best_cand_in_opp_dir_idx].pu.b2_pred_mode)
                {
                    ps_pu_node1->pu.mv.i1_l1_ref_idx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.i1_l0_ref_idx;
                    ps_pu_node1->pu.mv.s_l1_mv.i2_mvx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l0_mv.i2_mvx;
                    ps_pu_node1->pu.mv.s_l1_mv.i2_mvy =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l0_mv.i2_mvy;
                }
                else
                {
                    ps_pu_node1->pu.mv.i1_l1_ref_idx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.i1_l1_ref_idx;
                    ps_pu_node1->pu.mv.s_l1_mv.i2_mvx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l1_mv.i2_mvx;
                    ps_pu_node1->pu.mv.s_l1_mv.i2_mvy =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l1_mv.i2_mvy;
                }
            }
            else
            {
                ps_pu_node1->pu.b2_pred_mode = PRED_BI;

                if(PRED_L0 == ps_pu_node2[best_cand_in_opp_dir_idx].pu.b2_pred_mode)
                {
                    ps_pu_node1->pu.mv.i1_l0_ref_idx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.i1_l0_ref_idx;
                    ps_pu_node1->pu.mv.s_l0_mv.i2_mvx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l0_mv.i2_mvx;
                    ps_pu_node1->pu.mv.s_l0_mv.i2_mvy =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l0_mv.i2_mvy;
                }
                else
                {
                    ps_pu_node1->pu.mv.i1_l0_ref_idx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.i1_l1_ref_idx;
                    ps_pu_node1->pu.mv.s_l0_mv.i2_mvx =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l1_mv.i2_mvx;
                    ps_pu_node1->pu.mv.s_l0_mv.i2_mvy =
                        ps_pu_node2[best_cand_in_opp_dir_idx].pu.mv.s_l1_mv.i2_mvy;
                }
            }

            ps_part_type_result->as_pu_results[j].i4_tot_cost = bi_cost;
        }

        best_cost = MIN(uni_cost, bi_cost);
        tot_cost += best_cost;
    }

    hme_debrief_bipred_eval(
        ps_part_type_result,
        as_pred_buf_data,
        &ps_inter_ctb_prms->s_pred_buf_mngr,
        au1_pred_buf_array_indixes,
        ps_cmn_utils_optimised_function_list);

    ps_part_type_result->i4_tot_cost = tot_cost;
}

WORD32 hme_evalsatd_pt_pu_8x8_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector)
{
    S32 ai4_satd_4x4[4]; /* num 4x4s in a 8x8 */
    S32 i4_satd_8x8;
    S16 *pi2_had_out;
    S32 i4_tu_split_flag = 0;
    S32 i4_tu_early_cbf = 0;

    S32 i4_early_cbf = 1;
    //  S32 i4_i, i4_k;
    S32 i4_total_satd_cost = 0;
    S32 best_cost_tu_split;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *api4_tu_split[HAD_32x32 + 1];
    S32 *api4_tu_early_cbf[HAD_32x32 + 1];

    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;
    S32 *pi4_tu_split = ps_prms->pi4_tu_split_flags;
    S32 *pi4_early_cbf = ps_prms->pi4_tu_early_cbf;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    /* Initialize tu_split_cost to "0" */
    ps_prms->i4_tu_split_cost = 0;
    pi2_had_out = (S16 *)ps_prms->pu1_wkg_mem;

    api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &i4_satd_8x8;
    api4_satd_pu[HAD_16x16] = NULL;
    api4_satd_pu[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    api4_tu_split[HAD_4x4] = NULL;
    api4_tu_split[HAD_8x8] = &i4_tu_split_flag;
    api4_tu_split[HAD_16x16] = NULL;
    api4_tu_split[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    api4_tu_early_cbf[HAD_4x4] = NULL;
    api4_tu_early_cbf[HAD_8x8] = &i4_tu_early_cbf;
    api4_tu_early_cbf[HAD_16x16] = NULL;
    api4_tu_early_cbf[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    /* Call recursive 16x16 HAD module; updates satds for 4x4, 8x8 and 16x16 */

    /* Return value is merge of both best_stad_cost and tu_split_flags */
    best_cost_tu_split = ps_func_selector->pf_had_8x8_using_4_4x4_r(
        pu1_inp,
        inp_stride,
        pu1_ref,
        ref_stride,
        pi2_had_out,
        8,
        api4_satd_pu,
        api4_tu_split,
        api4_tu_early_cbf,
        0,
        2,
        0,
        0,
        i4_frm_qstep,
        0,
        ps_prms->u1_max_tr_depth,
        ps_prms->u1_max_tr_size,
        &(ps_prms->i4_tu_split_cost),
        NULL);

    /* For SATD computation following TU size are assumed for a 8x8 CU */
    /* 8 for 2Nx2N, 4 for Nx2N,2NxN                                    */

    i4_total_satd_cost = best_cost_tu_split >> 2;

    /* Second last bit has the tu pslit flag */
    i4_tu_split_flag = (best_cost_tu_split & 0x3) >> 1;

    /* Last bit corrsponds to the Early CBF flag */
    i4_early_cbf = (best_cost_tu_split & 0x1);

    /* Update 8x8 SATDs */
    pi4_sad_grid[PART_ID_2Nx2N] = i4_satd_8x8;
    pi4_tu_split[PART_ID_2Nx2N] = i4_tu_split_flag;
    pi4_early_cbf[PART_ID_2Nx2N] = i4_early_cbf;

    return i4_total_satd_cost;
}
//#endif
/**
********************************************************************************
*  @fn     S32 hme_evalsatd_update_1_best_result_pt_pu_16x16
*
*  @brief  Evaluates the SATD with partial updates for all the best partitions
*          of a 16x16 CU based on recursive Hadamard 16x16, 8x8 and 4x4 satds
*
*  @param[inout]  ps_prms: error prms containg current and ref ptr, strides,
*                 pointer to sad grid of each partitions
*
*  @return     None
********************************************************************************
*/

void hme_evalsatd_update_2_best_results_pt_pu_16x16(
    err_prms_t *ps_prms, result_upd_prms_t *ps_result_prms)
{
    S32 ai4_satd_4x4[16]; /* num 4x4s in a 16x16 */
    S32 ai4_satd_8x8[4]; /* num 8x8s in a 16x16 */
    S32 i4_satd_16x16; /* 16x16 satd cost     */
    S32 i;
    S16 ai2_8x8_had[256];
    S16 *pi2_y0;
    U08 *pu1_src, *pu1_pred;
    S32 pos_x_y_4x4_0, pos_x_y_4x4 = 0;
    S32 *ppi4_hsad;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
    api4_satd_pu[HAD_16x16] = &i4_satd_16x16;
    api4_satd_pu[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    ppi4_hsad = api4_satd_pu[HAD_16x16];

    /* Call recursive 16x16 HAD module; updates satds for 4x4, 8x8 and 16x16 */
    for(i = 0; i < 4; i++)
    {
        pu1_src = pu1_inp + (i & 0x01) * 8 + (i >> 1) * inp_stride * 8;
        pu1_pred = pu1_ref + (i & 0x01) * 8 + (i >> 1) * ref_stride * 8;
        pi2_y0 = ai2_8x8_had + (i & 0x01) * 8 + (i >> 1) * 16 * 8;
        pos_x_y_4x4_0 = pos_x_y_4x4 + (i & 0x01) * 2 + (i >> 1) * (2 << 16);

        ihevce_had_8x8_using_4_4x4(
            pu1_src, inp_stride, pu1_pred, ref_stride, pi2_y0, 16, api4_satd_pu, pos_x_y_4x4_0, 4);
    }

    /* For SATD computation following TU size are assumed for a 16x16 CU */
    /* 16 for 2Nx2N, 8 for NxN/Nx2N,2NxN and mix of 4 and 8 for AMPs     */

    /* Update 8x8 SATDs */
    /* Modified to cost calculation using only 4x4 SATD */

    //  ai4_satd_8x8[0] = ai4_satd_4x4[0] + ai4_satd_4x4[1] + ai4_satd_4x4[4] + ai4_satd_4x4[5];
    //  ai4_satd_8x8[1] = ai4_satd_4x4[2] + ai4_satd_4x4[3] + ai4_satd_4x4[6] + ai4_satd_4x4[7];
    //  ai4_satd_8x8[2] = ai4_satd_4x4[8] + ai4_satd_4x4[9] + ai4_satd_4x4[12] + ai4_satd_4x4[13];
    //  ai4_satd_8x8[3] = ai4_satd_4x4[10] + ai4_satd_4x4[11] + ai4_satd_4x4[14] + ai4_satd_4x4[15];

    /* Update 16x16 SATDs */
    pi4_sad_grid[PART_ID_2Nx2N] =
        ai4_satd_8x8[0] + ai4_satd_8x8[1] + ai4_satd_8x8[2] + ai4_satd_8x8[3];

    pi4_sad_grid[PART_ID_NxN_TL] = ai4_satd_8x8[0];
    pi4_sad_grid[PART_ID_NxN_TR] = ai4_satd_8x8[1];
    pi4_sad_grid[PART_ID_NxN_BL] = ai4_satd_8x8[2];
    pi4_sad_grid[PART_ID_NxN_BR] = ai4_satd_8x8[3];

    /* Update 8x16 / 16x8 SATDs */
    pi4_sad_grid[PART_ID_Nx2N_L] = ai4_satd_8x8[0] + ai4_satd_8x8[2];
    pi4_sad_grid[PART_ID_Nx2N_R] = ai4_satd_8x8[1] + ai4_satd_8x8[3];
    pi4_sad_grid[PART_ID_2NxN_T] = ai4_satd_8x8[0] + ai4_satd_8x8[1];
    pi4_sad_grid[PART_ID_2NxN_B] = ai4_satd_8x8[2] + ai4_satd_8x8[3];

    /* Update AMP SATDs 16x12,16x4, 12x16,4x16  */
    pi4_sad_grid[PART_ID_nLx2N_L] =
        ai4_satd_4x4[0] + ai4_satd_4x4[4] + ai4_satd_4x4[8] + ai4_satd_4x4[12];

    pi4_sad_grid[PART_ID_nLx2N_R] = ai4_satd_4x4[1] + ai4_satd_4x4[5] + ai4_satd_4x4[9] +
                                    ai4_satd_4x4[13] + pi4_sad_grid[PART_ID_Nx2N_R];

    pi4_sad_grid[PART_ID_nRx2N_L] = ai4_satd_4x4[2] + ai4_satd_4x4[6] + ai4_satd_4x4[10] +
                                    ai4_satd_4x4[14] + pi4_sad_grid[PART_ID_Nx2N_L];

    pi4_sad_grid[PART_ID_nRx2N_R] =
        ai4_satd_4x4[3] + ai4_satd_4x4[7] + ai4_satd_4x4[11] + ai4_satd_4x4[15];

    pi4_sad_grid[PART_ID_2NxnU_T] =
        ai4_satd_4x4[0] + ai4_satd_4x4[1] + ai4_satd_4x4[2] + ai4_satd_4x4[3];

    pi4_sad_grid[PART_ID_2NxnU_B] = ai4_satd_4x4[4] + ai4_satd_4x4[5] + ai4_satd_4x4[6] +
                                    ai4_satd_4x4[7] + pi4_sad_grid[PART_ID_2NxN_B];

    pi4_sad_grid[PART_ID_2NxnD_T] = ai4_satd_4x4[8] + ai4_satd_4x4[9] + ai4_satd_4x4[10] +
                                    ai4_satd_4x4[11] + pi4_sad_grid[PART_ID_2NxN_T];

    pi4_sad_grid[PART_ID_2NxnD_B] =
        ai4_satd_4x4[12] + ai4_satd_4x4[13] + ai4_satd_4x4[14] + ai4_satd_4x4[15];

    /* Call the update results function */
    {
        S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
        mv_refine_ctxt_t *ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
        S32 *pi4_valid_part_ids = &ps_subpel_refine_ctxt->ai4_part_id[0];
        S32 best_node_cost;
        S32 second_best_node_cost;

        /*For each valid partition, update the refine_prm structure to reflect the best and second
        best candidates for that partition*/

        for(i4_count = 0; i4_count < ps_subpel_refine_ctxt->i4_num_valid_parts; i4_count++)
        {
            S32 update_required = 0;
            S32 part_id = pi4_valid_part_ids[i4_count];
            S32 index = (ps_subpel_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

            /* Use a pre-computed cost instead of freshly evaluating subpel cost */
            i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];

            /*Calculate total cost*/
            i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
            i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);

            /*****************************************************************/
            /* We do not labor through the results if the total cost worse   */
            /* than the last of the results.                                 */
            /*****************************************************************/
            best_node_cost = CLIP_S16(ps_subpel_refine_ctxt->i2_tot_cost[0][index]);
            second_best_node_cost = CLIP_S16(ps_subpel_refine_ctxt->i2_tot_cost[1][index]);

            if(i4_tot_cost < second_best_node_cost)
            {
                update_required = 2;

                /*************************************************************/
                /* Identify where the current result isto be placed.Basically*/
                /* find the node which has cost just higher thannodeundertest*/
                /*************************************************************/
                if(i4_tot_cost < best_node_cost)
                {
                    update_required = 1;
                }
                else if(i4_tot_cost == ps_subpel_refine_ctxt->i2_tot_cost[0][index])
                {
                    update_required = 0;
                }
                if(update_required == 2)
                {
                    ps_subpel_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                    ps_subpel_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                    ps_subpel_refine_ctxt->i2_mv_x[1][index] = ps_result_prms->i2_mv_x;
                    ps_subpel_refine_ctxt->i2_mv_y[1][index] = ps_result_prms->i2_mv_y;
                    ps_subpel_refine_ctxt->i2_ref_idx[1][index] = ps_result_prms->i1_ref_idx;
                }
                else if(update_required == 1)
                {
                    ps_subpel_refine_ctxt->i2_tot_cost[1][index] =
                        ps_subpel_refine_ctxt->i2_tot_cost[0][index];
                    ps_subpel_refine_ctxt->i2_mv_cost[1][index] =
                        ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                    ps_subpel_refine_ctxt->i2_mv_x[1][index] =
                        ps_subpel_refine_ctxt->i2_mv_x[0][index];
                    ps_subpel_refine_ctxt->i2_mv_y[1][index] =
                        ps_subpel_refine_ctxt->i2_mv_y[0][index];
                    ps_subpel_refine_ctxt->i2_ref_idx[1][index] =
                        ps_subpel_refine_ctxt->i2_ref_idx[0][index];

                    ps_subpel_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                    ps_subpel_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                    ps_subpel_refine_ctxt->i2_mv_x[0][index] = ps_result_prms->i2_mv_x;
                    ps_subpel_refine_ctxt->i2_mv_y[0][index] = ps_result_prms->i2_mv_y;
                    ps_subpel_refine_ctxt->i2_ref_idx[0][index] = ps_result_prms->i1_ref_idx;
                }
            }
        }
    }
}

//#if COMPUTE_16x16_R == C
void hme_evalsatd_update_1_best_result_pt_pu_16x16(
    err_prms_t *ps_prms, result_upd_prms_t *ps_result_prms)
{
    S32 ai4_satd_4x4[16]; /* num 4x4s in a 16x16 */
    S32 ai4_satd_8x8[4]; /* num 8x8s in a 16x16 */
    S32 i4_satd_16x16; /* 16x16 satd cost     */
    S32 i;
    S16 ai2_8x8_had[256];
    S16 *pi2_y0;
    U08 *pu1_src, *pu1_pred;
    S32 pos_x_y_4x4_0, pos_x_y_4x4 = 0;
    S32 *ppi4_hsad;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
    api4_satd_pu[HAD_16x16] = &i4_satd_16x16;
    api4_satd_pu[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    ppi4_hsad = api4_satd_pu[HAD_16x16];

    /* Call recursive 16x16 HAD module; updates satds for 4x4, 8x8 and 16x16 */
    for(i = 0; i < 4; i++)
    {
        pu1_src = pu1_inp + (i & 0x01) * 8 + (i >> 1) * inp_stride * 8;
        pu1_pred = pu1_ref + (i & 0x01) * 8 + (i >> 1) * ref_stride * 8;
        pi2_y0 = ai2_8x8_had + (i & 0x01) * 8 + (i >> 1) * 16 * 8;
        pos_x_y_4x4_0 = pos_x_y_4x4 + (i & 0x01) * 2 + (i >> 1) * (2 << 16);

        ihevce_had_8x8_using_4_4x4(
            pu1_src, inp_stride, pu1_pred, ref_stride, pi2_y0, 16, api4_satd_pu, pos_x_y_4x4_0, 4);
    }

    /* For SATD computation following TU size are assumed for a 16x16 CU */
    /* 16 for 2Nx2N, 8 for NxN/Nx2N,2NxN and mix of 4 and 8 for AMPs     */

    /* Update 8x8 SATDs */
    /* Modified to cost calculation using only 4x4 SATD */

    //  ai4_satd_8x8[0] = ai4_satd_4x4[0] + ai4_satd_4x4[1] + ai4_satd_4x4[4] + ai4_satd_4x4[5];
    //  ai4_satd_8x8[1] = ai4_satd_4x4[2] + ai4_satd_4x4[3] + ai4_satd_4x4[6] + ai4_satd_4x4[7];
    //  ai4_satd_8x8[2] = ai4_satd_4x4[8] + ai4_satd_4x4[9] + ai4_satd_4x4[12] + ai4_satd_4x4[13];
    //  ai4_satd_8x8[3] = ai4_satd_4x4[10] + ai4_satd_4x4[11] + ai4_satd_4x4[14] + ai4_satd_4x4[15];

    /* Update 16x16 SATDs */
    pi4_sad_grid[PART_ID_2Nx2N] =
        ai4_satd_8x8[0] + ai4_satd_8x8[1] + ai4_satd_8x8[2] + ai4_satd_8x8[3];

    pi4_sad_grid[PART_ID_NxN_TL] = ai4_satd_8x8[0];
    pi4_sad_grid[PART_ID_NxN_TR] = ai4_satd_8x8[1];
    pi4_sad_grid[PART_ID_NxN_BL] = ai4_satd_8x8[2];
    pi4_sad_grid[PART_ID_NxN_BR] = ai4_satd_8x8[3];

    /* Update 8x16 / 16x8 SATDs */
    pi4_sad_grid[PART_ID_Nx2N_L] = ai4_satd_8x8[0] + ai4_satd_8x8[2];
    pi4_sad_grid[PART_ID_Nx2N_R] = ai4_satd_8x8[1] + ai4_satd_8x8[3];
    pi4_sad_grid[PART_ID_2NxN_T] = ai4_satd_8x8[0] + ai4_satd_8x8[1];
    pi4_sad_grid[PART_ID_2NxN_B] = ai4_satd_8x8[2] + ai4_satd_8x8[3];

    /* Update AMP SATDs 16x12,16x4, 12x16,4x16  */
    pi4_sad_grid[PART_ID_nLx2N_L] =
        ai4_satd_4x4[0] + ai4_satd_4x4[2] + ai4_satd_4x4[8] + ai4_satd_4x4[10];
    pi4_sad_grid[PART_ID_nRx2N_R] =
        ai4_satd_4x4[5] + ai4_satd_4x4[7] + ai4_satd_4x4[13] + ai4_satd_4x4[15];
    pi4_sad_grid[PART_ID_2NxnU_T] =
        ai4_satd_4x4[0] + ai4_satd_4x4[1] + ai4_satd_4x4[4] + ai4_satd_4x4[5];
    pi4_sad_grid[PART_ID_2NxnD_B] =
        ai4_satd_4x4[10] + ai4_satd_4x4[11] + ai4_satd_4x4[14] + ai4_satd_4x4[15];

    pi4_sad_grid[PART_ID_nLx2N_R] = pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
    pi4_sad_grid[PART_ID_nRx2N_L] = pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
    pi4_sad_grid[PART_ID_2NxnU_B] = pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
    pi4_sad_grid[PART_ID_2NxnD_T] = pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];

    /* Call the update results function */
    {
        S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
        mv_refine_ctxt_t *ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
        S32 *pi4_valid_part_ids = &ps_subpel_refine_ctxt->ai4_part_id[0];
        S32 best_node_cost;
        S32 second_best_node_cost;

        /*For each valid partition, update the refine_prm structure to reflect the best and second
        best candidates for that partition*/

        for(i4_count = 0; i4_count < ps_subpel_refine_ctxt->i4_num_valid_parts; i4_count++)
        {
            S32 update_required = 0;
            S32 part_id = pi4_valid_part_ids[i4_count];
            S32 index = (ps_subpel_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

            /* Use a pre-computed cost instead of freshly evaluating subpel cost */
            i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];

            /*Calculate total cost*/
            i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
            i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);

            /*****************************************************************/
            /* We do not labor through the results if the total cost worse   */
            /* than the last of the results.                                 */
            /*****************************************************************/
            best_node_cost = CLIP_S16(ps_subpel_refine_ctxt->i2_tot_cost[0][index]);
            second_best_node_cost = SHRT_MAX;

            if(i4_tot_cost < second_best_node_cost)
            {
                update_required = 0;

                /*************************************************************/
                /* Identify where the current result isto be placed.Basically*/
                /* find the node which has cost just higher thannodeundertest*/
                /*************************************************************/
                if(i4_tot_cost < best_node_cost)
                {
                    update_required = 1;
                }
                else if(i4_tot_cost == ps_subpel_refine_ctxt->i2_tot_cost[0][index])
                {
                    update_required = 0;
                }
                if(update_required == 2)
                {
                    ps_subpel_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                    ps_subpel_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                    ps_subpel_refine_ctxt->i2_mv_x[1][index] = ps_result_prms->i2_mv_x;
                    ps_subpel_refine_ctxt->i2_mv_y[1][index] = ps_result_prms->i2_mv_y;
                    ps_subpel_refine_ctxt->i2_ref_idx[1][index] = ps_result_prms->i1_ref_idx;
                }
                else if(update_required == 1)
                {
                    ps_subpel_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                    ps_subpel_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                    ps_subpel_refine_ctxt->i2_mv_x[0][index] = ps_result_prms->i2_mv_x;
                    ps_subpel_refine_ctxt->i2_mv_y[0][index] = ps_result_prms->i2_mv_y;
                    ps_subpel_refine_ctxt->i2_ref_idx[0][index] = ps_result_prms->i1_ref_idx;
                }
            }
        }
    }
}

WORD32 hme_evalsatd_pt_pu_16x16_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector)
{
    S32 ai4_satd_4x4[16]; /* num 4x4s in a 16x16 */
    S32 ai4_satd_8x8[4]; /* num 8x8s in a 16x16 */
    S32 ai4_tu_split_8x8[16];
    S32 i4_satd_16x16; /* 16x16 satd cost     */

    S32 ai4_tu_early_cbf_8x8[16];

    //S16 ai2_had_out[256];
    S16 *pi2_had_out;
    S32 tu_split_flag = 0;
    S32 early_cbf_flag = 0;
    S32 total_satd_cost = 0;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *api4_tu_split[HAD_32x32 + 1];
    S32 *api4_tu_early_cbf[HAD_32x32 + 1];

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    /* Initialize tu_split_cost to "0" */
    ps_prms->i4_tu_split_cost = 0;

    pi2_had_out = (S16 *)ps_prms->pu1_wkg_mem;

    api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
    api4_satd_pu[HAD_16x16] = &i4_satd_16x16;
    api4_satd_pu[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    api4_tu_split[HAD_4x4] = NULL;
    api4_tu_split[HAD_8x8] = &ai4_tu_split_8x8[0];
    api4_tu_split[HAD_16x16] = &tu_split_flag;
    api4_tu_split[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    api4_tu_early_cbf[HAD_4x4] = NULL;
    api4_tu_early_cbf[HAD_8x8] = &ai4_tu_early_cbf_8x8[0];
    api4_tu_early_cbf[HAD_16x16] = &early_cbf_flag;
    api4_tu_early_cbf[HAD_32x32] = NULL; /* 32x32 not used for 16x16 subpel refine */

    /* Call recursive 16x16 HAD module; updates satds for 4x4, 8x8 and 16x16 */
    ps_func_selector->pf_had_16x16_r(
        pu1_inp,
        inp_stride,
        pu1_ref,
        ref_stride,
        pi2_had_out,
        16,
        api4_satd_pu,
        api4_tu_split,
        api4_tu_early_cbf,
        0,
        4,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        0,
        ps_prms->u1_max_tr_depth,
        ps_prms->u1_max_tr_size,
        &(ps_prms->i4_tu_split_cost),
        NULL);

    total_satd_cost = i4_satd_16x16;

    ps_prms->pi4_tu_split_flags[0] = tu_split_flag;

    ps_prms->pi4_tu_early_cbf[0] = early_cbf_flag;

    return total_satd_cost;
}

/**
********************************************************************************
*  @fn     S32 hme_evalsatd_pt_pu_32x32
*
*  @brief  Evaluates the SATD with partial updates for all the best partitions
*          of a 32x32 CU based on recursive Hadamard 16x16, 8x8 and 4x4 satds
*
*  @param[inout]  ps_prms: error prms containg current and ref ptr, strides,
*                 pointer to sad grid of each partitions
*
*  @return     None
********************************************************************************
*/
void hme_evalsatd_pt_pu_32x32(err_prms_t *ps_prms)
{
    //S32 ai4_satd_4x4[64];   /* num 4x4s in a 32x32 */
    S32 ai4_satd_8x8[16]; /* num 8x8s in a 32x32 */
    S32 ai4_satd_16x16[4]; /* num 16x16 in a 32x32 */
    S32 i4_satd_32x32;
    //    S16 ai2_had_out[32*32];
    U08 *pu1_src;
    U08 *pu1_pred;
    S32 i;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    //api4_satd_pu[HAD_4x4]   = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
    api4_satd_pu[HAD_16x16] = &ai4_satd_16x16[0];
    api4_satd_pu[HAD_32x32] = &i4_satd_32x32;

    /* 32x32 SATD is calculates as the sum of the 4 8x8's in the block */
    for(i = 0; i < 16; i++)
    {
        pu1_src = pu1_inp + ((i & 0x3) << 3) + ((i >> 2) * inp_stride * 8);

        pu1_pred = pu1_ref + ((i & 0x3) << 3) + ((i >> 2) * ref_stride * 8);

        ai4_satd_8x8[i] = ps_prms->ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
            pu1_src, inp_stride, pu1_pred, ref_stride, NULL, 1);
    }

    /* Modified to cost calculation using only 8x8 SATD for 32x32*/
    ai4_satd_16x16[0] = ai4_satd_8x8[0] + ai4_satd_8x8[1] + ai4_satd_8x8[4] + ai4_satd_8x8[5];
    ai4_satd_16x16[1] = ai4_satd_8x8[2] + ai4_satd_8x8[3] + ai4_satd_8x8[6] + ai4_satd_8x8[7];
    ai4_satd_16x16[2] = ai4_satd_8x8[8] + ai4_satd_8x8[9] + ai4_satd_8x8[12] + ai4_satd_8x8[13];
    ai4_satd_16x16[3] = ai4_satd_8x8[10] + ai4_satd_8x8[11] + ai4_satd_8x8[14] + ai4_satd_8x8[15];

    /* Update 32x32 SATD */
    pi4_sad_grid[PART_ID_2Nx2N] =
        ai4_satd_16x16[0] + ai4_satd_16x16[1] + ai4_satd_16x16[2] + ai4_satd_16x16[3];

    /* Update 16x16 SATDs */
    pi4_sad_grid[PART_ID_NxN_TL] = ai4_satd_16x16[0];
    pi4_sad_grid[PART_ID_NxN_TR] = ai4_satd_16x16[1];
    pi4_sad_grid[PART_ID_NxN_BL] = ai4_satd_16x16[2];
    pi4_sad_grid[PART_ID_NxN_BR] = ai4_satd_16x16[3];

    /* Update 16x32 / 32x16 SATDs */
    pi4_sad_grid[PART_ID_Nx2N_L] = ai4_satd_16x16[0] + ai4_satd_16x16[2];
    pi4_sad_grid[PART_ID_Nx2N_R] = ai4_satd_16x16[1] + ai4_satd_16x16[3];
    pi4_sad_grid[PART_ID_2NxN_T] = ai4_satd_16x16[0] + ai4_satd_16x16[1];
    pi4_sad_grid[PART_ID_2NxN_B] = ai4_satd_16x16[2] + ai4_satd_16x16[3];

    /* Update AMP SATDs 32x24,32x8, 24x32,8x32  */
    pi4_sad_grid[PART_ID_nLx2N_L] =
        ai4_satd_8x8[0] + ai4_satd_8x8[4] + ai4_satd_8x8[8] + ai4_satd_8x8[12];

    pi4_sad_grid[PART_ID_nLx2N_R] = ai4_satd_8x8[1] + ai4_satd_8x8[5] + ai4_satd_8x8[9] +
                                    ai4_satd_8x8[13] + pi4_sad_grid[PART_ID_Nx2N_R];

    pi4_sad_grid[PART_ID_nRx2N_L] = ai4_satd_8x8[2] + ai4_satd_8x8[6] + ai4_satd_8x8[10] +
                                    ai4_satd_8x8[14] + pi4_sad_grid[PART_ID_Nx2N_L];

    pi4_sad_grid[PART_ID_nRx2N_R] =
        ai4_satd_8x8[3] + ai4_satd_8x8[7] + ai4_satd_8x8[11] + ai4_satd_8x8[15];

    pi4_sad_grid[PART_ID_2NxnU_T] =
        ai4_satd_8x8[0] + ai4_satd_8x8[1] + ai4_satd_8x8[2] + ai4_satd_8x8[3];

    pi4_sad_grid[PART_ID_2NxnU_B] = ai4_satd_8x8[4] + ai4_satd_8x8[5] + ai4_satd_8x8[6] +
                                    ai4_satd_8x8[7] + pi4_sad_grid[PART_ID_2NxN_B];

    pi4_sad_grid[PART_ID_2NxnD_T] = ai4_satd_8x8[8] + ai4_satd_8x8[9] + ai4_satd_8x8[10] +
                                    ai4_satd_8x8[11] + pi4_sad_grid[PART_ID_2NxN_T];

    pi4_sad_grid[PART_ID_2NxnD_B] =
        ai4_satd_8x8[12] + ai4_satd_8x8[13] + ai4_satd_8x8[14] + ai4_satd_8x8[15];
}

WORD32 hme_evalsatd_pt_pu_32x32_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector)
{
    S32 ai4_satd_4x4[64]; /* num 4x4s in a 32x32 */
    S32 ai4_satd_8x8[16]; /* num 8x8s in a 32x32 */
    S32 ai4_tu_split_8x8[16];
    S32 ai4_satd_16x16[4]; /* num 16x16 in a 32x32 */
    S32 ai4_tu_split_16x16[4];
    S32 i4_satd_32x32;

    S32 ai4_tu_early_cbf_8x8[16];
    S32 ai4_tu_early_cbf_16x16[4];
    S32 early_cbf_flag;

    S16 *pi2_had_out;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *api4_tu_split[HAD_32x32 + 1];
    S32 *api4_tu_early_cbf[HAD_32x32 + 1];

    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;
    S32 *pi4_tu_split_flag = ps_prms->pi4_tu_split_flags;
    S32 *pi4_tu_early_cbf = ps_prms->pi4_tu_early_cbf;

    S32 tu_split_flag = 0;
    S32 total_satd_cost = 0;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    /* Initialize tu_split_cost to "0" */
    ps_prms->i4_tu_split_cost = 0;

    pi2_had_out = (S16 *)ps_prms->pu1_wkg_mem;

    api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
    api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
    api4_satd_pu[HAD_16x16] = &ai4_satd_16x16[0];
    api4_satd_pu[HAD_32x32] = &i4_satd_32x32;

    api4_tu_split[HAD_4x4] = NULL;
    api4_tu_split[HAD_8x8] = &ai4_tu_split_8x8[0];
    api4_tu_split[HAD_16x16] = &ai4_tu_split_16x16[0];
    api4_tu_split[HAD_32x32] = &tu_split_flag;

    api4_tu_early_cbf[HAD_4x4] = NULL;
    api4_tu_early_cbf[HAD_8x8] = &ai4_tu_early_cbf_8x8[0];
    api4_tu_early_cbf[HAD_16x16] = &ai4_tu_early_cbf_16x16[0];
    api4_tu_early_cbf[HAD_32x32] = &early_cbf_flag;

    /* Call recursive 32x32 HAD module; updates satds for 4x4, 8x8, 16x16 and 32x32 */
    ihevce_had_32x32_r(
        pu1_inp,
        inp_stride,
        pu1_ref,
        ref_stride,
        pi2_had_out,
        32,
        api4_satd_pu,
        api4_tu_split,
        api4_tu_early_cbf,
        0,
        8,
        lambda,
        lambda_q_shift,
        i4_frm_qstep,
        0,
        ps_prms->u1_max_tr_depth,
        ps_prms->u1_max_tr_size,
        &(ps_prms->i4_tu_split_cost),
        ps_func_selector);

    total_satd_cost = i4_satd_32x32;

    /*The structure of the TU_SPLIT flag for the current 32x32 is as follows
    TL_16x16 - 5bits (4 for child and LSBit for 16x16 split)
    TR_16x16 - 5bits (4 for child and LSBit for 16x16 split)
    BL_16x16 - 5bits (4 for child and LSBit for 16x16 split)
    BR_16x16 - 5bits (4 for child and LSBit for 16x16 split)
    32x32_split - 1bit (LSBit)

    TU_SPLIT : (TL_16x16)_(TR_16x16)_(BL_16x16)_(BR_16x16)_32x32_split (21bits)*/

    pi4_sad_grid[PART_ID_2Nx2N] = total_satd_cost;
    pi4_tu_split_flag[PART_ID_2Nx2N] = tu_split_flag;
    pi4_tu_early_cbf[PART_ID_2Nx2N] = early_cbf_flag;

    return total_satd_cost;
}

/**
********************************************************************************
*  @fn     S32 hme_evalsatd_pt_pu_64x64
*
*  @brief  Evaluates the SATD with partial updates for all the best partitions
*          of a 64x64 CU based on accumulated Hadamard 32x32 and 16x16 satds
*
*           Note : 64x64 SATD does not do hadamard Transform using 32x32 hadamard
*                  outputs but directly uses four 32x32 SATD and 16 16x16 SATDS as
*                  TU size of 64 is not supported in HEVC
*
*  @param[inout]  ps_prms: error prms containg current and ref ptr, strides,
*                 pointer to sad grid of each partitions
*
*  @return     None
********************************************************************************
*/

void hme_evalsatd_pt_pu_64x64(err_prms_t *ps_prms)
{
    //S32 ai4_satd_4x4[4][64];   /* num 4x4s in a 32x32 * num 32x32 in 64x64 */
    S32 ai4_satd_8x8[4][16]; /* num 8x8s in a 32x32 * num 32x32 in 64x64 */
    S32 ai4_satd_16x16[4][4]; /* num 16x16 in a 32x32* num 32x32 in 64x64 */
    S32 ai4_satd_32x32[4]; /* num 32x32 in 64x64 */
    //    S16 ai2_had_out[32*32];
    S32 i, j;

    //  S32 ai4_tu_split_8x8[4][16];
    //  S32 ai4_tu_split_16x16[4][4];
    //  S32 ai4_tu_split_32x32[4];

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    //  S32 *api4_tu_split[HAD_32x32 + 1];

    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;
    U08 *pu1_src;
    U08 *pu1_pred;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    for(i = 0; i < 4; i++)
    {
        S32 blkx = (i & 0x1);
        S32 blky = (i >> 1);
        U08 *pu1_pi0, *pu1_pi1;

        //api4_satd_pu[HAD_4x4]   = &ai4_satd_4x4[i][0];
        api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[i][0];
        api4_satd_pu[HAD_16x16] = &ai4_satd_16x16[i][0];
        api4_satd_pu[HAD_32x32] = &ai4_satd_32x32[i];

        pu1_pi0 = pu1_inp + (blkx * 32) + (blky * 32 * inp_stride);
        pu1_pi1 = pu1_ref + (blkx * 32) + (blky * 32 * ref_stride);

        /* 64x64 SATD is calculates as the sum of the 4 16x16's in the block */
        for(j = 0; j < 16; j++)
        {
            pu1_src = pu1_pi0 + ((j & 0x3) << 3) + ((j >> 2) * inp_stride * 8);

            pu1_pred = pu1_pi1 + ((j & 0x3) << 3) + ((j >> 2) * ref_stride * 8);

            ai4_satd_8x8[i][j] = ps_prms->ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
                pu1_src, inp_stride, pu1_pred, ref_stride, NULL, 1);
        }

        /* Modified to cost calculation using only 8x8 SATD for 32x32*/
        ai4_satd_16x16[i][0] =
            ai4_satd_8x8[i][0] + ai4_satd_8x8[i][1] + ai4_satd_8x8[i][4] + ai4_satd_8x8[i][5];
        ai4_satd_16x16[i][1] =
            ai4_satd_8x8[i][2] + ai4_satd_8x8[i][3] + ai4_satd_8x8[i][6] + ai4_satd_8x8[i][7];
        ai4_satd_16x16[i][2] =
            ai4_satd_8x8[i][8] + ai4_satd_8x8[i][9] + ai4_satd_8x8[i][12] + ai4_satd_8x8[i][13];
        ai4_satd_16x16[i][3] =
            ai4_satd_8x8[i][10] + ai4_satd_8x8[i][11] + ai4_satd_8x8[i][14] + ai4_satd_8x8[i][15];
    }

    /* Modified to cost calculation using only 8x8 SATD for 32x32*/

    ai4_satd_32x32[0] =
        ai4_satd_16x16[0][0] + ai4_satd_16x16[0][1] + ai4_satd_16x16[0][2] + ai4_satd_16x16[0][3];
    ai4_satd_32x32[1] =
        ai4_satd_16x16[1][0] + ai4_satd_16x16[1][1] + ai4_satd_16x16[1][2] + ai4_satd_16x16[1][3];
    ai4_satd_32x32[2] =
        ai4_satd_16x16[2][0] + ai4_satd_16x16[2][1] + ai4_satd_16x16[2][2] + ai4_satd_16x16[2][3];
    ai4_satd_32x32[3] =
        ai4_satd_16x16[3][0] + ai4_satd_16x16[3][1] + ai4_satd_16x16[3][2] + ai4_satd_16x16[3][3];

    /* Update 64x64 SATDs */
    pi4_sad_grid[PART_ID_2Nx2N] =
        ai4_satd_32x32[0] + ai4_satd_32x32[1] + ai4_satd_32x32[2] + ai4_satd_32x32[3];

    /* Update 32x32 SATDs */
    pi4_sad_grid[PART_ID_NxN_TL] = ai4_satd_32x32[0];
    pi4_sad_grid[PART_ID_NxN_TR] = ai4_satd_32x32[1];
    pi4_sad_grid[PART_ID_NxN_BL] = ai4_satd_32x32[2];
    pi4_sad_grid[PART_ID_NxN_BR] = ai4_satd_32x32[3];

    /* Update 32x64 / 64x32 SATDs */
    pi4_sad_grid[PART_ID_Nx2N_L] = ai4_satd_32x32[0] + ai4_satd_32x32[2];
    pi4_sad_grid[PART_ID_Nx2N_R] = ai4_satd_32x32[1] + ai4_satd_32x32[3];
    pi4_sad_grid[PART_ID_2NxN_T] = ai4_satd_32x32[0] + ai4_satd_32x32[1];
    pi4_sad_grid[PART_ID_2NxN_B] = ai4_satd_32x32[2] + ai4_satd_32x32[3];

    /* Update AMP SATDs 64x48,64x16, 48x64,16x64  */
    pi4_sad_grid[PART_ID_nLx2N_L] =
        ai4_satd_16x16[0][0] + ai4_satd_16x16[0][2] + ai4_satd_16x16[2][0] + ai4_satd_16x16[2][2];

    pi4_sad_grid[PART_ID_nLx2N_R] = ai4_satd_16x16[0][1] + ai4_satd_16x16[0][3] +
                                    ai4_satd_16x16[2][1] + ai4_satd_16x16[2][3] +
                                    pi4_sad_grid[PART_ID_Nx2N_R];

    pi4_sad_grid[PART_ID_nRx2N_L] = ai4_satd_16x16[1][0] + ai4_satd_16x16[1][2] +
                                    ai4_satd_16x16[3][0] + ai4_satd_16x16[3][2] +
                                    pi4_sad_grid[PART_ID_Nx2N_L];

    pi4_sad_grid[PART_ID_nRx2N_R] =
        ai4_satd_16x16[1][1] + ai4_satd_16x16[1][3] + ai4_satd_16x16[3][1] + ai4_satd_16x16[3][3];

    pi4_sad_grid[PART_ID_2NxnU_T] =
        ai4_satd_16x16[0][0] + ai4_satd_16x16[0][1] + ai4_satd_16x16[1][0] + ai4_satd_16x16[1][1];

    pi4_sad_grid[PART_ID_2NxnU_B] = ai4_satd_16x16[0][2] + ai4_satd_16x16[0][3] +
                                    ai4_satd_16x16[1][2] + ai4_satd_16x16[1][3] +
                                    pi4_sad_grid[PART_ID_2NxN_B];

    pi4_sad_grid[PART_ID_2NxnD_T] = ai4_satd_16x16[2][0] + ai4_satd_16x16[2][1] +
                                    ai4_satd_16x16[3][0] + ai4_satd_16x16[3][1] +
                                    pi4_sad_grid[PART_ID_2NxN_T];

    pi4_sad_grid[PART_ID_2NxnD_B] =
        ai4_satd_16x16[2][2] + ai4_satd_16x16[2][3] + ai4_satd_16x16[3][2] + ai4_satd_16x16[3][3];
}

WORD32 hme_evalsatd_pt_pu_64x64_tu_rec(
    err_prms_t *ps_prms,
    WORD32 lambda,
    WORD32 lambda_q_shift,
    WORD32 i4_frm_qstep,
    me_func_selector_t *ps_func_selector)
{
    S32 ai4_satd_4x4[64]; /* num 4x4s in a 32x32 * num 32x32 in 64x64 */
    S32 ai4_satd_8x8[16]; /* num 8x8s in a 32x32 * num 32x32 in 64x64 */
    S32 ai4_satd_16x16[4]; /* num 16x16 in a 32x32* num 32x32 in 64x64 */
    S32 ai4_satd_32x32[4]; /* num 32x32 in 64x64 */

    S32 ai4_tu_split_8x8[16];
    S32 ai4_tu_split_16x16[4];

    S32 ai4_tu_early_cbf_8x8[16];
    S32 ai4_tu_early_cbf_16x16[4];

    S16 *pi2_had_out;
    S32 i;

    /* Initialize array of ptrs to hold partial SATDs at all levels of 16x16 */
    S32 *api4_satd_pu[HAD_32x32 + 1];
    S32 *api4_tu_split[HAD_32x32 + 1];
    S32 *api4_tu_early_cbf[HAD_32x32 + 1];

    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;

    S32 tu_split_flag = 0;
    S32 total_satd_cost = 0;

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    /* Initialize tu_split_cost to "0" */
    ps_prms->i4_tu_split_cost = 0;

    pi2_had_out = (S16 *)ps_prms->pu1_wkg_mem;

    for(i = 0; i < 4; i++)
    {
        S32 blkx = (i & 0x1);
        S32 blky = (i >> 1);
        U08 *pu1_pi0, *pu1_pi1;
        tu_split_flag = 0;

        api4_satd_pu[HAD_4x4] = &ai4_satd_4x4[0];
        api4_satd_pu[HAD_8x8] = &ai4_satd_8x8[0];
        api4_satd_pu[HAD_16x16] = &ai4_satd_16x16[0];
        api4_satd_pu[HAD_32x32] = &ai4_satd_32x32[i];

        api4_tu_split[HAD_4x4] = NULL;
        api4_tu_split[HAD_8x8] = &ai4_tu_split_8x8[0];
        api4_tu_split[HAD_16x16] = &ai4_tu_split_16x16[0];
        api4_tu_split[HAD_32x32] = &ps_prms->pi4_tu_split_flags[i];

        api4_tu_early_cbf[HAD_4x4] = NULL;
        api4_tu_early_cbf[HAD_8x8] = &ai4_tu_early_cbf_8x8[0];
        api4_tu_early_cbf[HAD_16x16] = &ai4_tu_early_cbf_16x16[0];
        api4_tu_early_cbf[HAD_32x32] = &ps_prms->pi4_tu_early_cbf[i];

        pu1_pi0 = pu1_inp + (blkx * 32) + (blky * 32 * inp_stride);
        pu1_pi1 = pu1_ref + (blkx * 32) + (blky * 32 * ref_stride);

        /* Call recursive 32x32 HAD module; updates satds for 4x4, 8x8, 16x16 and 32x32 */
        ihevce_had_32x32_r(
            pu1_pi0,
            inp_stride,
            pu1_pi1,
            ref_stride,
            pi2_had_out,
            32,
            api4_satd_pu,
            api4_tu_split,
            api4_tu_early_cbf,
            0,
            8,
            lambda,
            lambda_q_shift,
            i4_frm_qstep,
            1,
            ps_prms->u1_max_tr_depth,
            ps_prms->u1_max_tr_size,
            &(ps_prms->i4_tu_split_cost),
            ps_func_selector);
    }

    total_satd_cost = ai4_satd_32x32[0] + ai4_satd_32x32[1] + ai4_satd_32x32[2] + ai4_satd_32x32[3];

    /* Update 64x64 SATDs */
    pi4_sad_grid[PART_ID_2Nx2N] =
        ai4_satd_32x32[0] + ai4_satd_32x32[1] + ai4_satd_32x32[2] + ai4_satd_32x32[3];

    return total_satd_cost;
}

/**
********************************************************************************
*  @fn     void hme_subpel_refine_search_node(search_node_t *ps_search_node,
*                                   hme_subpel_prms_t *ps_prms,
*                                   layer_ctxt_t *ps_curr_layer,
*                                   BLK_SIZE_T e_blk_size,
*                                   S32 x_off,
*                                   S32 y_off)
*
*  @brief  Refines a given partition within a CU
*
*  @param[in,out]  ps_search_node: supplies starting mv and also ref id.
*                   updated with the accurate subpel mv
*
*  @param[in]  ps_prms: subpel prms input to this function
*
*  @param[in]  ps_curr_layer : layer context
*
*  @param[in]  e_blk_size : Block size enumeration
*
*  @param[in]  x_off : x offset of the partition w.r.t. pic start
*
*  @param[in]  y_off : y offset of the partition w.r.t. pic start
*
*  @return None
********************************************************************************
*/

static __inline PF_SAD_RESULT_FXN_T hme_get_calc_sad_and_result_subpel_fxn(
    me_func_selector_t *ps_func_selector,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list,
    S32 i4_part_mask,
    U08 u1_use_satd,
    U08 u1_num_parts,
    U08 u1_num_results)
{
    PF_SAD_RESULT_FXN_T pf_err_compute;

    ASSERT((1 == u1_num_results) || (2 == u1_num_results));

    if(1 == u1_num_results)
    {
        if(u1_use_satd)
        {
            if(u1_num_parts == 1)
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_eq_1;
            }
            else if((u1_num_parts > 1) && (u1_num_parts <= 8))
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_9;
            }
            else
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_1_best_result_pt_pu_16x16_num_part_lt_17;
            }
        }
        else
        {
            if(u1_num_parts == 1)
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_1_best_result_subpel_num_part_eq_1;
            }
            else if(((i4_part_mask & ENABLE_SQUARE_PARTS) != 0) && (u1_num_parts == 5))
            {
                pf_err_compute =
                    ps_me_optimised_function_list->pf_calc_sad_and_1_best_result_subpel_square_parts;
            }
            else if((u1_num_parts > 1) && (u1_num_parts <= 8))
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_1_best_result_subpel_num_part_lt_9;
            }
            else
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_1_best_result_subpel_num_part_lt_17;
            }
        }
    }
    else
    {
        if(u1_use_satd)
        {
            if(u1_num_parts == 1)
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_eq_1;
            }
            else if((u1_num_parts > 1) && (u1_num_parts <= 8))
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_lt_9;
            }
            else
            {
                pf_err_compute =
                    ps_func_selector->pf_evalsatd_update_2_best_results_pt_pu_16x16_num_part_lt_17;
            }
        }
        else
        {
            if(u1_num_parts == 1)
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_2_best_results_subpel_num_part_eq_1;
            }
            else if(((i4_part_mask & ENABLE_SQUARE_PARTS) != 0) && (u1_num_parts == 5))
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_2_best_results_subpel_square_parts;
            }
            else if((u1_num_parts > 1) && (u1_num_parts <= 8))
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_2_best_results_subpel_num_part_lt_9;
            }
            else
            {
                pf_err_compute = ps_me_optimised_function_list
                                     ->pf_calc_sad_and_2_best_results_subpel_num_part_lt_17;
            }
        }
    }

    return pf_err_compute;
}

#if DIAMOND_GRID == 1
S32 hme_subpel_refine_search_node_high_speed(
    search_node_t *ps_search_node,
    hme_subpel_prms_t *ps_prms,
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_blk_size,
    S32 x_off,
    S32 y_off,
    search_results_t *ps_search_results,
    S32 pred_lx,
    S32 i4_part_mask,
    S32 *pi4_valid_part_ids,
    S32 search_idx,
    subpel_dedup_enabler_t *ps_dedup_enabler,
    me_func_selector_t *ps_func_selector,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    S32 i4_num_hpel_refine, i4_num_qpel_refine;
    S32 i4_offset, i4_grid_mask;
    S08 i1_ref_idx;
    S32 i4_blk_wd, i4_blk_ht;
    S32 i4_ref_stride, i4_i;
    pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
    result_upd_prms_t s_result_prms;
    search_node_t s_temp_search_node;

    /*************************************************************************/
    /* Tracks current MV with the fractional component.                      */
    /*************************************************************************/
    S32 i4_mv_x, i4_mv_y;
    S32 i4_frac_x, i4_frac_y;

    /*************************************************************************/
    /* Function pointer for SAD/SATD, array and prms structure to pass to    */
    /* This function                                                         */
    /*************************************************************************/
    PF_SAD_RESULT_FXN_T pf_err_compute;

    S32 ai4_sad_grid[17], i4_tot_cost;
    err_prms_t s_err_prms;

    /*************************************************************************/
    /* Allowed MV RANGE                                                      */
    /*************************************************************************/
    range_prms_t *ps_range_prms;

    /*************************************************************************/
    /* stores min id in grid with associated min cost.                       */
    /*************************************************************************/
    S32 i4_min_cost, i4_min_sad;
    GRID_PT_T e_min_id;

    PF_INTERP_FXN_T pf_qpel_interp;
    /*************************************************************************/
    /* For hpel and qpel we move in diamonds and hence each point in the     */
    /* diamond will belong to a completely different plane. To simplify the  */
    /* look up of the ref ptr, we declare a 2x2 array of ref ptrs for the    */
    /* hpel planes which are interpolated during recon.                      */
    /*************************************************************************/
    U08 *apu1_hpel_ref[4], *pu1_ref;

    interp_prms_t s_interp_prms;

    /*************************************************************************/
    /* Maintains the minimum id of interpolated buffers, and the pointer that*/
    /* points to the corresponding predicted buf with its stride.            */
    /* Note that the pointer cannot be derived just from the id, since the   */
    /* pointer may also point to the hpel buffer (in case we request interp  */
    /* of a hpel pt, which already exists in the recon hpel planes)          */
    /*************************************************************************/
    U08 *pu1_final_out;
    S32 i4_final_out_stride;
    S32 part_id;
    S32 check_for_duplicate = 0;

    subpel_refine_ctxt_t *ps_subpel_refine_ctxt = ps_prms->ps_subpel_refine_ctxt;

    S32 mvx_qpel;
    S32 mvy_qpel;

    pf_err_compute = hme_get_calc_sad_and_result_subpel_fxn(
        ps_func_selector,
        ps_me_optimised_function_list,
        i4_part_mask,
        ps_prms->i4_use_satd,
        ps_subpel_refine_ctxt->i4_num_valid_parts,
        ps_search_results->u1_num_results_per_part);

    i4_num_hpel_refine = ps_prms->i4_num_steps_hpel_refine;
    i4_num_qpel_refine = ps_prms->i4_num_steps_qpel_refine;

    /* Prediction contet should now deal with qpel units */
    HME_SET_MVPRED_RES(ps_pred_ctxt, MV_RES_QPEL);

    /* Buffer allocation for subpel */
    /* Current design is that there may be many partitions and different mvs */
    /* that attempt subpel refinemnt. While there is possibility of overlap, the */
    /* hashing to detect and avoid overlap may be very complex. So, currently,   */
    /* the only thing done is to store the eventual predicted buffer with every  */
    /* ctb node that holds the result of hte best subpel search */

    /* Compute the base pointer for input, interpolated buffers */
    /* The base pointers point as follows: */
    /* fx fy : 0, 0 :: fx, hy : 0, 0.5, hx, fy: 0.5, 0, hx, fy: 0.5, 0.5 */
    /* To these, we need to add the offset of the current node */
    i4_ref_stride = ps_curr_layer->i4_rec_stride;
    i4_offset = x_off + (y_off * i4_ref_stride);
    i1_ref_idx = ps_search_node->i1_ref_idx;

    apu1_hpel_ref[0] = ps_curr_layer->ppu1_list_rec_fxfy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[1] = ps_curr_layer->ppu1_list_rec_hxfy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[2] = ps_curr_layer->ppu1_list_rec_fxhy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[3] = ps_curr_layer->ppu1_list_rec_hxhy[i1_ref_idx] + i4_offset;

    /* Initialize result params used for partition update */
    s_result_prms.pf_mv_cost_compute = NULL;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.pi4_valid_part_ids = pi4_valid_part_ids;
    s_result_prms.i1_ref_idx = ps_search_node->i1_ref_idx;
    s_result_prms.u1_pred_lx = search_idx;
    s_result_prms.i4_part_mask = i4_part_mask;
    s_result_prms.ps_search_node_base = ps_search_node;
    s_result_prms.pi4_sad_grid = &ai4_sad_grid[0];
    s_result_prms.i4_grid_mask = 1;
    s_result_prms.ps_search_node = &s_temp_search_node;
    s_temp_search_node.i1_ref_idx = ps_search_node->i1_ref_idx;

    /* convert to hpel units */
    i4_mv_x = ps_search_node->s_mv.i2_mvx >> 1;
    i4_mv_y = ps_search_node->s_mv.i2_mvy >> 1;

    /* for first pt, we compute at all locations in the grid, 4 + 1 centre */
    ps_range_prms = ps_prms->aps_mv_range_qpel[i1_ref_idx];
    i4_grid_mask = (GRID_DIAMOND_ENABLE_ALL);
    i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 2, ps_range_prms);

    i4_min_cost = MAX_32BIT_VAL;
    i4_min_sad = MAX_32BIT_VAL;

    /*************************************************************************/
    /* Prepare the input params to SAD/SATD function. Note that input is     */
    /* passed from the calling funcion since it may be I (normal subpel      */
    /* refinement) or 2I - P0 in case of bidirect subpel refinement.         */
    /* Both cases are handled here.                                          */
    /*************************************************************************/
    s_err_prms.pu1_inp = (U08 *)ps_prms->pv_inp;
    s_err_prms.i4_inp_stride = ps_prms->i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = (ENABLE_2Nx2N);
    s_err_prms.i4_grid_mask = 1;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0];
    s_err_prms.i4_blk_wd = i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    s_err_prms.i4_blk_ht = i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];

    s_result_prms.ps_subpel_refine_ctxt = ps_subpel_refine_ctxt;

    part_id = ps_search_node->u1_part_id;
    for(i4_i = 0; i4_i < i4_num_hpel_refine; i4_i++)
    {
        e_min_id = PT_C;

        mvx_qpel = i4_mv_x << 1;
        mvy_qpel = i4_mv_y << 1;

        /* Central pt */
        if(i4_grid_mask & BIT_EN(PT_C))
        {
            //ps_search_node->i2_mv_x = (S16)i4_mv_x;
            //ps_search_node->i2_mv_x = (S16)i4_mv_y;
            /* central pt is i4_mv_x, i4_mv_y */
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel, mvy_qpel, check_for_duplicate);

            i4_frac_x = i4_mv_x & 1;
            i4_frac_y = i4_mv_y & 1;
            pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
            s_err_prms.pu1_ref = pu1_ref + (i4_mv_x >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);

            /* Update the mv's with the current candt motion vectors */
            s_result_prms.i2_mv_x = mvx_qpel;
            s_result_prms.i2_mv_y = mvy_qpel;
            s_temp_search_node.s_mv.i2_mvx = mvx_qpel;
            s_temp_search_node.s_mv.i2_mvy = mvy_qpel;

            pf_err_compute(&s_err_prms, &s_result_prms);

            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
            if(i4_tot_cost < i4_min_cost)
            {
                i4_min_cost = i4_tot_cost;
                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                e_min_id = PT_C;
                pu1_final_out = s_err_prms.pu1_ref;
            }
        }

        /* left pt */
        if(i4_grid_mask & BIT_EN(PT_L))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel - 2, mvy_qpel, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)((i4_mv_x - 1) << 1);
                ps_search_node->s_mv.i2_mvy = (S16)(i4_mv_y << 1);
                /* central pt is i4_mv_x - 1, i4_mv_y */
                i4_frac_x = (i4_mv_x - 1) & 1;  // same as (x-1)&1
                i4_frac_y = i4_mv_y & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + ((i4_mv_x - 1) >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);

                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel - 2;
                s_result_prms.i2_mv_y = mvy_qpel;
                s_temp_search_node.s_mv.i2_mvx = mvx_qpel - 2;
                s_temp_search_node.s_mv.i2_mvy = mvy_qpel;

                pf_err_compute(&s_err_prms, &s_result_prms);
                //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_L;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* top pt */
        if(i4_grid_mask & BIT_EN(PT_T))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel, mvy_qpel - 2, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)(i4_mv_x << 1);
                ps_search_node->s_mv.i2_mvy = (S16)((i4_mv_y - 1) << 1);
                /* top pt is i4_mv_x, i4_mv_y - 1 */
                i4_frac_x = i4_mv_x & 1;
                i4_frac_y = (i4_mv_y - 1) & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + (i4_mv_x >> 1) + (((i4_mv_y - 1) >> 1) * i4_ref_stride);

                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel;
                s_result_prms.i2_mv_y = mvy_qpel - 2;
                s_temp_search_node.s_mv.i2_mvx = mvx_qpel;
                s_temp_search_node.s_mv.i2_mvy = mvy_qpel - 2;

                pf_err_compute(&s_err_prms, &s_result_prms);
                //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_T;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* right pt */
        if(i4_grid_mask & BIT_EN(PT_R))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, num_unique_nodes, mvx_qpel + 2, mvy_qpel, check_for_duplicate);
            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)((i4_mv_x + 1) << 1);
                ps_search_node->s_mv.i2_mvy = (S16)(i4_mv_y << 1);
                /* right pt is i4_mv_x + 1, i4_mv_y */
                i4_frac_x = (i4_mv_x + 1) & 1;
                i4_frac_y = i4_mv_y & 1;

                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + ((i4_mv_x + 1) >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);

                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel + 2;
                s_result_prms.i2_mv_y = mvy_qpel;
                s_temp_search_node.s_mv.i2_mvx = mvx_qpel + 2;
                s_temp_search_node.s_mv.i2_mvy = mvy_qpel;

                pf_err_compute(&s_err_prms, &s_result_prms);
                //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_R;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* bottom pt */
        if(i4_grid_mask & BIT_EN(PT_B))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, num_unique_nodes, mvx_qpel, mvy_qpel + 2, check_for_duplicate);
            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = ((S16)i4_mv_x << 1);
                ps_search_node->s_mv.i2_mvy = ((S16)(i4_mv_y + 1) << 1);
                i4_frac_x = i4_mv_x & 1;
                i4_frac_y = (i4_mv_y + 1) & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + (i4_mv_x >> 1) + (((i4_mv_y + 1) >> 1) * i4_ref_stride);

                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel;
                s_result_prms.i2_mv_y = mvy_qpel + 2;
                s_temp_search_node.s_mv.i2_mvx = mvx_qpel;
                s_temp_search_node.s_mv.i2_mvy = mvy_qpel + 2;

                pf_err_compute(&s_err_prms, &s_result_prms);
                //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_B;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* Early exit in case of central point */
        if(e_min_id == PT_C)
            break;

        /*********************************************************************/
        /* Depending on the best result location, we may be able to skip     */
        /* atleast two pts, centre pt and one more pt. E.g. if right pt is   */
        /* the best result, the next iteration need not do centre, left pts  */
        /*********************************************************************/
        i4_grid_mask = gai4_opt_grid_mask_diamond[e_min_id];
        i4_mv_x += gai1_grid_id_to_x[e_min_id];
        i4_mv_y += gai1_grid_id_to_y[e_min_id];
        ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
        ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;
        i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 2, ps_range_prms);
    }

    /* Convert to QPEL units */
    i4_mv_x <<= 1;
    i4_mv_y <<= 1;

    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

    /* Exact interpolation or averaging chosen here */
    pf_qpel_interp = ps_prms->pf_qpel_interp;

    /* Next QPEL ME */
    /* In this case, we have option of doing exact QPEL interpolation or avg */
    /*************************************************************************/
    /*        x                                                              */
    /*    A b C d                                                            */
    /*    e f g h                                                            */
    /*    I j K l                                                            */
    /*    m n o p                                                            */
    /*    Q r S t                                                            */
    /*                                                                       */
    /*    Approximate QPEL logic                                             */
    /*    b = avg(A,C) f = avg(I,C), g= avg(C,K) j=avg(I,K)                  */
    /*    for any given pt, we can get all the information required about    */
    /*    the surrounding 4 pts. For example, given point C (0.5, 0)         */
    /*     surrounding pts info:                                             */
    /*     b : qpel offset: 1, 0, generated by averaging. buffer1: fpel buf  */
    /*           buffer 2: hxfy, offsets for both are 0, 0                   */
    /*    similarly for other pts the info can be gotten                     */
    /*************************************************************************/
    i4_grid_mask = GRID_DIAMOND_ENABLE_ALL ^ (BIT_EN(PT_C));
    i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 1, ps_range_prms);

    /*************************************************************************/
    /* One time preparation of non changing interpolation params. These      */
    /* include a set of ping pong result buf ptrs, input buf ptrs and some   */
    /* working memory (not used though in case of averaging).                */
    /*************************************************************************/
    s_interp_prms.ppu1_ref = &apu1_hpel_ref[0];
    s_interp_prms.i4_ref_stride = i4_ref_stride;
    s_interp_prms.i4_blk_wd = i4_blk_wd;
    s_interp_prms.i4_blk_ht = i4_blk_ht;

    i4_final_out_stride = i4_ref_stride;

    {
        U08 *pu1_mem;
        /*********************************************************************/
        /* Allocation of working memory for interpolated buffers. We maintain*/
        /* an intermediate working buffer, and 2 ping pong interpolated out  */
        /* buffers, purpose of ping pong explained later below               */
        /*********************************************************************/
        pu1_mem = ps_prms->pu1_wkg_mem;
        s_interp_prms.pu1_wkg_mem = pu1_mem;

        //pu1_mem += (INTERP_INTERMED_BUF_SIZE);
        s_interp_prms.apu1_interp_out[0] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[1] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[2] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[3] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[4] = pu1_mem;

        /*********************************************************************/
        /* Stride of interpolated output is just a function of blk width of  */
        /* this partition and hence remains constant for this partition      */
        /*********************************************************************/
        s_interp_prms.i4_out_stride = (i4_blk_wd);
    }

    {
        UWORD8 *apu1_final[4];
        WORD32 ai4_ref_stride[4];
        /*************************************************************************/
        /* Ping pong design for interpolated buffers. We use a min id, which     */
        /* tracks the id of the ppu1_interp_out that stores the best result.     */
        /* When new interp to be done, it uses 1 - bes result id to do the interp*/
        /* min id is toggled when any new result becomes the best result.        */
        /*************************************************************************/

        for(i4_i = 0; i4_i < i4_num_qpel_refine; i4_i++)
        {
            e_min_id = PT_C;

            mvx_qpel = i4_mv_x;
            mvy_qpel = i4_mv_y;
            hme_qpel_interp_comprehensive(
                &s_interp_prms,
                apu1_final,
                ai4_ref_stride,
                i4_mv_x,
                i4_mv_y,
                i4_grid_mask,
                ps_me_optimised_function_list);
            if(i4_grid_mask & BIT_EN(PT_L))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler,
                    num_unique_nodes,
                    mvx_qpel - 1,
                    mvy_qpel - 0,
                    check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x - 1;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

                    s_err_prms.pu1_ref = apu1_final[0];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[0];

                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel - 1;
                    s_result_prms.i2_mv_y = mvy_qpel;
                    s_temp_search_node.s_mv.i2_mvx = mvx_qpel - 1;
                    s_temp_search_node.s_mv.i2_mvy = mvy_qpel;

                    pf_err_compute(&s_err_prms, &s_result_prms);
                    //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_L;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            if(i4_grid_mask & BIT_EN(PT_T))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler,
                    num_unique_nodes,
                    mvx_qpel - 0,
                    mvy_qpel - 1,
                    check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y - 1;

                    s_err_prms.pu1_ref = apu1_final[1];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[1];

                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel;
                    s_result_prms.i2_mv_y = mvy_qpel - 1;

                    s_temp_search_node.s_mv.i2_mvx = mvx_qpel;
                    s_temp_search_node.s_mv.i2_mvy = mvy_qpel - 1;

                    pf_err_compute(&s_err_prms, &s_result_prms);

                    //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_T;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            if(i4_grid_mask & BIT_EN(PT_R))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, num_unique_nodes, mvx_qpel + 1, mvy_qpel, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x + 1;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

                    s_err_prms.pu1_ref = apu1_final[2];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[2];

                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel + 1;
                    s_result_prms.i2_mv_y = mvy_qpel;

                    s_temp_search_node.s_mv.i2_mvx = mvx_qpel + 1;
                    s_temp_search_node.s_mv.i2_mvy = mvy_qpel;

                    pf_err_compute(&s_err_prms, &s_result_prms);

                    //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_R;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            /* i4_mv_x and i4_mv_y will always be the centre pt */
            /* for qpel we  start with least hpel, and hence compute of center pt never reqd */
            if(i4_grid_mask & BIT_EN(PT_B))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, num_unique_nodes, mvx_qpel, mvy_qpel + 1, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y + 1;

                    s_err_prms.pu1_ref = apu1_final[3];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[3];

                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel;
                    s_result_prms.i2_mv_y = mvy_qpel + 1;

                    s_temp_search_node.s_mv.i2_mvx = mvx_qpel;
                    s_temp_search_node.s_mv.i2_mvy = mvy_qpel + 1;

                    pf_err_compute(&s_err_prms, &s_result_prms);

                    //hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_B;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }

            /* New QPEL mv x and y */
            if(e_min_id == PT_C)
                break;
            i4_grid_mask = gai4_opt_grid_mask_diamond[e_min_id];
            i4_mv_x += gai1_grid_id_to_x[e_min_id];
            i4_mv_y += gai1_grid_id_to_y[e_min_id];
            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;
            i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 1, ps_range_prms);
        }
    }

    /* update modified motion vectors and cost at end of subpel */
    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;
    ps_search_node->i4_tot_cost = i4_min_cost;
    ps_search_node->i4_sad = i4_min_sad;

    /********************************************************************************/
    /* TODO: Restoring back Sad lambda from Hadamard lambda                         */
    /* Need to pass the had/satd lambda in more cleaner way for subpel cost compute */
    /********************************************************************************/
    //ps_pred_ctxt->lambda >>= 1;

    return (i4_min_cost);
}
#elif DIAMOND_GRID == 0
S32 hme_subpel_refine_search_node_high_speed(
    search_node_t *ps_search_node,
    hme_subpel_prms_t *ps_prms,
    layer_ctxt_t *ps_curr_layer,
    BLK_SIZE_T e_blk_size,
    S32 x_off,
    S32 y_off,
    search_results_t *ps_search_results,
    S32 pred_lx,
    S32 i4_part_mask,
    S32 *pi4_valid_part_ids,
    S32 search_idx,
    subpel_dedup_enabler_t *ps_dedup_enabler,
    me_func_selector_t *ps_func_selector)
{
    S32 i4_num_hpel_refine, i4_num_qpel_refine;
    S32 i4_offset, i4_grid_mask;
    S08 i1_ref_idx;
    S32 i4_blk_wd, i4_blk_ht;
    S32 i4_ref_stride, i4_i;
    pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
    result_upd_prms_t s_result_prms;

    /*************************************************************************/
    /* Tracks current MV with the fractional component.                      */
    /*************************************************************************/
    S32 i4_mv_x, i4_mv_y;
    S32 i4_frac_x, i4_frac_y;

    /*************************************************************************/
    /* Function pointer for SAD/SATD, array and prms structure to pass to    */
    /* This function                                                         */
    /*************************************************************************/
    PF_SAD_FXN_T pf_err_compute;
    S32 ai4_sad_grid[9][17], i4_tot_cost;
    err_prms_t s_err_prms;

    /*************************************************************************/
    /* Allowed MV RANGE                                                      */
    /*************************************************************************/
    range_prms_t *ps_range_prms;

    /*************************************************************************/
    /* stores min id in grid with associated min cost.                       */
    /*************************************************************************/
    S32 i4_min_cost, i4_min_sad;
    GRID_PT_T e_min_id;

    PF_INTERP_FXN_T pf_qpel_interp;
    /*************************************************************************/
    /* For hpel and qpel we move in diamonds and hence each point in the     */
    /* diamond will belong to a completely different plane. To simplify the  */
    /* look up of the ref ptr, we declare a 2x2 array of ref ptrs for the    */
    /* hpel planes which are interpolated during recon.                      */
    /*************************************************************************/
    U08 *apu1_hpel_ref[4], *pu1_ref;

    interp_prms_t s_interp_prms;

    /*************************************************************************/
    /* Maintains the minimum id of interpolated buffers, and the pointer that*/
    /* points to the corresponding predicted buf with its stride.            */
    /* Note that the pointer cannot be derived just from the id, since the   */
    /* pointer may also point to the hpel buffer (in case we request interp  */
    /* of a hpel pt, which already exists in the recon hpel planes)          */
    /*************************************************************************/
    U08 *pu1_final_out;
    S32 i4_final_out_stride;
    S32 part_id;
    S32 check_for_duplicate = 0;

    S32 mvx_qpel;
    S32 mvy_qpel;

    /*************************************************************************/
    /* Appropriate Err compute fxn, depends on SAD/SATD, blk size and remains*/
    /* fixed through this subpel refinement for this partition.              */
    /* Note, we do not enable grid sads since each pt is different buffers.  */
    /* Hence, part mask is also nearly dont care and we use 2Nx2N enabled.   */
    /*************************************************************************/
    if(ps_prms->i4_use_satd)
    {
        pf_err_compute = hme_evalsatd_update_1_best_result_pt_pu_16x16;
    }
    else
    {
        pf_err_compute = hme_evalsad_grid_pu_16x16; /* hme_evalsad_pt_pu_16x16; */
    }

    i4_num_hpel_refine = ps_prms->i4_num_steps_hpel_refine;
    i4_num_qpel_refine = ps_prms->i4_num_steps_qpel_refine;

    /* Prediction contet should now deal with qpel units */
    HME_SET_MVPRED_RES(ps_pred_ctxt, MV_RES_QPEL);

    /* Buffer allocation for subpel */
    /* Current design is that there may be many partitions and different mvs */
    /* that attempt subpel refinemnt. While there is possibility of overlap, the */
    /* hashing to detect and avoid overlap may be very complex. So, currently,   */
    /* the only thing done is to store the eventual predicted buffer with every  */
    /* ctb node that holds the result of hte best subpel search */

    /* Compute the base pointer for input, interpolated buffers */
    /* The base pointers point as follows:
    /* fx fy : 0, 0 :: fx, hy : 0, 0.5, hx, fy: 0.5, 0, hx, fy: 0.5, 0.5 */
    /* To these, we need to add the offset of the current node */
    i4_ref_stride = ps_curr_layer->i4_rec_stride;
    i4_offset = x_off + (y_off * i4_ref_stride);
    i1_ref_idx = ps_search_node->i1_ref_idx;

    apu1_hpel_ref[0] = ps_curr_layer->ppu1_list_rec_fxfy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[1] = ps_curr_layer->ppu1_list_rec_hxfy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[2] = ps_curr_layer->ppu1_list_rec_fxhy[i1_ref_idx] + i4_offset;
    apu1_hpel_ref[3] = ps_curr_layer->ppu1_list_rec_hxhy[i1_ref_idx] + i4_offset;

    /* Initialize result params used for partition update */
    s_result_prms.pf_mv_cost_compute = NULL;
    s_result_prms.ps_search_results = ps_search_results;
    s_result_prms.pi4_valid_part_ids = pi4_valid_part_ids;
    s_result_prms.i1_ref_idx = search_idx;
    s_result_prms.i4_part_mask = i4_part_mask;
    s_result_prms.ps_search_node_base = ps_search_node;
    s_result_prms.pi4_sad_grid = &ai4_sad_grid[0][0];
    s_result_prms.i4_grid_mask = 1;

    /* convert to hpel units */
    i4_mv_x = ps_search_node->s_mv.i2_mvx >> 1;
    i4_mv_y = ps_search_node->s_mv.i2_mvy >> 1;

    /* for first pt, we compute at all locations in the grid, 4 + 1 centre */
    ps_range_prms = ps_prms->ps_mv_range_qpel;
    i4_grid_mask = (GRID_ALL_PTS_VALID);
    i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 2, ps_range_prms);

    i4_min_cost = MAX_32BIT_VAL;
    i4_min_sad = MAX_32BIT_VAL;

    /*************************************************************************/
    /* Prepare the input params to SAD/SATD function. Note that input is     */
    /* passed from the calling funcion since it may be I (normal subpel      */
    /* refinement) or 2I - P0 in case of bidirect subpel refinement.         */
    /* Both cases are handled here.                                          */
    /*************************************************************************/
    s_err_prms.pu1_inp = (U08 *)ps_prms->pv_inp;
    s_err_prms.i4_inp_stride = ps_prms->i4_inp_stride;
    s_err_prms.i4_ref_stride = i4_ref_stride;
    s_err_prms.i4_part_mask = (ENABLE_2Nx2N);
    s_err_prms.i4_grid_mask = 1;
    s_err_prms.pi4_sad_grid = &ai4_sad_grid[0][0];
    s_err_prms.i4_blk_wd = i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
    s_err_prms.i4_blk_ht = i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];

    /* TODO: Currently doubling lambda for Hadamard Sad instead of 1.9*sadlambda */
    //ps_pred_ctxt->lambda <<= 1;
    part_id = ps_search_node->u1_part_id;
    for(i4_i = 0; i4_i < i4_num_hpel_refine; i4_i++)
    {
        e_min_id = PT_C;

        mvx_qpel = i4_mv_x << 1;
        mvy_qpel = i4_mv_y << 1;

        /* Central pt */
        if(i4_grid_mask & BIT_EN(PT_C))
        {
            //ps_search_node->i2_mv_x = (S16)i4_mv_x;
            //ps_search_node->i2_mv_x = (S16)i4_mv_y;
            /* central pt is i4_mv_x, i4_mv_y */
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel, mvy_qpel, check_for_duplicate);

            i4_frac_x = i4_mv_x & 1;
            i4_frac_y = i4_mv_y & 1;
            pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
            s_err_prms.pu1_ref = pu1_ref + (i4_mv_x >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);
            pf_err_compute(&s_err_prms);
            /* Update the mv's with the current candt motion vectors */
            s_result_prms.i2_mv_x = mvx_qpel;
            s_result_prms.i2_mv_y = mvy_qpel;
            hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
            if(i4_tot_cost < i4_min_cost)
            {
                i4_min_cost = i4_tot_cost;
                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                e_min_id = PT_C;
                pu1_final_out = s_err_prms.pu1_ref;
            }
        }

        /* left pt */
        if(i4_grid_mask & BIT_EN(PT_L))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel - 2, mvy_qpel, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)((i4_mv_x - 1) << 1);
                ps_search_node->s_mv.i2_mvy = (S16)(i4_mv_y << 1);
                /* central pt is i4_mv_x - 1, i4_mv_y */
                i4_frac_x = (i4_mv_x - 1) & 1;  // same as (x-1)&1
                i4_frac_y = i4_mv_y & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + ((i4_mv_x - 1) >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);

                pf_err_compute(&s_err_prms);
                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel;
                s_result_prms.i2_mv_y = mvy_qpel;
                hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_L;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* top pt */
        if(i4_grid_mask & BIT_EN(PT_T))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel, mvy_qpel - 2, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)(i4_mv_x << 1);
                ps_search_node->s_mv.i2_mvy = (S16)((i4_mv_y - 1) << 1);
                /* top pt is i4_mv_x, i4_mv_y - 1 */
                i4_frac_x = i4_mv_x & 1;
                i4_frac_y = (i4_mv_y - 1) & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + (i4_mv_x >> 1) + (((i4_mv_y - 1) >> 1) * i4_ref_stride);
                pf_err_compute(&s_err_prms);
                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel;
                s_result_prms.i2_mv_y = mvy_qpel - 2;
                hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_T;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* right pt */
        if(i4_grid_mask & BIT_EN(PT_R))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel + 2, mvy_qpel, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = (S16)((i4_mv_x + 1) << 1);
                ps_search_node->s_mv.i2_mvy = (S16)(i4_mv_y << 1);
                /* right pt is i4_mv_x + 1, i4_mv_y */
                i4_frac_x = (i4_mv_x + 1) & 1;
                i4_frac_y = i4_mv_y & 1;

                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + ((i4_mv_x + 1) >> 1) + ((i4_mv_y >> 1) * i4_ref_stride);
                pf_err_compute(&s_err_prms);
                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel + 2;
                s_result_prms.i2_mv_y = mvy_qpel;
                hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_R;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        /* bottom pt */
        if(i4_grid_mask & BIT_EN(PT_B))
        {
            CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                ps_dedup_enabler, 1, mvx_qpel, mvy_qpel + 2, check_for_duplicate);

            if(!check_for_duplicate)
            {
                /* search node mv is stored in qpel units */
                ps_search_node->s_mv.i2_mvx = ((S16)i4_mv_x << 1);
                ps_search_node->s_mv.i2_mvy = ((S16)(i4_mv_y + 1) << 1);
                i4_frac_x = i4_mv_x & 1;
                i4_frac_y = (i4_mv_y + 1) & 1;
                pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                s_err_prms.pu1_ref =
                    pu1_ref + (i4_mv_x >> 1) + (((i4_mv_y + 1) >> 1) * i4_ref_stride);

                pf_err_compute(&s_err_prms);
                /* Update the mv's with the current candt motion vectors */
                s_result_prms.i2_mv_x = mvx_qpel;
                s_result_prms.i2_mv_y = mvy_qpel + 2;
                hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                if(i4_tot_cost < i4_min_cost)
                {
                    i4_min_cost = i4_tot_cost;
                    i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    e_min_id = PT_B;
                    pu1_final_out = s_err_prms.pu1_ref;
                }
            }
        }
        if(e_min_id == PT_C)
        {
            if(!i4_i)
            {
                /* TL pt */
                if(i4_grid_mask & BIT_EN(PT_TL))
                {
                    S32 mvx_minus_1 = (i4_mv_x - 1);
                    S32 mvy_minus_1 = (i4_mv_y - 1);

                    CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                        ps_dedup_enabler, 1, mvx_qpel - 2, mvy_qpel - 2, check_for_duplicate);

                    if(!check_for_duplicate)
                    {
                        /* search node mv is stored in qpel units */
                        ps_search_node->s_mv.i2_mvx = ((S16)mvx_minus_1 << 1);
                        ps_search_node->s_mv.i2_mvy = ((S16)mvy_minus_1 << 1);
                        i4_frac_x = mvx_minus_1 & 1;
                        i4_frac_y = mvy_minus_1 & 1;
                        pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                        s_err_prms.pu1_ref =
                            pu1_ref + (mvx_minus_1 >> 1) + ((mvy_minus_1 >> 1) * i4_ref_stride);

                        pf_err_compute(&s_err_prms);
                        /* Update the mv's with the current candt motion vectors */
                        s_result_prms.i2_mv_x = mvx_qpel - 2;
                        s_result_prms.i2_mv_y = mvy_qpel - 2;
                        hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                        i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                        if(i4_tot_cost < i4_min_cost)
                        {
                            i4_min_cost = i4_tot_cost;
                            i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            e_min_id = PT_TL;
                            pu1_final_out = s_err_prms.pu1_ref;
                        }
                    }
                }
                /* TR pt */
                if(i4_grid_mask & BIT_EN(PT_TR))
                {
                    S32 mvx_plus_1 = (i4_mv_x + 1);
                    S32 mvy_minus_1 = (i4_mv_y - 1);

                    CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                        ps_dedup_enabler, 1, mvx_qpel + 2, mvy_qpel - 2, check_for_duplicate);

                    if(!check_for_duplicate)
                    {
                        /* search node mv is stored in qpel units */
                        ps_search_node->s_mv.i2_mvx = ((S16)mvx_plus_1 << 1);
                        ps_search_node->s_mv.i2_mvy = ((S16)mvy_minus_1 << 1);
                        i4_frac_x = mvx_plus_1 & 1;
                        i4_frac_y = mvy_minus_1 & 1;
                        pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                        s_err_prms.pu1_ref =
                            pu1_ref + (mvx_plus_1 >> 1) + ((mvy_minus_1 >> 1) * i4_ref_stride);

                        pf_err_compute(&s_err_prms);
                        /* Update the mv's with the current candt motion vectors */
                        s_result_prms.i2_mv_x = mvx_qpel + 2;
                        s_result_prms.i2_mv_y = mvy_qpel - 2;
                        hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                        i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                        if(i4_tot_cost < i4_min_cost)
                        {
                            i4_min_cost = i4_tot_cost;
                            i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            e_min_id = PT_TR;
                            pu1_final_out = s_err_prms.pu1_ref;
                        }
                    }
                }
                /* BL pt */
                if(i4_grid_mask & BIT_EN(PT_BL))
                {
                    S32 mvx_minus_1 = (i4_mv_x - 1);
                    S32 mvy_plus_1 = (i4_mv_y + 1);

                    CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                        ps_dedup_enabler, 1, mvx_qpel - 2, mvy_qpel + 2, check_for_duplicate);

                    if(!check_for_duplicate)
                    {
                        /* search node mv is stored in qpel units */
                        ps_search_node->s_mv.i2_mvx = ((S16)mvx_minus_1 << 1);
                        ps_search_node->s_mv.i2_mvy = ((S16)mvy_plus_1 << 1);
                        i4_frac_x = mvx_minus_1 & 1;
                        i4_frac_y = mvy_plus_1 & 1;
                        pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                        s_err_prms.pu1_ref =
                            pu1_ref + (mvx_minus_1 >> 1) + ((mvy_plus_1 >> 1) * i4_ref_stride);

                        pf_err_compute(&s_err_prms);
                        /* Update the mv's with the current candt motion vectors */
                        s_result_prms.i2_mv_x = mvx_qpel - 2;
                        s_result_prms.i2_mv_y = mvy_qpel + 2;
                        hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                        i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                        if(i4_tot_cost < i4_min_cost)
                        {
                            i4_min_cost = i4_tot_cost;
                            i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            e_min_id = PT_BL;
                            pu1_final_out = s_err_prms.pu1_ref;
                        }
                    }
                }
                /* BR pt */
                if(i4_grid_mask & BIT_EN(PT_BR))
                {
                    S32 mvx_plus_1 = (i4_mv_x + 1);
                    S32 mvy_plus_1 = (i4_mv_y + 1);
                    CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                        ps_dedup_enabler, 1, mvx_qpel + 2, mvy_qpel + 2, check_for_duplicate);

                    if(!check_for_duplicate)
                    {
                        /* search node mv is stored in qpel units */
                        ps_search_node->s_mv.i2_mvx = ((S16)mvx_plus_1 << 1);
                        ps_search_node->s_mv.i2_mvy = ((S16)mvy_plus_1 << 1);
                        i4_frac_x = mvx_plus_1 & 1;
                        i4_frac_y = mvy_plus_1 & 1;
                        pu1_ref = apu1_hpel_ref[i4_frac_y * 2 + i4_frac_x];
                        s_err_prms.pu1_ref =
                            pu1_ref + (mvx_plus_1 >> 1) + ((mvy_plus_1 >> 1) * i4_ref_stride);

                        pf_err_compute(&s_err_prms);
                        /* Update the mv's with the current candt motion vectors */
                        s_result_prms.i2_mv_x = mvx_qpel + 2;
                        s_result_prms.i2_mv_y = mvy_qpel + 2;
                        hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                        i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                        if(i4_tot_cost < i4_min_cost)
                        {
                            i4_min_cost = i4_tot_cost;
                            i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            e_min_id = PT_BR;
                            pu1_final_out = s_err_prms.pu1_ref;
                        }
                    }
                }
                if(e_min_id == PT_C)
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }

        /*********************************************************************/
        /* Depending on the best result location, we may be able to skip     */
        /* atleast two pts, centre pt and one more pt. E.g. if right pt is   */
        /* the best result, the next iteration need not do centre, left pts  */
        /*********************************************************************/
        if(i4_i)
        {
            i4_grid_mask = gai4_opt_grid_mask_diamond[e_min_id];
        }
        else
        {
            i4_grid_mask = gai4_opt_grid_mask_conventional[e_min_id];
        }
        i4_mv_x += gai1_grid_id_to_x[e_min_id];
        i4_mv_y += gai1_grid_id_to_y[e_min_id];
        ps_search_node->s_mv.i2_mvx = (S16)(i4_mv_x << 1);
        ps_search_node->s_mv.i2_mvy = (S16)(i4_mv_y << 1);
        i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 2, ps_range_prms);
    }

    /* Convert to QPEL units */
    i4_mv_x <<= 1;
    i4_mv_y <<= 1;

    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

    /* Early exit if this partition is visiting same hpel mv again */
    /* Assumption : Checkin for early exit in best result of partition */
    if((ps_search_results->aps_part_results[search_idx][part_id][0].i2_best_hpel_mv_x ==
        ps_search_node->s_mv.i2_mvx) &&
       (ps_search_results->aps_part_results[search_idx][part_id][0].i2_best_hpel_mv_y ==
        ps_search_node->s_mv.i2_mvy))
    {
        return (ps_search_results->aps_part_results[search_idx][part_id][0].i4_tot_cost);
    }
    else
    {
        /* Store the best hpel mv for future early exit checks */
        ps_search_results->aps_part_results[search_idx][part_id][0].i2_best_hpel_mv_x =
            (S16)i4_mv_x;
        ps_search_results->aps_part_results[search_idx][part_id][0].i2_best_hpel_mv_y =
            (S16)i4_mv_y;
    }

    /* Early exit if this partition is visiting same hpel mv again */
    /* Assumption : Checkin for early exit in second best result of partition */
    if((ps_search_results->aps_part_results[search_idx][part_id][1].i2_best_hpel_mv_x ==
        ps_search_node->s_mv.i2_mvx) &&
       (ps_search_results->aps_part_results[search_idx][part_id][1].i2_best_hpel_mv_y ==
        ps_search_node->s_mv.i2_mvy))
    {
        return (ps_search_results->aps_part_results[search_idx][part_id][1].i4_tot_cost);
    }
    else
    {
        /* Store the best hpel mv for future early exit checks */
        ps_search_results->aps_part_results[search_idx][part_id][1].i2_best_hpel_mv_x =
            (S16)i4_mv_x;
        ps_search_results->aps_part_results[search_idx][part_id][1].i2_best_hpel_mv_y =
            (S16)i4_mv_y;
    }

    /* Exact interpolation or averaging chosen here */
    pf_qpel_interp = ps_prms->pf_qpel_interp;

    /* Next QPEL ME */
    /* In this case, we have option of doing exact QPEL interpolation or avg */
    /*************************************************************************/
    /*        x                                                              */
    /*    A b C d                                                            */
    /*    e f g h                                                            */
    /*    I j K l                                                            */
    /*    m n o p                                                            */
    /*    Q r S t                                                            */
    /*                                                                       */
    /*    Approximate QPEL logic                                             */
    /*    b = avg(A,C) f = avg(I,C), g= avg(C,K) j=avg(I,K)                  */
    /*    for any given pt, we can get all the information required about    */
    /*    the surrounding 4 pts. For example, given point C (0.5, 0)         */
    /*     surrounding pts info:                                             */
    /*     b : qpel offset: 1, 0, generated by averaging. buffer1: fpel buf  */
    /*           buffer 2: hxfy, offsets for both are 0, 0                   */
    /*    similarly for other pts the info can be gotten                     */
    /*************************************************************************/
    i4_grid_mask = GRID_ALL_PTS_VALID ^ (BIT_EN(PT_C));
    i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 1, ps_range_prms);

    /*************************************************************************/
    /* One time preparation of non changing interpolation params. These      */
    /* include a set of ping pong result buf ptrs, input buf ptrs and some   */
    /* working memory (not used though in case of averaging).                */
    /*************************************************************************/
    s_interp_prms.ppu1_ref = &apu1_hpel_ref[0];
    s_interp_prms.i4_ref_stride = i4_ref_stride;
    s_interp_prms.i4_blk_wd = i4_blk_wd;
    s_interp_prms.i4_blk_ht = i4_blk_ht;

    i4_final_out_stride = i4_ref_stride;

    {
        U08 *pu1_mem;
        /*********************************************************************/
        /* Allocation of working memory for interpolated buffers. We maintain*/
        /* an intermediate working buffer, and 2 ping pong interpolated out  */
        /* buffers, purpose of ping pong explained later below               */
        /*********************************************************************/
        pu1_mem = ps_prms->pu1_wkg_mem;
        s_interp_prms.pu1_wkg_mem = pu1_mem;

        //pu1_mem += (INTERP_INTERMED_BUF_SIZE);
        s_interp_prms.apu1_interp_out[0] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[1] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[2] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[3] = pu1_mem;

        pu1_mem += (INTERP_OUT_BUF_SIZE);
        s_interp_prms.apu1_interp_out[4] = pu1_mem;

        /*********************************************************************/
        /* Stride of interpolated output is just a function of blk width of  */
        /* this partition and hence remains constant for this partition      */
        /*********************************************************************/
        s_interp_prms.i4_out_stride = (i4_blk_wd);
    }

    {
        UWORD8 *apu1_final[4];
        WORD32 ai4_ref_stride[4];
        /*************************************************************************/
        /* Ping pong design for interpolated buffers. We use a min id, which     */
        /* tracks the id of the ppu1_interp_out that stores the best result.     */
        /* When new interp to be done, it uses 1 - bes result id to do the interp*/
        /* min id is toggled when any new result becomes the best result.        */
        /*************************************************************************/

        for(i4_i = 0; i4_i < i4_num_qpel_refine; i4_i++)
        {
            e_min_id = PT_C;

            hme_qpel_interp_comprehensive(
                &s_interp_prms, apu1_final, ai4_ref_stride, i4_mv_x, i4_mv_y, i4_grid_mask);

            mvx_qpel = i4_mv_x;
            mvy_qpel = i4_mv_y;

            if(i4_grid_mask & BIT_EN(PT_L))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, 1, mvx_qpel - 1, mvy_qpel - 0, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x - 1;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

                    s_err_prms.pu1_ref = apu1_final[0];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[0];

                    pf_err_compute(&s_err_prms);
                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel - 1;
                    s_result_prms.i2_mv_y = mvy_qpel;
                    hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_L;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            if(i4_grid_mask & BIT_EN(PT_T))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, 1, mvx_qpel - 0, mvy_qpel - 1, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y - 1;

                    s_err_prms.pu1_ref = apu1_final[1];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[1];

                    pf_err_compute(&s_err_prms);
                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel;
                    s_result_prms.i2_mv_y = mvy_qpel - 1;
                    hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_T;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            if(i4_grid_mask & BIT_EN(PT_R))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, 1, mvx_qpel + 1, mvy_qpel, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x + 1;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;

                    s_err_prms.pu1_ref = apu1_final[2];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[2];

                    pf_err_compute(&s_err_prms);
                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel + 1;
                    s_result_prms.i2_mv_y = mvy_qpel;
                    hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_R;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }
            /* i4_mv_x and i4_mv_y will always be the centre pt */
            /* for qpel we  start with least hpel, and hence compute of center pt never reqd */
            if(i4_grid_mask & BIT_EN(PT_B))
            {
                CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                    ps_dedup_enabler, 1, mvx_qpel, mvy_qpel + 1, check_for_duplicate);

                if(!check_for_duplicate)
                {
                    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
                    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y + 1;

                    s_err_prms.pu1_ref = apu1_final[3];
                    s_err_prms.i4_ref_stride = ai4_ref_stride[3];

                    pf_err_compute(&s_err_prms);
                    /* Update the mv's with the current candt motion vectors */
                    s_result_prms.i2_mv_x = mvx_qpel;
                    s_result_prms.i2_mv_y = mvy_qpel + 1;
                    hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);
                    i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];
                    if(i4_tot_cost < i4_min_cost)
                    {
                        e_min_id = PT_B;
                        i4_min_cost = i4_tot_cost;
                        i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                    }
                }
            }

            if(e_min_id == PT_C)
            {
                if(!i4_i)
                {
                    S32 i4_interp_buf_id = 0;

                    if(i4_grid_mask & BIT_EN(PT_TL))
                    {
                        CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                            ps_dedup_enabler, 1, mvx_qpel - 1, mvy_qpel - 1, check_for_duplicate);

                        if(!check_for_duplicate)
                        {
                            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x - 1;
                            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y - 1;

                            /* Carry out the interpolation */
                            pf_qpel_interp(
                                &s_interp_prms, i4_mv_x - 1, i4_mv_y - 1, i4_interp_buf_id);

                            s_err_prms.pu1_ref = s_interp_prms.pu1_final_out;
                            s_err_prms.i4_ref_stride = s_interp_prms.i4_final_out_stride;

                            pf_err_compute(&s_err_prms);
                            /* Update the mv's with the current candt motion vectors */
                            s_result_prms.i2_mv_x = mvx_qpel - 1;
                            s_result_prms.i2_mv_y = mvy_qpel - 1;
                            hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                            if(i4_tot_cost < i4_min_cost)
                            {
                                e_min_id = PT_TL;
                                i4_min_cost = i4_tot_cost;
                                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            }
                        }
                    }
                    if(i4_grid_mask & BIT_EN(PT_TR))
                    {
                        CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                            ps_dedup_enabler, 1, mvx_qpel + 1, mvy_qpel - 1, check_for_duplicate);

                        if(!check_for_duplicate)
                        {
                            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x + 1;
                            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y - 1;

                            /* Carry out the interpolation */
                            pf_qpel_interp(
                                &s_interp_prms, i4_mv_x + 1, i4_mv_y - 1, i4_interp_buf_id);

                            s_err_prms.pu1_ref = s_interp_prms.pu1_final_out;
                            s_err_prms.i4_ref_stride = s_interp_prms.i4_final_out_stride;

                            pf_err_compute(&s_err_prms);
                            /* Update the mv's with the current candt motion vectors */
                            s_result_prms.i2_mv_x = mvx_qpel + 1;
                            s_result_prms.i2_mv_y = mvy_qpel - 1;
                            hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                            if(i4_tot_cost < i4_min_cost)
                            {
                                e_min_id = PT_TR;
                                i4_min_cost = i4_tot_cost;
                                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            }
                        }
                    }
                    if(i4_grid_mask & BIT_EN(PT_BL))
                    {
                        CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                            ps_dedup_enabler, 1, mvx_qpel - 1, mvy_qpel + 1, check_for_duplicate);

                        if(!check_for_duplicate)
                        {
                            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x - 1;
                            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y + 1;

                            /* Carry out the interpolation */
                            pf_qpel_interp(
                                &s_interp_prms, i4_mv_x - 1, i4_mv_y + 1, i4_interp_buf_id);

                            s_err_prms.pu1_ref = s_interp_prms.pu1_final_out;
                            s_err_prms.i4_ref_stride = s_interp_prms.i4_final_out_stride;

                            pf_err_compute(&s_err_prms);
                            /* Update the mv's with the current candt motion vectors */
                            s_result_prms.i2_mv_x = mvx_qpel - 1;
                            s_result_prms.i2_mv_y = mvy_qpel + 1;
                            hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                            if(i4_tot_cost < i4_min_cost)
                            {
                                e_min_id = PT_BL;
                                i4_min_cost = i4_tot_cost;
                                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            }
                        }
                    }
                    /* i4_mv_x and i4_mv_y will always be the centre pt */
                    /* for qpel we  start with least hpel, and hence compute of center pt never reqd */
                    if(i4_grid_mask & BIT_EN(PT_BR))
                    {
                        CHECK_FOR_DUPES_AND_INSERT_UNIQUE_NODES(
                            ps_dedup_enabler, 1, mvx_qpel + 1, mvy_qpel + 1, check_for_duplicate);

                        if(!check_for_duplicate)
                        {
                            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x + 1;
                            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y + 1;

                            /* Carry out the interpolation */
                            pf_qpel_interp(
                                &s_interp_prms, i4_mv_x + 1, i4_mv_y + 1, i4_interp_buf_id);

                            s_err_prms.pu1_ref = s_interp_prms.pu1_final_out;
                            s_err_prms.i4_ref_stride = s_interp_prms.i4_final_out_stride;

                            pf_err_compute(&s_err_prms);
                            /* Update the mv's with the current candt motion vectors */
                            s_result_prms.i2_mv_x = mvx_qpel + 1;
                            s_result_prms.i2_mv_y = mvy_qpel + 1;
                            hme_update_results_pt_pu_best1_subpel_hs(&s_result_prms);

                            i4_tot_cost = s_err_prms.pi4_sad_grid[part_id];

                            if(i4_tot_cost < i4_min_cost)
                            {
                                e_min_id = PT_BR;
                                i4_min_cost = i4_tot_cost;
                                i4_min_sad = s_err_prms.pi4_sad_grid[part_id];
                            }
                        }
                    }
                    if(e_min_id == PT_C)
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if(i4_i)
            {
                i4_grid_mask = gai4_opt_grid_mask_diamond[e_min_id];
            }
            else
            {
                i4_grid_mask = gai4_opt_grid_mask_conventional[e_min_id];
            }
            i4_mv_x += gai1_grid_id_to_x[e_min_id];
            i4_mv_y += gai1_grid_id_to_y[e_min_id];
            ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
            ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;
            i4_grid_mask &= hme_clamp_grid_by_mvrange(ps_search_node, 1, ps_range_prms);
        }
    }

    /* update modified motion vectors and cost at end of subpel */
    ps_search_node->s_mv.i2_mvx = (S16)i4_mv_x;
    ps_search_node->s_mv.i2_mvy = (S16)i4_mv_y;
    ps_search_node->i4_tot_cost = i4_min_cost;
    ps_search_node->i4_sad = i4_min_sad;

    /********************************************************************************/
    /* TODO: Restoring back Sad lambda from Hadamard lambda                         */
    /* Need to pass the had/satd lambda in more cleaner way for subpel cost compute */
    /********************************************************************************/
    //ps_pred_ctxt->lambda >>= 1;

    return (i4_min_cost);
}
#endif

static void hme_subpel_refine_struct_to_search_results_struct_converter(
    subpel_refine_ctxt_t *ps_subpel_refine_ctxt,
    search_results_t *ps_search_results,
    U08 u1_pred_dir,
    ME_QUALITY_PRESETS_T e_quality_preset)
{
    U08 i;

    U08 u1_num_results_per_part = ps_search_results->u1_num_results_per_part;

    for(i = 0; i < ps_subpel_refine_ctxt->i4_num_valid_parts; i++)
    {
        S32 index;
        S32 i4_sad;

        S32 part_id = ps_subpel_refine_ctxt->ai4_part_id[i];

        search_node_t *ps_best_node = ps_search_results->aps_part_results[u1_pred_dir][part_id];

        if(ps_subpel_refine_ctxt->i4_num_valid_parts > 8)
        {
            index = part_id;
        }
        else
        {
            index = i;
        }

        if(!ps_best_node->u1_subpel_done)
        {
            i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                     ps_subpel_refine_ctxt->i2_mv_cost[0][index];
            ps_best_node[0].i4_sdi = 0;
            ASSERT((e_quality_preset == ME_PRISTINE_QUALITY) ? (ps_best_node[0].i4_sdi >= 0) : 1);
            ps_best_node[0].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[0][index];

            if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] == MAX_SIGNED_16BIT_VAL)
            {
                i4_sad = MAX_SIGNED_16BIT_VAL;
            }

            ps_best_node[0].i4_sad = i4_sad;
            ps_best_node[0].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];
            ps_best_node[0].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
            ps_best_node[0].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
            ps_best_node[0].i1_ref_idx = (WORD8)ps_subpel_refine_ctxt->i2_ref_idx[0][index];
            ps_best_node->u1_subpel_done = 1;

            if(2 == u1_num_results_per_part)
            {
                i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[1][index] -
                         ps_subpel_refine_ctxt->i2_mv_cost[1][index];
                ps_best_node[1].i4_sdi = 0;
                ps_best_node[1].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[1][index];

                if(ps_subpel_refine_ctxt->i2_tot_cost[1][index] == MAX_SIGNED_16BIT_VAL)
                {
                    i4_sad = MAX_SIGNED_16BIT_VAL;
                }

                ps_best_node[1].i4_sad = i4_sad;
                ps_best_node[1].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[1][index];
                ps_best_node[1].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[1][index];
                ps_best_node[1].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[1][index];
                ps_best_node[1].i1_ref_idx = (WORD8)ps_subpel_refine_ctxt->i2_ref_idx[1][index];
                ps_best_node[1].u1_subpel_done = 1;
            }
        }
        else if(
            (2 == u1_num_results_per_part) &&
            (ps_subpel_refine_ctxt->i2_tot_cost[0][index] < ps_best_node[1].i4_tot_cost))
        {
            if(ps_subpel_refine_ctxt->i2_tot_cost[1][index] < ps_best_node[0].i4_tot_cost)
            {
                i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                         ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                ps_best_node[0].i4_sdi = 0;
                ps_best_node[0].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[0][index];

                if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] == MAX_SIGNED_16BIT_VAL)
                {
                    i4_sad = MAX_SIGNED_16BIT_VAL;
                }

                ps_best_node[0].i4_sad = i4_sad;
                ps_best_node[0].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                ps_best_node[0].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
                ps_best_node[0].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
                ps_best_node[0].i1_ref_idx = (S08)ps_subpel_refine_ctxt->i2_ref_idx[0][index];

                i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[1][index] -
                         ps_subpel_refine_ctxt->i2_mv_cost[1][index];
                ps_best_node[1].i4_sdi = 0;
                ps_best_node[1].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[1][index];

                if(ps_subpel_refine_ctxt->i2_tot_cost[1][index] == MAX_SIGNED_16BIT_VAL)
                {
                    i4_sad = MAX_SIGNED_16BIT_VAL;
                }

                ps_best_node[1].i4_sad = i4_sad;
                ps_best_node[1].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[1][index];
                ps_best_node[1].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[1][index];
                ps_best_node[1].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[1][index];
                ps_best_node[1].i1_ref_idx = (S08)ps_subpel_refine_ctxt->i2_ref_idx[1][index];
            }
            else if(ps_subpel_refine_ctxt->i2_tot_cost[1][index] > ps_best_node[0].i4_tot_cost)
            {
                if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] >= ps_best_node[0].i4_tot_cost)
                {
                    i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                             ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                    ps_best_node[1].i4_sdi = 0;
                    ps_best_node[1].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[0][index];

                    if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] == MAX_SIGNED_16BIT_VAL)
                    {
                        i4_sad = MAX_SIGNED_16BIT_VAL;
                    }

                    ps_best_node[1].i4_sad = i4_sad;
                    ps_best_node[1].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                    ps_best_node[1].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
                    ps_best_node[1].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
                    ps_best_node[1].i1_ref_idx = (S08)ps_subpel_refine_ctxt->i2_ref_idx[0][index];
                }
                else if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] < ps_best_node[0].i4_tot_cost)
                {
                    memmove(&ps_best_node[1], &ps_best_node[0], sizeof(search_node_t));

                    i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                             ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                    ps_best_node[0].i4_sdi = 0;
                    ps_best_node[0].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[0][index];

                    if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] == MAX_SIGNED_16BIT_VAL)
                    {
                        i4_sad = MAX_SIGNED_16BIT_VAL;
                    }

                    ps_best_node[0].i4_sad = i4_sad;
                    ps_best_node[0].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];
                    ps_best_node[0].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
                    ps_best_node[0].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
                    ps_best_node[0].i1_ref_idx = (S08)ps_subpel_refine_ctxt->i2_ref_idx[0][index];
                }
            }
        }
        else if(
            (1 == u1_num_results_per_part) &&
            (ps_subpel_refine_ctxt->i2_tot_cost[0][index] < ps_best_node[0].i4_tot_cost))
        {
            i4_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                     ps_subpel_refine_ctxt->i2_mv_cost[0][index];
            ps_best_node[0].i4_sdi = 0;
            ps_best_node[0].i4_tot_cost = ps_subpel_refine_ctxt->i2_tot_cost[0][index];

            if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] == MAX_SIGNED_16BIT_VAL)
            {
                i4_sad = MAX_SIGNED_16BIT_VAL;
            }

            ps_best_node[0].i4_sad = i4_sad;
            ps_best_node[0].i4_mv_cost = ps_subpel_refine_ctxt->i2_mv_cost[0][index];
            ps_best_node[0].s_mv.i2_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
            ps_best_node[0].s_mv.i2_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
            ps_best_node[0].i1_ref_idx = (S08)ps_subpel_refine_ctxt->i2_ref_idx[0][index];
        }
    }
}

/**
********************************************************************************
*  @fn     S32 hme_subpel_refine_cu_hs
*
*  @brief  Evaluates the best subpel mvs for active partitions of an MB in L0
*          layer for the high speed preset. Recursive hadamard SATD / SAD
*          and mv cost is used for 2NxN and NxN partitions with active partition
*          update
*
*  @param[in]  ps_prms: subpel prms input to this function
*
*  @param[in]  ps_curr_layer: points to the current layer ctxt
*
*  @param[out] ps_search_results: points to the search resutls that get updated
*              with best results
*
*  @param[in]  search_idx:  ref id of the frame for which results get updated
*
*  @param[in]  ps_wt_inp_prms:  current frame input params
*
*  @return     None
********************************************************************************
*/
void hme_subpel_refine_cu_hs(
    hme_subpel_prms_t *ps_prms,
    layer_ctxt_t *ps_curr_layer,
    search_results_t *ps_search_results,
    S32 search_idx,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    WORD32 blk_8x8_mask,
    me_func_selector_t *ps_func_selector,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list,
    ihevce_me_optimised_function_list_t *ps_me_optimised_function_list)
{
    /* Unique search node list for 2nx2n and nxn partitions */
    search_node_t as_nodes_2nx2n[MAX_RESULTS_PER_PART * 5];
    subpel_dedup_enabler_t as_subpel_dedup_enabler[MAX_NUM_REF];
    search_node_t *ps_search_node;

    S32 i, i4_part_mask, j;
    S32 i4_sad_grid;
    S32 max_subpel_cand;
    WORD32 index;
    S32 num_unique_nodes_2nx2n;
    S32 part_id;
    S32 x_off, y_off;
    S32 i4_inp_off;

    CU_SIZE_T e_cu_size;
    BLK_SIZE_T e_blk_size;

    subpel_refine_ctxt_t *ps_subpel_refine_ctxt = ps_prms->ps_subpel_refine_ctxt;

    S32 i4_use_satd = ps_prms->i4_use_satd;
    S32 i4_num_act_refs = ps_prms->i4_num_act_ref_l0 + ps_prms->i4_num_act_ref_l1;

    ASSERT(ps_search_results->u1_num_results_per_part <= MAX_RESULTS_PER_PART);

    if(!DISABLE_SUBPEL_REFINEMENT_WHEN_SRC_IS_NOISY || !ps_prms->u1_is_cu_noisy)
    {
        e_cu_size = ps_search_results->e_cu_size;
        i4_part_mask = ps_search_results->i4_part_mask;

        ps_prms->i4_inp_type = sizeof(U08);

        num_unique_nodes_2nx2n = 0;

        for(i = 0; i < i4_num_act_refs; i++)
        {
            as_subpel_dedup_enabler[i].u1_ref_idx = MAX_NUM_REF;
        }

        /************************************************************************/
        /*                                                                      */
        /*  Initialize SATD cost for each valid partition id.one time before    */
        /*  doing full pel time. This is because of the following reasons:      */
        /*   1. Full pel cost was done in  SAD while subpel is in SATD mode     */
        /*   2. Partitions like AMP, Nx2N and 2NxN are refined on the fly while */
        /*      doing Diamond search for 2Nx2N and NxN. This partitions are     */
        /*      not explicitly refine in high speed mode                        */
        /*                                                                      */
        /************************************************************************/
        for(i = 0; i < ps_subpel_refine_ctxt->i4_num_valid_parts; i++)
        {
            S32 enable_subpel = 0;
            S32 part_type;

            /* Derive the x and y offsets of this part id */
            part_id = ps_subpel_refine_ctxt->ai4_part_id[i];
            if(ps_subpel_refine_ctxt->i4_num_valid_parts > 8)
            {
                index = part_id;
            }
            else
            {
                index = i;
            }

            part_type = ge_part_id_to_part_type[part_id];
            x_off = gas_part_attr_in_cu[part_id].u1_x_start << e_cu_size;
            y_off = gas_part_attr_in_cu[part_id].u1_y_start << e_cu_size;
            x_off += ps_search_results->u1_x_off;
            y_off += ps_search_results->u1_y_off;
            i4_inp_off = x_off + y_off * ps_prms->i4_inp_stride;
            e_blk_size = ge_part_id_to_blk_size[e_cu_size][part_id];

            x_off += ps_prms->i4_ctb_x_off;
            y_off += ps_prms->i4_ctb_y_off;

            max_subpel_cand = 0;

            /* Choose the minimum number of candidates to be used for Sub pel refinement */
            if(PART_ID_2Nx2N == part_type)
            {
                max_subpel_cand =
                    MIN(ps_prms->u1_max_subpel_candts_2Nx2N,
                        ps_search_results->u1_num_results_per_part);
            }
            else if(PRT_NxN == part_type)
            {
                max_subpel_cand = MIN(
                    ps_prms->u1_max_subpel_candts_NxN, ps_search_results->u1_num_results_per_part);
            }

            /* If incomplete CTB, NxN num candidates should be forced to min 1 */
            if((0 == max_subpel_cand) && (blk_8x8_mask != 15))
            {
                max_subpel_cand = 1;
            }

            if((PART_ID_2Nx2N == part_type) || (PRT_NxN == part_type))
            {
                enable_subpel = 1;
            }

            /* Compute full pel SATD for each result per partition before subpel */
            /* refinement starts.                                                */
            /* Also prepare unique candidate list for 2Nx2N and NxN partitions   */
            for(j = 0; j < ps_search_results->u1_num_results_per_part; j++)
            {
                err_prms_t s_err_prms;
                S32 i4_satd = 0;
                S32 i1_ref_idx;
                U08 *pu1_ref_base;
                S32 i4_ref_stride = ps_curr_layer->i4_rec_stride;
                S32 i4_mv_x, i4_mv_y;

                ps_search_node = ps_search_results->aps_part_results[search_idx][part_id] + j;

                if(ps_subpel_refine_ctxt->i2_mv_x[j][index] == INTRA_MV)
                {
                    ps_search_node->u1_subpel_done = 1;
                    continue;
                }

                i1_ref_idx = ps_subpel_refine_ctxt->i2_ref_idx[j][index];
                ps_prms->pv_inp = (void *)(ps_wt_inp_prms->apu1_wt_inp[i1_ref_idx] + i4_inp_off);
                pu1_ref_base = ps_curr_layer->ppu1_list_rec_fxfy[i1_ref_idx];

                i4_mv_x = ps_subpel_refine_ctxt->i2_mv_x[j][index];
                i4_mv_y = ps_subpel_refine_ctxt->i2_mv_y[j][index];

                if(i4_use_satd)
                {
                    s_err_prms.pu1_inp = (U08 *)ps_prms->pv_inp;
                    s_err_prms.i4_inp_stride = ps_prms->i4_inp_stride;
                    s_err_prms.pu1_ref = pu1_ref_base + x_off + (y_off * i4_ref_stride) + i4_mv_x +
                                         (i4_mv_y * i4_ref_stride);

                    s_err_prms.i4_ref_stride = i4_ref_stride;
                    s_err_prms.i4_part_mask = (ENABLE_2Nx2N);
                    s_err_prms.i4_grid_mask = 1;
                    s_err_prms.pi4_sad_grid = &i4_sad_grid;
                    s_err_prms.i4_blk_wd = gau1_blk_size_to_wd[e_blk_size];
                    s_err_prms.i4_blk_ht = gau1_blk_size_to_ht[e_blk_size];

                    s_err_prms.ps_cmn_utils_optimised_function_list =
                        ps_cmn_utils_optimised_function_list;

                    compute_satd_8bit(&s_err_prms);

                    i4_satd = s_err_prms.pi4_sad_grid[0];

                    ps_subpel_refine_ctxt->i2_tot_cost[j][index] =
                        CLIP_S16(ps_subpel_refine_ctxt->i2_mv_cost[j][index] + i4_satd);
                    ps_subpel_refine_ctxt->ai2_fullpel_satd[j][index] = i4_satd;
                }

                /* Sub-pel candidate filtration */
                if(j)
                {
                    S16 i2_best_sad;
                    S32 i4_best_mvx;
                    S32 i4_best_mvy;

                    search_node_t *ps_node =
                        ps_search_results->aps_part_results[search_idx][part_id];

                    U08 u1_is_subpel_done = ps_node->u1_subpel_done;
                    S16 i2_curr_sad = ps_subpel_refine_ctxt->ai2_fullpel_satd[j][index];
                    S32 i4_curr_mvx = i4_mv_x << 2;
                    S32 i4_curr_mvy = i4_mv_y << 2;

                    if(u1_is_subpel_done)
                    {
                        i2_best_sad = ps_node->i4_sad;

                        if(ps_node->i1_ref_idx == i1_ref_idx)
                        {
                            i4_best_mvx = ps_node->s_mv.i2_mvx;
                            i4_best_mvy = ps_node->s_mv.i2_mvy;
                        }
                        else if(i1_ref_idx == ps_subpel_refine_ctxt->i2_ref_idx[0][index])
                        {
                            i4_best_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
                            i4_best_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
                        }
                        else
                        {
                            i4_best_mvx = INTRA_MV;
                            i4_best_mvy = INTRA_MV;
                        }
                    }
                    else
                    {
                        i2_best_sad = ps_subpel_refine_ctxt->i2_tot_cost[0][index] -
                                      ps_subpel_refine_ctxt->i2_mv_cost[0][index];

                        if(i1_ref_idx == ps_subpel_refine_ctxt->i2_ref_idx[0][index])
                        {
                            i4_best_mvx = ps_subpel_refine_ctxt->i2_mv_x[0][index];
                            i4_best_mvy = ps_subpel_refine_ctxt->i2_mv_y[0][index];
                        }
                        else
                        {
                            i4_best_mvx = INTRA_MV;
                            i4_best_mvy = INTRA_MV;
                        }
                    }

                    i2_best_sad += (i2_best_sad >> ps_prms->u1_subpel_candt_threshold);

                    if(((ABS(i4_curr_mvx - i4_best_mvx) < 2) &&
                        (ABS(i4_curr_mvy - i4_best_mvy) < 2)) ||
                       (i2_curr_sad > i2_best_sad))
                    {
                        enable_subpel = 0;
                    }
                }

                ps_search_node->u1_part_id = part_id;

                /* Convert mvs in part results from FPEL to QPEL units */
                ps_subpel_refine_ctxt->i2_mv_x[j][index] <<= 2;
                ps_subpel_refine_ctxt->i2_mv_y[j][index] <<= 2;

                /* If the candidate number is more than the number of candts
                set initally, do not add those candts for refinement */
                if(j >= max_subpel_cand)
                {
                    enable_subpel = 0;
                }

                if(enable_subpel)
                {
                    if(num_unique_nodes_2nx2n == 0)
                    {
                        S32 i4_index = ps_subpel_refine_ctxt->i2_ref_idx[j][index];

                        as_subpel_dedup_enabler[i4_index].i2_mv_x =
                            ps_subpel_refine_ctxt->i2_mv_x[j][index];
                        as_subpel_dedup_enabler[i4_index].i2_mv_y =
                            ps_subpel_refine_ctxt->i2_mv_y[j][index];
                        as_subpel_dedup_enabler[i4_index].u1_ref_idx =
                            (U08)ps_subpel_refine_ctxt->i2_ref_idx[j][index];
                        memset(
                            as_subpel_dedup_enabler[i4_index].au4_node_map,
                            0,
                            sizeof(U32) * 2 * MAP_X_MAX);
                    }
                    INSERT_NEW_NODE_NOMAP_ALTERNATE(
                        as_nodes_2nx2n, num_unique_nodes_2nx2n, ps_subpel_refine_ctxt, j, i);
                }
            }

            /*********************************************************************************************/
            /* If sad_1 < sad_2, then satd_1 need not be lesser than satd_2. Therefore, after conversion */
            /* to satd, tot_cost_1 may not be lesser than tot_cost_2. So we need to sort the search nodes*/
            /* for each partition again, based on the new costs                                          */
            /*********************************************************************************************/
            /*********************************************************************************************/
            /* Because right now, we store only the two best candidates for each partition, the sort will*/
            /* converge to a simple swap.                                                                */
            /* ASSUMPTION : We store only two best results per partition                                 */
            /*********************************************************************************************/
            if(ps_search_results->u1_num_results_per_part == 2)
            {
                if(ps_subpel_refine_ctxt->i2_tot_cost[0][index] >
                   ps_subpel_refine_ctxt->i2_tot_cost[1][index])
                {
                    SWAP(
                        ps_subpel_refine_ctxt->i2_tot_cost[0][index],
                        ps_subpel_refine_ctxt->i2_tot_cost[1][index]);

                    SWAP(
                        ps_subpel_refine_ctxt->i2_mv_cost[0][index],
                        ps_subpel_refine_ctxt->i2_mv_cost[1][index]);

                    SWAP(
                        ps_subpel_refine_ctxt->i2_mv_x[0][index],
                        ps_subpel_refine_ctxt->i2_mv_x[1][index]);

                    SWAP(
                        ps_subpel_refine_ctxt->i2_mv_y[0][index],
                        ps_subpel_refine_ctxt->i2_mv_y[1][index]);

                    SWAP(
                        ps_subpel_refine_ctxt->i2_ref_idx[0][index],
                        ps_subpel_refine_ctxt->i2_ref_idx[1][index]);

                    SWAP(
                        ps_subpel_refine_ctxt->ai2_fullpel_satd[0][index],
                        ps_subpel_refine_ctxt->ai2_fullpel_satd[1][index]);
                }
            }
        }

        if(blk_8x8_mask == 0xf)
        {
            num_unique_nodes_2nx2n =
                MIN(num_unique_nodes_2nx2n, ps_prms->u1_max_num_subpel_refine_centers);
        }
        {
            x_off = gas_part_attr_in_cu[0].u1_x_start << e_cu_size;
            y_off = gas_part_attr_in_cu[0].u1_y_start << e_cu_size;
            x_off += ps_search_results->u1_x_off;
            y_off += ps_search_results->u1_y_off;
            i4_inp_off = x_off + y_off * ps_prms->i4_inp_stride;
            e_blk_size = ge_part_id_to_blk_size[e_cu_size][0];

            for(j = 0; j < num_unique_nodes_2nx2n; j++)
            {
                S32 pred_lx;
                ps_search_node = &as_nodes_2nx2n[j];

                if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
                {
                    continue;
                }

                {
                    S08 i1_ref_idx = ps_search_node->i1_ref_idx;
                    subpel_dedup_enabler_t *ps_dedup_enabler =
                        &(as_subpel_dedup_enabler[i1_ref_idx]);

                    if(ps_dedup_enabler->u1_ref_idx == MAX_NUM_REF)
                    {
                        as_subpel_dedup_enabler[i1_ref_idx].i2_mv_x = ps_search_node->s_mv.i2_mvx;
                        as_subpel_dedup_enabler[i1_ref_idx].i2_mv_y = ps_search_node->s_mv.i2_mvy;
                        as_subpel_dedup_enabler[i1_ref_idx].u1_ref_idx = i1_ref_idx;
                        memset(
                            as_subpel_dedup_enabler[i1_ref_idx].au4_node_map,
                            0,
                            sizeof(U32) * 2 * MAP_X_MAX);
                    }
                }

                pred_lx = search_idx;
                ps_prms->pv_inp =
                    (void *)(ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off);

                hme_subpel_refine_search_node_high_speed(
                    ps_search_node,
                    ps_prms,
                    ps_curr_layer,
                    e_blk_size,
                    x_off + ps_prms->i4_ctb_x_off,
                    y_off + ps_prms->i4_ctb_y_off,
                    ps_search_results,
                    pred_lx,
                    i4_part_mask,
                    &ps_subpel_refine_ctxt->ai4_part_id[0],
                    search_idx,
                    &(as_subpel_dedup_enabler[ps_search_node->i1_ref_idx]),
                    ps_func_selector,
                    ps_me_optimised_function_list);
            }
        }
    }
    else
    {
        for(i = 0; i < ps_subpel_refine_ctxt->i4_num_valid_parts; i++)
        {
            S32 i4_index;

            S32 i4_part_id = ps_subpel_refine_ctxt->ai4_part_id[i];

            if(ps_subpel_refine_ctxt->i4_num_valid_parts > 8)
            {
                i4_index = i4_part_id;
            }
            else
            {
                i4_index = i;
            }

            for(j = 0; j < ps_search_results->u1_num_results_per_part; j++)
            {
                ps_subpel_refine_ctxt->i2_mv_x[j][i4_index] <<= 2;
                ps_subpel_refine_ctxt->i2_mv_y[j][i4_index] <<= 2;
            }
        }
    }

    hme_subpel_refine_struct_to_search_results_struct_converter(
        ps_subpel_refine_ctxt, ps_search_results, search_idx, ps_prms->e_me_quality_presets);
}
