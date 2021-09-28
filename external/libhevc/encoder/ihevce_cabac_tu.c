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
* @file ihevce_cabac_tu.c
*
* @brief
*  This file contains function definitions for cabac entropy coding of
*  transform units of HEVC syntax
*
* @author
*  ittiam
*
* @List of Functions
*  ihevce_cabac_encode_qp_delta()
*  ihevce_cabac_encode_last_coeff_x_y()
*  ihevce_encode_transform_tree()
*  ihevce_cabac_residue_encode()
*  ihevce_cabac_residue_encode_rdopt()
*  ihevce_cabac_residue_encode_rdoq()
*  ihevce_code_all_sig_coeffs_as_0_explicitly()
*  ihevce_find_new_last_csb()
*  ihevce_copy_backup_ctxt()
*  ihevce_estimate_num_bits_till_next_non_zero_coeff()
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
#include "ihevc_trans_macros.h"
#include "ihevc_trans_tables.h"

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
#include "ihevce_common_utils.h"
#include "ihevce_trace.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/
extern UWORD16 gau2_ihevce_cabac_bin_to_bits[64 * 2];

/**
******************************************************************************
* @brief  LUT for deriving of last significant coeff prefix.
*
* @input   : last_significant_coeff
*
* @output  : last_significant_prefix (does not include the
*
* @remarks Look up tables taken frm HM-8.0-dev
******************************************************************************
*/
const UWORD8 gu1_hevce_last_coeff_prefix[32] = { 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
                                                 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9 };

/**
*****************************************************************************
* @brief  LUT for deriving of last significant coeff suffix
*
* @input   : last significant prefix
*
* @output  : prefix code that needs to be subtracted from last_pos to get
*           suffix as per equation 7-55 in section 7.4.12.
*
*           It returns the following code for last_significant_prefix > 3
*            ((1 << ((last_significant_coeff_x_prefix >> 1) - 1))  *
*            (2 + (last_significant_coeff_x_prefix & 1))
*
*
* @remarks Look up tables taken frm HM-8.0-dev
*****************************************************************************
*/
const UWORD8 gu1_hevce_last_coeff_prefix_code[10] = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24 };

/**
*****************************************************************************
* @brief  returns raster index of 4x4 block for diag up-right/horz/vert scans
*
* @input   : scan type and scan idx
*
* @output  : packed y pos(msb 4bit) and x pos(lsb 2bit)
*
*****************************************************************************
*/
const UWORD8 gu1_hevce_scan4x4[3][16] = {
    /* diag up right */
    { 0, 4, 1, 8, 5, 2, 12, 9, 6, 3, 13, 10, 7, 14, 11, 15 },

    /* horz */
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 },

    /* vert */
    { 0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15 }
};

/**
*****************************************************************************
* @brief  returns context increment for sig coeff based on csbf neigbour
*         flags (bottom and right) and current coeff postion in 4x4 block
*         See section 9.3.3.1.4 for details on this context increment
*
* @input   : neigbour csbf flags(bit0:rightcsbf, bit1:bottom csbf)
*           coeff idx in raster order (0-15)
*
* @output  : context increment for sig coeff flag
*
*****************************************************************************
*/
const UWORD8 gu1_hevce_sigcoeff_ctxtinc[4][16] = {
    /* nbr csbf = 0:  sigCtx = (xP+yP == 0) ? 2 : (xP+yP < 3) ? 1: 0 */
    { 2, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 },

    /* nbr csbf = 1:  sigCtx = (yP == 0) ? 2 : (yP == 1) ? 1: 0      */
    { 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },

    /* nbr csbf = 2:  sigCtx = (xP == 0) ? 2 : (xP == 1) ? 1: 0      */
    { 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0, 2, 1, 0, 0 },

    /* nbr csbf = 3:  sigCtx = 2                                     */
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 }
};

const UWORD8 gu1_hevce_sigcoeff_ctxtinc_00[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/**
*****************************************************************************
* @brief  returns context increment for sig coeff for 4x4 tranform size as
*         per Table 9-39 in section 9.3.3.1.4
*
* @input   : coeff idx in raster order (0-15)
*
* @output  : context increment for sig coeff flag
*
*****************************************************************************
*/
const UWORD8 gu1_hevce_sigcoeff_ctxtinc_tr4[16] = { 0, 1, 4, 5, 2, 3, 4, 5, 6, 6, 8, 8, 7, 7, 8, 0 };

#define DISABLE_ZCSBF 0

#define TEST_CABAC_BITESTIMATE 0

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
/**
******************************************************************************
*
*  @brief Entropy encoding of qp_delta in a tu as per sec 9.3.2 Table 9-32
*
*  @par   Description
*  trunacted unary binarization is done based upto abs_delta of 5 and the rest
*  is coded as 0th order Exponential Golomb code
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]      qp_delta
*  delta qp that needs to be encoded
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_qp_delta(cab_ctxt_t *ps_cabac, WORD32 qp_delta)
{
    WORD32 qp_delta_abs = ABS(qp_delta);
    WORD32 c_max = TU_MAX_QP_DELTA_ABS;
    WORD32 ctxt_inc = IHEVC_CAB_QP_DELTA_ABS;
    WORD32 ctxt_inc_max = CTXT_MAX_QP_DELTA_ABS;
    WORD32 ret = IHEVCE_SUCCESS;

    /* qp_delta_abs is coded as combination of tunary and eg0 code  */
    /* See Table 9-32 and Table 9-37 for details on cu_qp_delta_abs */
    ret |= ihevce_cabac_encode_tunary(
        ps_cabac, MIN(qp_delta_abs, c_max), c_max, ctxt_inc, 0, ctxt_inc_max);
    if(qp_delta_abs >= c_max)
    {
        ret |= ihevce_cabac_encode_egk(ps_cabac, qp_delta_abs - c_max, 0);
    }
    AEV_TRACE("cu_qp_delta_abs", qp_delta_abs, ps_cabac->u4_range);

    /* code the qp delta sign flag */
    if(qp_delta_abs)
    {
        WORD32 sign = (qp_delta < 0) ? 1 : 0;
        ret |= ihevce_cabac_encode_bypass_bin(ps_cabac, sign);
        AEV_TRACE("cu_qp_delta_sign", sign, ps_cabac->u4_range);
    }

    return (ret);
}

/**
******************************************************************************
*
*  @brief Encodes position of the last coded coeff (in scan order) of TU
*
*  @par   Description
*  Entropy encode of last coded coeff of a TU as per section:7.3.13
*
*  @param[inout]   ps_cabac
*  pointer to cabac context (handle)
*
*  @param[in]      last_coeff_x
*  x co-ordinate of the last coded coeff of TU(in scan order)
*
*  @param[in]      last_coeff_y
*  x co-ordinate of the last coded coeff of TU (in scan order
*
*  @param[in]      log2_tr_size
*  transform block size corresponding to this node in quad tree
*
*  @param[in]      is_luma
*  indicates if residual block corresponds to luma or chroma block
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_last_coeff_x_y(
    cab_ctxt_t *ps_cabac,
    WORD32 last_coeff_x,
    WORD32 last_coeff_y,
    WORD32 log2_tr_size,
    WORD32 is_luma)
{
    WORD32 ret = IHEVCE_SUCCESS;

    WORD32 last_coeff_x_prefix;
    WORD32 last_coeff_y_prefix;
    WORD32 suffix, suf_length;
    WORD32 c_max;
    WORD32 ctxt_idx_x, ctxt_idx_y, ctx_shift;

    /* derive the prefix code */
    last_coeff_x_prefix = gu1_hevce_last_coeff_prefix[last_coeff_x];
    last_coeff_y_prefix = gu1_hevce_last_coeff_prefix[last_coeff_y];

    c_max = gu1_hevce_last_coeff_prefix[(1 << log2_tr_size) - 1];

    /* context increment as per section 9.3.3.1.2 */
    if(is_luma)
    {
        WORD32 ctx_offset = (3 * (log2_tr_size - 2)) + ((log2_tr_size - 1) >> 2);

        ctxt_idx_x = IHEVC_CAB_COEFFX_PREFIX + ctx_offset;
        ctxt_idx_y = IHEVC_CAB_COEFFY_PREFIX + ctx_offset;
        ctx_shift = (log2_tr_size + 1) >> 2;
    }
    else
    {
        ctxt_idx_x = IHEVC_CAB_COEFFX_PREFIX + 15;
        ctxt_idx_y = IHEVC_CAB_COEFFY_PREFIX + 15;
        ctx_shift = log2_tr_size - 2;
    }

    /* code the last_coeff_x_prefix as tunary binarized code */
    ret |= ihevce_cabac_encode_tunary(
        ps_cabac, last_coeff_x_prefix, c_max, ctxt_idx_x, ctx_shift, c_max);

    AEV_TRACE("last_coeff_x_prefix", last_coeff_x_prefix, ps_cabac->u4_range);

    /* code the last_coeff_y_prefix as tunary binarized code */
    ret |= ihevce_cabac_encode_tunary(
        ps_cabac, last_coeff_y_prefix, c_max, ctxt_idx_y, ctx_shift, c_max);

    AEV_TRACE("last_coeff_y_prefix", last_coeff_y_prefix, ps_cabac->u4_range);

    if(last_coeff_x_prefix > 3)
    {
        /* code the last_coeff_x_suffix as FLC bypass code */
        suffix = last_coeff_x - gu1_hevce_last_coeff_prefix_code[last_coeff_x_prefix];

        suf_length = ((last_coeff_x_prefix - 2) >> 1);

        ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, suffix, suf_length);

        AEV_TRACE("last_coeff_x_suffix", suffix, ps_cabac->u4_range);
    }

    if(last_coeff_y_prefix > 3)
    {
        /* code the last_coeff_y_suffix as FLC bypass code */
        suffix = last_coeff_y - gu1_hevce_last_coeff_prefix_code[last_coeff_y_prefix];

        suf_length = ((last_coeff_y_prefix - 2) >> 1);

        ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, suffix, suf_length);

        AEV_TRACE("last_coeff_y_suffix", suffix, ps_cabac->u4_range);
    }

    return (ret);
}

/**
******************************************************************************
*
*  @brief Encodes a transform tree as per section 7.3.11
*
*  @par   Description
*  Uses recursion till a leaf node is reached where a transform unit
*  is coded. While recursing split_transform_flag and parent chroma cbf flags
*  are coded before recursing to leaf node
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]      x0_ctb
*  x co-ordinate w.r.t ctb start of current tu node of coding tree
*
*  @param[in]      y0_ctb
*  y co-ordinate w.r.t ctb start of current cu node of coding tree
*
*  @param[in]      log2_tr_size
*  transform block size corresponding to this node in quad tree
*
*  @param[in]      tr_depth
*  current depth of the tree
*
*  @param[in]      tr_depth
*  current depth of the tree
*
*  @param[in]      blk_num
*  current block number in the quad tree (required for chorma 4x4 coding)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_encode_transform_tree(
    entropy_context_t *ps_entropy_ctxt,
    WORD32 x0_ctb,
    WORD32 y0_ctb,
    WORD32 log2_tr_size,
    WORD32 tr_depth,
    WORD32 blk_num,
    cu_enc_loop_out_t *ps_enc_cu)
{
    WORD32 ret = IHEVCE_SUCCESS;
    sps_t *ps_sps = ps_entropy_ctxt->ps_sps;
    WORD32 split_tr_flag;

    WORD32 tu_idx = ps_entropy_ctxt->i4_tu_idx;
    tu_enc_loop_out_t *ps_enc_tu = ps_enc_cu->ps_enc_tu + tu_idx;

    /* TU size in pels */
    WORD32 tu_size = 4 << ps_enc_tu->s_tu.b3_size;

    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    WORD32 max_tr_depth;
    WORD32 is_intra = (ps_enc_cu->b1_pred_mode_flag == PRED_MODE_INTRA);
    WORD32 log2_min_trafo_size, log2_max_trafo_size;
    UWORD32 u4_bits_estimated_prev;

    WORD32 intra_nxn_pu = 0;
    WORD32 ctxt_inc;
    WORD32 cbf_luma = 0;
    WORD32 ai4_cbf_cb[2] = { 0, 0 };
    WORD32 ai4_cbf_cr[2] = { 0, 0 };
    UWORD32 tu_split_bits = 0;
    UWORD8 u1_is_422 = (ps_sps->i1_chroma_format_idc == 2);

    tu_split_bits = ps_cabac->u4_bits_estimated_q12;
    /* intialize min / max transform sizes based on sps */
    log2_min_trafo_size = ps_sps->i1_log2_min_transform_block_size;

    log2_max_trafo_size = log2_min_trafo_size + ps_sps->i1_log2_diff_max_min_transform_block_size;

    /* intialize max transform depth for intra / inter signalled in sps */
    if(is_intra)
    {
        max_tr_depth = ps_sps->i1_max_transform_hierarchy_depth_intra;
        intra_nxn_pu = ps_enc_cu->b3_part_mode == PART_NxN;
    }
    else
    {
        max_tr_depth = ps_sps->i1_max_transform_hierarchy_depth_inter;
    }

    /* Sanity checks */
    ASSERT(tr_depth <= 4);
    ASSERT(log2_min_trafo_size >= 2);
    ASSERT(log2_max_trafo_size <= 5);
    ASSERT((tu_idx >= 0) && (tu_idx < ps_enc_cu->u2_num_tus_in_cu));
    ASSERT((tu_size >= 4) && (tu_size <= (1 << log2_tr_size)));

    /* Encode split transform flag based on following conditions; sec 7.3.11 */
    if((log2_tr_size <= log2_max_trafo_size) && (log2_tr_size > log2_min_trafo_size) &&
       (tr_depth < max_tr_depth) && (!(intra_nxn_pu && (tr_depth == 0))))
    {
        /* encode the split transform flag, context derived as per Table9-37 */
        ctxt_inc = IHEVC_CAB_SPLIT_TFM + (5 - log2_tr_size);

        /* split if actual tu size is smaller than target tu size */
        split_tr_flag = tu_size < (1 << log2_tr_size);
        u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
        ret |= ihevce_cabac_encode_bin(ps_cabac, split_tr_flag, ctxt_inc);

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {  // clang-format off
            /*PIC INFO : populate cu split flag*/
            ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_split_tu_flag +=
                (ps_cabac->u4_bits_estimated_q12 - u4_bits_estimated_prev);
        }  // clang-format on

        AEV_TRACE("split_transform_flag", split_tr_flag, ps_cabac->u4_range);
    }
    else
    {
        WORD32 inter_split;
        /*********************************************************************/
        /*                                                                   */
        /* split tr is implicitly derived as 1 if  (see section 7.4.10)      */
        /*  a. log2_tr_size > log2_max_trafo_size                            */
        /*  b. intra cu has NXN pu                                           */
        /*  c. inter cu is not 2Nx2N && max_transform_hierarchy_depth_inter=0*/
        /*                                                                   */
        /* split tu is implicitly derived as 0 otherwise                     */
        /*********************************************************************/
        inter_split = (!is_intra) && (max_tr_depth == 0) && (tr_depth == 0) &&
                      (ps_enc_cu->b3_part_mode != PART_2Nx2N);

        if((log2_tr_size > log2_max_trafo_size) || (intra_nxn_pu && (tr_depth == 0)) ||
           (inter_split))
        {
            split_tr_flag = 1;
        }
        else
        {
            split_tr_flag = 0;
        }
    }
    /*accumulate only tu tree bits*/
    ps_cabac->u4_true_tu_split_flag_q12 += ps_cabac->u4_bits_estimated_q12 - tu_split_bits;

    /* Encode the cbf flags for chroma before the split as per sec 7.3.11   */
    if(log2_tr_size > 2)
    {
        /* encode the cbf cb, context derived as per Table 9-37 */
        ctxt_inc = IHEVC_CAB_CBCR_IDX + tr_depth;

        /* Note chroma cbf is coded for depth=0 or if parent cbf was coded */
        if((tr_depth == 0) || (ps_entropy_ctxt->apu1_cbf_cb[0][tr_depth - 1]) ||
           (ps_entropy_ctxt->apu1_cbf_cb[1][tr_depth - 1]))
        {
#if CABAC_BIT_EFFICIENT_CHROMA_PARENT_CBF
            /*************************************************************/
            /* Bit-Efficient chroma cbf signalling                       */
            /* if children nodes have 0 cbf parent cbf can be coded as 0 */
            /* peeking through all the child nodes for cb to check if    */
            /* parent can be coded as 0                                  */
            /*************************************************************/
            WORD32 tu_cnt = 0;
            while(1)
            {
                WORD32 trans_size = 1 << (ps_enc_tu[tu_cnt].s_tu.b3_size + 2);
                WORD32 tu_x = (ps_enc_tu[tu_cnt].s_tu.b4_pos_x << 2);
                WORD32 tu_y = (ps_enc_tu[tu_cnt].s_tu.b4_pos_y << 2);

                ASSERT(tu_cnt < ps_enc_cu->u2_num_tus_in_cu);

                if((ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf) || (ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf_subtu1))
                {
                    ai4_cbf_cb[0] = ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf;
                    ai4_cbf_cb[1] = ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf_subtu1;
                    break;
                }

                /* 8x8 parent has only one 4x4 valid chroma block for 420 */
                if(3 == log2_tr_size)
                    break;

                if((tu_x + trans_size == (x0_ctb + (1 << log2_tr_size))) &&
                   (tu_y + trans_size == (y0_ctb + (1 << log2_tr_size))))
                {
                    ai4_cbf_cb[0] = ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf;
                    ai4_cbf_cb[1] = ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf_subtu1;
                    ASSERT(
                        (0 == ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf) &&
                        (0 == ps_enc_tu[tu_cnt].s_tu.b1_cb_cbf_subtu1));
                    break;
                }

                tu_cnt++;
            }
#else
            /* read cbf only when split is 0 (child node) else force cbf=1 */
            ai4_cbf_cb[0] = (split_tr_flag && (log2_tr_size > 3)) ? 1 : ps_enc_tu->s_tu.b1_cb_cbf;
            ai4_cbf_cb[1] =
                (split_tr_flag && (log2_tr_size > 3)) ? 1 : ps_enc_tu->s_tu.b1_cb_cbf_subtu1;

#endif
            if((u1_is_422) && ((!split_tr_flag) || (3 == log2_tr_size)))
            {
                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cb[0], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cb", ai4_cbf_cb[0], ps_cabac->u4_range);

                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cb[1], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cb", ai4_cbf_cb[1], ps_cabac->u4_range);
            }
            else
            {
                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cb[0] || ai4_cbf_cb[1], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cb", ai4_cbf_cb[0] || ai4_cbf_cb[1], ps_cabac->u4_range);
            }
        }
        else
        {
            ai4_cbf_cb[0] = ps_entropy_ctxt->apu1_cbf_cb[0][tr_depth - 1];
            ai4_cbf_cb[1] = ps_entropy_ctxt->apu1_cbf_cb[1][tr_depth - 1];
        }

        if((tr_depth == 0) || (ps_entropy_ctxt->apu1_cbf_cr[0][tr_depth - 1]) ||
           (ps_entropy_ctxt->apu1_cbf_cr[1][tr_depth - 1]))
        {
#if CABAC_BIT_EFFICIENT_CHROMA_PARENT_CBF
            /*************************************************************/
            /* Bit-Efficient chroma cbf signalling                       */
            /* if children nodes have 0 cbf parent cbf can be coded as 0 */
            /* peeking through all the child nodes for cr to check if    */
            /* parent can be coded as 0                                  */
            /*************************************************************/
            WORD32 tu_cnt = 0;
            while(1)
            {
                WORD32 trans_size = 1 << (ps_enc_tu[tu_cnt].s_tu.b3_size + 2);
                WORD32 tu_x = (ps_enc_tu[tu_cnt].s_tu.b4_pos_x << 2);
                WORD32 tu_y = (ps_enc_tu[tu_cnt].s_tu.b4_pos_y << 2);

                ASSERT(tu_cnt < ps_enc_cu->u2_num_tus_in_cu);

                if((ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf) || (ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf_subtu1))
                {
                    ai4_cbf_cr[0] = ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf;
                    ai4_cbf_cr[1] = ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf_subtu1;
                    break;
                }

                /* 8x8 parent has only one 4x4 valid chroma block for 420 */
                if(3 == log2_tr_size)
                    break;

                if((tu_x + trans_size == (x0_ctb + (1 << log2_tr_size))) &&
                   (tu_y + trans_size == (y0_ctb + (1 << log2_tr_size))))
                {
                    ai4_cbf_cr[0] = ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf;
                    ai4_cbf_cr[1] = ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf_subtu1;
                    ASSERT(
                        (0 == ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf) &&
                        (0 == ps_enc_tu[tu_cnt].s_tu.b1_cr_cbf_subtu1));
                    break;
                }

                tu_cnt++;
            }
#else
            /* read cbf only when split is 0 (child node) else force cbf=1 */
            ai4_cbf_cr[0] = (split_tr_flag && (log2_tr_size > 3)) ? 1 : ps_enc_tu->s_tu.b1_cr_cbf;
            ai4_cbf_cr[1] =
                (split_tr_flag && (log2_tr_size > 3)) ? 1 : ps_enc_tu->s_tu.b1_cr_cbf_subtu1;
#endif

            if((u1_is_422) && ((!split_tr_flag) || (3 == log2_tr_size)))
            {
                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cr[0], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cr", ai4_cbf_cr[0], ps_cabac->u4_range);

                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cr[1], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cr", ai4_cbf_cr[1], ps_cabac->u4_range);
            }
            else
            {
                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
                ret |= ihevce_cabac_encode_bin(ps_cabac, ai4_cbf_cr[0] || ai4_cbf_cr[1], ctxt_inc);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate CBF cr bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_chroma_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on

                AEV_TRACE("cbf_cr", ai4_cbf_cr[0] || ai4_cbf_cr[1], ps_cabac->u4_range);
            }
        }
        else
        {
            ai4_cbf_cr[0] = ps_entropy_ctxt->apu1_cbf_cr[0][tr_depth - 1];
            ai4_cbf_cr[1] = ps_entropy_ctxt->apu1_cbf_cr[1][tr_depth - 1];
        }

        ps_entropy_ctxt->apu1_cbf_cb[0][tr_depth] = ai4_cbf_cb[0];
        ps_entropy_ctxt->apu1_cbf_cr[0][tr_depth] = ai4_cbf_cr[0];
        ps_entropy_ctxt->apu1_cbf_cb[1][tr_depth] = ai4_cbf_cb[1];
        ps_entropy_ctxt->apu1_cbf_cr[1][tr_depth] = ai4_cbf_cr[1];
    }
    else
    {
        ai4_cbf_cb[0] = ps_entropy_ctxt->apu1_cbf_cb[0][tr_depth - 1];
        ai4_cbf_cr[0] = ps_entropy_ctxt->apu1_cbf_cr[0][tr_depth - 1];
        ai4_cbf_cb[1] = ps_entropy_ctxt->apu1_cbf_cb[1][tr_depth - 1];
        ai4_cbf_cr[1] = ps_entropy_ctxt->apu1_cbf_cr[1][tr_depth - 1];
    }

    if(split_tr_flag)
    {
        /* recurse into quad child nodes till a leaf node is reached */
        WORD32 x1_ctb = x0_ctb + ((1 << log2_tr_size) >> 1);
        WORD32 y1_ctb = y0_ctb + ((1 << log2_tr_size) >> 1);

        /* node0 of quad tree */
        ret |= ihevce_encode_transform_tree(
            ps_entropy_ctxt,
            x0_ctb,
            y0_ctb,
            log2_tr_size - 1,
            tr_depth + 1,
            0, /* block 0 */
            ps_enc_cu);

        /* node1 of quad tree */
        ret |= ihevce_encode_transform_tree(
            ps_entropy_ctxt,
            x1_ctb,
            y0_ctb,
            log2_tr_size - 1,
            tr_depth + 1,
            1, /* block 1 */
            ps_enc_cu);

        /* node2 of quad tree */
        ret |= ihevce_encode_transform_tree(
            ps_entropy_ctxt,
            x0_ctb,
            y1_ctb,
            log2_tr_size - 1,
            tr_depth + 1,
            2, /* block 2 */
            ps_enc_cu);

        /* node3 of quad tree */
        ret |= ihevce_encode_transform_tree(
            ps_entropy_ctxt,
            x1_ctb,
            y1_ctb,
            log2_tr_size - 1,
            tr_depth + 1,
            3, /* block 3 */
            ps_enc_cu);
    }
    else
    {
        /* leaf node is reached! Encode the TU */
        WORD32 encode_delta_qp;
        void *pv_coeff;
        void *pv_cu_coeff = ps_enc_cu->pv_coeff;

        /* condition to encode qp of cu in first coded tu */
        encode_delta_qp = ps_entropy_ctxt->i1_encode_qp_delta &&
                          (ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS);

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {  // clang-format off
            /*PIC INFO : Tota TUs based on size*/
            if(32 == tu_size)
            {
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[3]++;
            }
            else
            {
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[tu_size >> 3]++;
            }
        }  // clang-format on

        /* sanity checks */
        ASSERT(ps_entropy_ctxt->i1_ctb_num_pcm_blks == 0);
        ASSERT((ps_enc_tu->s_tu.b4_pos_x << 2) == x0_ctb);
        ASSERT((ps_enc_tu->s_tu.b4_pos_y << 2) == y0_ctb);
        ASSERT(tu_size == (1 << log2_tr_size));

        /********************************************************************/
        /* encode luma cbf if any of following conditions are true          */
        /* intra cu | transform depth > 0 | any of chroma cbfs are coded    */
        /*                                                                  */
        /* Note that these conditions mean that cbf_luma need not be        */
        /* signalled and implicitly derived as 1 for inter cu whose tfr size*/
        /* is same as cu size and cbf for cb+cr are zero as no_residue_flag */
        /* at cu level = 1 indicated cbf luma is coded                      */
        /********************************************************************/
        if(is_intra || (tr_depth != 0) || ai4_cbf_cb[0] || ai4_cbf_cr[0] ||
           ((u1_is_422) && (ai4_cbf_cb[1] || ai4_cbf_cr[1])))
        {
            /* encode  cbf luma, context derived as per Table 9-37 */
            cbf_luma = ps_enc_tu->s_tu.b1_y_cbf;

            ctxt_inc = IHEVC_CAB_CBF_LUMA_IDX;
            ctxt_inc += (tr_depth == 0) ? 1 : 0;

            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {
                if(1 == cbf_luma)
                {
                    // clang-format off
                    /*PIC INFO: Populated coded Intra/Inter TUs in CU*/
                    if(1 == is_intra)
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_intra_coded_tu++;
                    else
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_inter_coded_tu++;
                    // clang-format on
                }
                else
                { /*PIC INFO: Populated coded non-coded TUs in CU*/
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_non_coded_tu++;
                }
            }
            u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
            ret |= ihevce_cabac_encode_bin(ps_cabac, cbf_luma, ctxt_inc);

            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {  // clang-format off
                /*PIC INFO : Populate CBF luma bits*/
                ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cbf_luma_bits +=
                    (ps_cabac->u4_bits_estimated_q12 - u4_bits_estimated_prev);
            }  // clang-format on
            AEV_TRACE("cbf_luma", cbf_luma, ps_cabac->u4_range);
        }
        else
        {
            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {
                /*PIC INFO: Populated coded Inter TUs in CU*/
                ps_entropy_ctxt->ps_pic_level_info->i8_total_inter_coded_tu++;
            }

            /* shall be 1 as no_residue_flag was encoded as 1 in inter cu */
            ASSERT(1 == ps_enc_tu->s_tu.b1_y_cbf);
            cbf_luma = ps_enc_tu->s_tu.b1_y_cbf;
        }

        /*******************************************************************/
        /* code qp delta conditionally if following conditions are true    */
        /* any cbf coded (luma/cb/cr) and qp_delta_coded is 0 for this cu  */
        /* see section 7.3.12 Transform unit Syntax                        */
        /*******************************************************************/
        {
            WORD32 cbf_chroma = (ai4_cbf_cb[0] || ai4_cbf_cr[0]) ||
                                (u1_is_422 && (ai4_cbf_cb[1] || ai4_cbf_cr[1]));

            if((cbf_luma || cbf_chroma) && encode_delta_qp)
            {
                WORD32 tu_qp = ps_enc_tu->s_tu.b7_qp;
                WORD32 qp_pred, qp_left, qp_top;
                WORD32 qp_delta = tu_qp - ps_entropy_ctxt->i1_cur_qp;
                WORD32 x_nbr_indx, y_nbr_indx;

                /* Added code for handling the QP neighbour population depending
                   on the diff_cu_qp_delta_depth: Lokesh  */
                /* minus 2 becoz the pos_x and pos_y are given in the order of
                 * 8x8 blocks rather than pixels */
                WORD32 log2_min_cu_qp_delta_size =
                    ps_entropy_ctxt->i1_log2_ctb_size -
                    ps_entropy_ctxt->ps_pps->i1_diff_cu_qp_delta_depth;
                //WORD32 min_cu_qp_delta_size = 1 << log2_min_cu_qp_delta_size;

                //WORD32 curr_pos_x = ps_enc_cu->b3_cu_pos_x << 3;
                //WORD32 curr_pos_y = ps_enc_cu->b3_cu_pos_y << 3;

                WORD32 block_addr_align = 15 << (log2_min_cu_qp_delta_size - 3);

                ps_entropy_ctxt->i4_qg_pos_x = ps_enc_cu->b3_cu_pos_x & block_addr_align;
                ps_entropy_ctxt->i4_qg_pos_y = ps_enc_cu->b3_cu_pos_y & block_addr_align;

                x_nbr_indx = ps_entropy_ctxt->i4_qg_pos_x - 1;
                y_nbr_indx = ps_entropy_ctxt->i4_qg_pos_y - 1;

                if(ps_entropy_ctxt->i4_qg_pos_x > 0)
                {
                    // clang-format off
                    qp_left =
                        ps_entropy_ctxt->ai4_8x8_cu_qp[x_nbr_indx +
                                            (ps_entropy_ctxt->i4_qg_pos_y * 8)];
                    // clang-format on
                }
                if(ps_entropy_ctxt->i4_qg_pos_y > 0)
                {
                    // clang-format off
                    qp_top = ps_entropy_ctxt->ai4_8x8_cu_qp[ps_entropy_ctxt->i4_qg_pos_x +
                                                 y_nbr_indx * 8];
                    // clang-format on
                }
                if(ps_entropy_ctxt->i4_qg_pos_x == 0)
                {
                    /*previous coded Qp*/
                    qp_left = ps_entropy_ctxt->i1_cur_qp;
                }
                if(ps_entropy_ctxt->i4_qg_pos_y == 0)
                {
                    /*previous coded Qp*/
                    qp_top = ps_entropy_ctxt->i1_cur_qp;
                }

                qp_pred = (qp_left + qp_top + 1) >> 1;
                // clang-format off
                /* start of every frame encode qp delta wrt slice qp when entrop
                 * sync is enabled */
                if(ps_entropy_ctxt->i4_ctb_x == 0 &&
                    ps_entropy_ctxt->i4_qg_pos_x == 0 &&
                    ps_entropy_ctxt->i4_qg_pos_y == 0 &&
                    ps_entropy_ctxt->s_cabac_ctxt.i1_entropy_coding_sync_enabled_flag)
                // clang-format on
                {
                    qp_pred = ps_entropy_ctxt->ps_slice_hdr->i1_slice_qp_delta +
                              ps_entropy_ctxt->ps_pps->i1_pic_init_qp;
                }
                qp_delta = tu_qp - qp_pred;

                /*PIC INFO : Populate QP delta bits*/
                u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;

                /* code the qp delta */
                ret |= ihevce_cabac_encode_qp_delta(ps_cabac, qp_delta);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {
                    // clang-format off
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_qp_delta_bits +=
                        (ps_cabac->u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                    // clang-format on
                }

                ps_entropy_ctxt->i1_cur_qp = tu_qp;
                //ps_entropy_ctxt->i1_cur_qp = Qp_pred;
                ps_entropy_ctxt->i1_encode_qp_delta = 0;
                //ps_entropy_ctxt->i4_is_cu_cbf_zero = 0;
            }

            if(cbf_luma || cbf_chroma)
            {
                ps_entropy_ctxt->i4_is_cu_cbf_zero = 0;
            }

            /* code the residue of for luma and chroma tu based on cbf */
            if((cbf_luma) && (1 == ps_entropy_ctxt->i4_enable_res_encode))
            {
                u4_bits_estimated_prev = ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12;
                /* code the luma residue */
                pv_coeff = (void *)((UWORD8 *)pv_cu_coeff + ps_enc_tu->i4_luma_coeff_offset);

                ret |= ihevce_cabac_residue_encode(ps_entropy_ctxt, pv_coeff, log2_tr_size, 1);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {  // clang-format off
                    /*PIC INFO : Populate Residue Luma Bits*/
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_res_luma_bits +=
                        (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                            u4_bits_estimated_prev);
                }  // clang-format on
            }

            /* code chroma residue based on tranform size                  */
            /* For Inta 4x4 pu chroma is coded after all 4 luma blks coded */
            /* Note: chroma not encoded in rdopt mode                      */
            if(((log2_tr_size > 2) || (3 == blk_num)) /* &&
                (CABAC_MODE_ENCODE_BITS == ps_cabac->e_cabac_op_mode) */
            )
            {
                WORD32 log2_chroma_tr_size;
                WORD32 i4_subtu_idx;
                void *pv_coeff_cb, *pv_coeff_cr;

                WORD32 i4_num_subtus = u1_is_422 + 1;

                if(1 == ps_entropy_ctxt->i4_enable_res_encode)
                {
                    for(i4_subtu_idx = 0; i4_subtu_idx < i4_num_subtus; i4_subtu_idx++)
                    {
                        if(ai4_cbf_cb[i4_subtu_idx])
                        {
                            /* initailize chroma transform size and coeff based
                             * on luma size */
                            if(2 == log2_tr_size)
                            {
                                /*********************************************************/
                                /* For Intra 4x4, chroma transform size is 4 and chroma  */
                                /* coeff offset is present  in the first Luma block      */
                                /*********************************************************/
                                log2_chroma_tr_size = 2;

                                /* -3 is for going to first luma tu of the 4 TUs in min CU */
                                pv_coeff_cb =
                                    (void
                                         *)((UWORD8 *)pv_cu_coeff + ps_enc_tu[-3].ai4_cb_coeff_offset[i4_subtu_idx]);
                            }
                            else
                            {
                                log2_chroma_tr_size = (log2_tr_size - 1);

                                pv_coeff_cb =
                                    (void
                                         *)((UWORD8 *)pv_cu_coeff + ps_enc_tu->ai4_cb_coeff_offset[i4_subtu_idx]);
                            }
                            // clang-format off
                            u4_bits_estimated_prev =
                                ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12;
                            // clang-format on
                            /* code the cb residue */
                            ret |= ihevce_cabac_residue_encode(
                                ps_entropy_ctxt, pv_coeff_cb, log2_chroma_tr_size, 0);

                            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                            {  // clang-format off
                                /*PIC INFO : Populate Residue Chroma cr Bits*/
                                ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_res_chroma_bits +=
                                    (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                                        u4_bits_estimated_prev);
                            }  // clang-format on
                        }
                    }
                }

                if(1 == ps_entropy_ctxt->i4_enable_res_encode)
                {
                    for(i4_subtu_idx = 0; i4_subtu_idx < i4_num_subtus; i4_subtu_idx++)
                    {
                        if(ai4_cbf_cr[i4_subtu_idx])
                        {
                            /* initailize chroma transform size and coeff based on luma size */
                            if(2 == log2_tr_size)
                            {
                                /*********************************************************/
                                /* For Intra 4x4, chroma transform size is 4 and chroma  */
                                /* coeff offset is present  in the first Luma block      */
                                /*********************************************************/
                                log2_chroma_tr_size = 2;

                                pv_coeff_cr =
                                    (void
                                         *)((UWORD8 *)pv_cu_coeff + ps_enc_tu[-3].ai4_cr_coeff_offset[i4_subtu_idx]);
                            }
                            else
                            {
                                log2_chroma_tr_size = (log2_tr_size - 1);

                                pv_coeff_cr =
                                    (void
                                         *)((UWORD8 *)pv_cu_coeff + ps_enc_tu->ai4_cr_coeff_offset[i4_subtu_idx]);
                            }
                            // clang-format off
                            u4_bits_estimated_prev =
                                ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12;
                            // clang-format on
                            /* code the cb residue */
                            ret |= ihevce_cabac_residue_encode(
                                ps_entropy_ctxt, pv_coeff_cr, log2_chroma_tr_size, 0);
                            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                            {  // clang-format off
                                /*PIC INFO : Populate Residue Chroma cr Bits*/
                                ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_res_chroma_bits +=
                                    (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                                        u4_bits_estimated_prev);
                            }  // clang-format on
                        }
                    }
                }
            }
        }

        /* update tu_idx after encoding current tu */
        ps_entropy_ctxt->i4_tu_idx++;
    }

    return ret;
}

/**
******************************************************************************
*
*  @brief Encodes a transform residual block as per section 7.3.13
*
*  @par   Description
*   The residual block is read from a compressed coeff buffer populated during
*   the scanning of the quantized coeffs. The contents of the buffer are
*   breifly explained in param description of pv_coeff
*
*  @remarks Does not support sign data hiding and transform skip flag currently
*
*  @remarks Need to resolve the differences between JVT-J1003_d7 spec and
*           HM.8.0-dev for related abs_greater_than_1 context initialization
*           and rice_max paramtere used for coeff abs level remaining
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]      pv_coeff
*  Compressed residue buffer containing following information:
*
*  HEADER(4 bytes) : last_coeff_x, last_coeff_y, scantype, last_subblock_num
*
*  For each 4x4 subblock starting from last_subblock_num (in scan order)
*     Read 2 bytes  : MSB 12bits (0xBAD marker), bit0 cur_csbf, bit1-2 nbr csbf
*
*    `If cur_csbf
*      Read 2 bytes : sig_coeff_map (16bits in scan_order 1:coded, 0:not coded)
*      Read 2 bytes : abs_gt1_flags (max of 8 only)
*      Read 2 bytes : coeff_sign_flags
*
*      Based on abs_gt1_flags and sig_coeff_map read remaining abs levels
*      Read 2 bytes : remaining_abs_coeffs_minus1 (this is in a loop)
*
*  @param[in]      log2_tr_size
*  transform size of the current TU
*
*  @param[in]      is_luma
*  boolean indicating if the texture type is luma / chroma
*
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_residue_encode(
    entropy_context_t *ps_entropy_ctxt, void *pv_coeff, WORD32 log2_tr_size, WORD32 is_luma)
{
    WORD32 ret = IHEVCE_SUCCESS;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;
    WORD32 i4_sign_data_hiding_flag, cu_tq_bypass_flag;

    UWORD8 *pu1_coeff_buf_hdr = (UWORD8 *)pv_coeff;
    UWORD16 *pu2_sig_coeff_buf = (UWORD16 *)pv_coeff;

    /* last sig coeff indices in scan order */
    WORD32 last_sig_coeff_x = pu1_coeff_buf_hdr[0];
    WORD32 last_sig_coeff_y = pu1_coeff_buf_hdr[1];

    /* read the scan type : upright diag / horz / vert */
    WORD32 scan_type = pu1_coeff_buf_hdr[2];

    /************************************************************************/
    /* position of the last coded sub block. This sub block contains coeff  */
    /* corresponding to last_sig_coeff_x, last_sig_coeff_y. Althoug this can*/
    /* be derived here it better to be populated by scanning module         */
    /************************************************************************/
    WORD32 last_csb = pu1_coeff_buf_hdr[3];

    WORD32 cur_csbf = 0, nbr_csbf;
    WORD32 sig_coeff_base_ctxt; /* cabac context for sig coeff flag    */
    WORD32 abs_gt1_base_ctxt; /* cabac context for abslevel > 1 flag */

    WORD32 gt1_ctxt = 1; /* required for abs_gt1_ctxt modelling */

    WORD32 i;

    /* sanity checks */
    /* transform skip not supported */
    ASSERT(0 == ps_entropy_ctxt->ps_pps->i1_transform_skip_enabled_flag);

    cu_tq_bypass_flag = ps_entropy_ctxt->ps_pps->i1_transform_skip_enabled_flag;

    i4_sign_data_hiding_flag = ps_entropy_ctxt->ps_pps->i1_sign_data_hiding_flag;

    if(SCAN_VERT == scan_type)
    {
        /* last coeff x and y are swapped for vertical scan */
        SWAP(last_sig_coeff_x, last_sig_coeff_y);
    }

    /* Encode the last_sig_coeff_x and last_sig_coeff_y */
    ret |= ihevce_cabac_encode_last_coeff_x_y(
        ps_cabac, last_sig_coeff_x, last_sig_coeff_y, log2_tr_size, is_luma);

    /*************************************************************************/
    /* derive base context index for sig coeff as per section 9.3.3.1.4      */
    /* TODO; convert to look up based on luma/chroma, scan type and tfr size */
    /*************************************************************************/
    if(is_luma)
    {
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG;
        abs_gt1_base_ctxt = IHEVC_CAB_COEFABS_GRTR1_FLAG;

        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            sig_coeff_base_ctxt += (scan_type == SCAN_DIAG_UPRIGHT) ? 9 : 15;
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

    /* go to csbf flags */
    pu2_sig_coeff_buf = (UWORD16 *)(pu1_coeff_buf_hdr + COEFF_BUF_HEADER_LEN);

    /************************************************************************/
    /* encode the csbf, sig_coeff_map, abs_grt1_flags, abs_grt2_flag, sign  */
    /* and abs_coeff_remaining for each 4x4 starting from last scan to first*/
    /************************************************************************/
    for(i = last_csb; i >= 0; i--)
    {
        UWORD16 u2_marker_csbf;
        WORD32 ctxt_idx;

        u2_marker_csbf = *pu2_sig_coeff_buf;
        pu2_sig_coeff_buf++;

        /* sanity checks for marker present in every csbf flag */
        ASSERT((u2_marker_csbf >> 4) == 0xBAD);

        /* extract the current and neigbour csbf flags */
        cur_csbf = u2_marker_csbf & 0x1;
        nbr_csbf = (u2_marker_csbf >> 1) & 0x3;

        /*********************************************************************/
        /* code the csbf flags; last and first csb not sent as it is derived */
        /*********************************************************************/
        if((i < last_csb) && (i > 0))
        {
            ctxt_idx = IHEVC_CAB_CODED_SUBLK_IDX;

            /* ctxt based on right / bottom avail csbf, section 9.3.3.1.3 */
            ctxt_idx += nbr_csbf ? 1 : 0;
            ctxt_idx += is_luma ? 0 : 2;

            ret |= ihevce_cabac_encode_bin(ps_cabac, cur_csbf, ctxt_idx);
            AEV_TRACE("coded_sub_block_flag", cur_csbf, ps_cabac->u4_range);
        }
        else
        {
            /* sanity check, this csb contains the last_sig_coeff */
            if(i == last_csb)
            {
                ASSERT(cur_csbf == 1);
            }
        }

        if(cur_csbf)
        {
            /*****************************************************************/
            /* encode the sig coeff map as per section 7.3.13                */
            /* significant_coeff_flags: msb=coeff15-lsb=coeff0 in scan order */
            /*****************************************************************/

            /* Added for Sign bit data hiding*/
            WORD32 first_scan_pos = 16;
            WORD32 last_scan_pos = -1;
            WORD32 sign_hidden = 0;

            UWORD16 u2_gt0_flags = *pu2_sig_coeff_buf;
            WORD32 gt1_flags = *(pu2_sig_coeff_buf + 1);
            WORD32 sign_flags = *(pu2_sig_coeff_buf + 2);

            WORD32 sig_coeff_map = u2_gt0_flags;

            WORD32 gt1_bins = 0; /* bins for coeffs with abslevel > 1 */

            WORD32 sign_bins = 0; /* bins for sign flags of coded coeffs  */
            WORD32 num_coded = 0; /* total coeffs coded in 4x4            */

            WORD32 infer_coeff; /* infer when 0,0 is the only coded coeff */
            WORD32 bit; /* temp boolean */

            /* total count of coeffs to be coded as abs level remaining */
            WORD32 num_coeffs_remaining = 0;

            /* count of coeffs to be coded as  abslevel-1 */
            WORD32 num_coeffs_base1 = 0;
            WORD32 scan_pos;
            WORD32 first_gt1_coeff = 0;

            if((i != 0) || (0 == last_csb))
            {
                /* sanity check, atleast one coeff is coded as csbf is set */
                ASSERT(sig_coeff_map != 0);
            }

            pu2_sig_coeff_buf += 3;

            scan_pos = 15;
            if(i == last_csb)
            {
                /*************************************************************/
                /* clear last_scan_pos for last block in scan order as this  */
                /* is communicated  throught last_coeff_x and last_coeff_y   */
                /*************************************************************/
                WORD32 next_sig = CLZ(sig_coeff_map) + 1;

                scan_pos = WORD_SIZE - next_sig;

                /* prepare the bins for gt1 flags */
                EXTRACT_BIT(bit, gt1_flags, scan_pos);

                /* insert gt1 bin in lsb */
                gt1_bins |= bit;

                /* prepare the bins for sign flags */
                EXTRACT_BIT(bit, sign_flags, scan_pos);

                /* insert sign bin in lsb */
                sign_bins |= bit;

                sig_coeff_map = CLEAR_BIT(sig_coeff_map, scan_pos);

                if(-1 == last_scan_pos)
                    last_scan_pos = scan_pos;

                scan_pos--;
                num_coded++;
            }

            /* infer 0,0 coeff for all 4x4 blocks except fitst and last */
            infer_coeff = (i < last_csb) && (i > 0);

            /* encode the required sigcoeff flags (abslevel > 0)   */
            while(scan_pos >= 0)
            {
                WORD32 y_pos_x_pos;
                WORD32 sig_ctxinc = 0; /* 0 is default inc for DC coeff */

                WORD32 sig_coeff;

                EXTRACT_BIT(sig_coeff, sig_coeff_map, scan_pos);

                /* derive the x,y pos */
                y_pos_x_pos = gu1_hevce_scan4x4[scan_type][scan_pos];

                /* derive the context inc as per section 9.3.3.1.4 */
                if(2 == log2_tr_size)
                {
                    /* 4x4 transform size increment uses lookup */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc_tr4[y_pos_x_pos];
                }
                else if(scan_pos || i)
                {
                    /* ctxt for AC coeff depends on curpos and neigbour csbf */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc[nbr_csbf][y_pos_x_pos];

                    /* based on luma subblock pos */
                    sig_ctxinc += (i && is_luma) ? 3 : 0;
                }
                else
                {
                    /* DC coeff has fixed context for luma and chroma */
                    sig_coeff_base_ctxt = is_luma ? IHEVC_CAB_COEFF_FLAG
                                                  : IHEVC_CAB_COEFF_FLAG + 27;
                }

                /*************************************************************/
                /* encode sig coeff only if required                         */
                /* decoder infers 0,0 coeff when all the other coeffs are 0  */
                /*************************************************************/
                if(scan_pos || (!infer_coeff))
                {
                    ctxt_idx = sig_ctxinc + sig_coeff_base_ctxt;
                    ret |= ihevce_cabac_encode_bin(ps_cabac, sig_coeff, ctxt_idx);
                    AEV_TRACE("significant_coeff_flag", sig_coeff, ps_cabac->u4_range);
                }

                if(sig_coeff)
                {
                    /* prepare the bins for gt1 flags */
                    EXTRACT_BIT(bit, gt1_flags, scan_pos);

                    /* shift and insert gt1 bin in lsb */
                    gt1_bins <<= 1;
                    gt1_bins |= bit;

                    /* prepare the bins for sign flags */
                    EXTRACT_BIT(bit, sign_flags, scan_pos);

                    /* shift and insert sign bin in lsb */
                    sign_bins <<= 1;
                    sign_bins |= bit;

                    num_coded++;

                    /* 0,0 coeff can no more be inferred :( */
                    infer_coeff = 0;

                    if(-1 == last_scan_pos)
                        last_scan_pos = scan_pos;

                    first_scan_pos = scan_pos;
                }

                scan_pos--;
            }

            /* Added for sign bit hiding*/
            sign_hidden = ((last_scan_pos - first_scan_pos) > 3 && !cu_tq_bypass_flag);

            /****************************************************************/
            /* encode the abs level greater than 1 bins; Section 7.3.13     */
            /* These have already been prepared during sig_coeff_map encode */
            /* Context modelling done as per section 9.3.3.1.5              */
            /****************************************************************/
            {
                WORD32 j;

                /* context set based on luma subblock pos */
                WORD32 ctxt_set = (i && is_luma) ? 2 : 0;

                /* count of coeffs with abslevel > 1; max of 8 to be coded */
                WORD32 num_gt1_bins = MIN(8, num_coded);

                if(num_coded > 8)
                {
                    /* pull back the bins to required number */
                    gt1_bins >>= (num_coded - 8);

                    num_coeffs_remaining += (num_coded - 8);
                    num_coeffs_base1 = (num_coded - 8);
                }

                /* See section 9.3.3.1.5           */
                ctxt_set += (0 == gt1_ctxt) ? 1 : 0;

                gt1_ctxt = 1;

                for(j = num_gt1_bins - 1; j >= 0; j--)
                {
                    /* Encodet the abs level gt1 bins */
                    ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                    EXTRACT_BIT(bit, gt1_bins, j);

                    ret |= ihevce_cabac_encode_bin(ps_cabac, bit, ctxt_idx);

                    AEV_TRACE("coeff_abs_level_greater1_flag", bit, ps_cabac->u4_range);

                    if(bit)
                    {
                        gt1_ctxt = 0;
                        num_coeffs_remaining++;
                    }
                    else if(gt1_ctxt && (gt1_ctxt < 3))
                    {
                        gt1_ctxt++;
                    }
                }

                /*************************************************************/
                /* encode abs level greater than 2 bin; Section 7.3.13       */
                /*************************************************************/
                if(gt1_bins)
                {
                    WORD32 gt2_bin;

                    first_gt1_coeff = pu2_sig_coeff_buf[0] + 1;
                    gt2_bin = (first_gt1_coeff > 2);

                    /* atleast one level > 2 */
                    ctxt_idx = IHEVC_CAB_COEFABS_GRTR2_FLAG;

                    ctxt_idx += (is_luma) ? ctxt_set : (ctxt_set + 4);

                    ret |= ihevce_cabac_encode_bin(ps_cabac, gt2_bin, ctxt_idx);

                    if(!gt2_bin)
                    {
                        /* sanity check */
                        ASSERT(first_gt1_coeff == 2);

                        /* no need to send this coeff as bypass bins */
                        pu2_sig_coeff_buf++;
                        num_coeffs_remaining--;
                    }

                    AEV_TRACE("coeff_abs_level_greater2_flag", gt2_bin, ps_cabac->u4_range);
                }
            }

            /*************************************************************/
            /* encode the coeff signs and abs remaing levels             */
            /*************************************************************/
            if(num_coded)
            {
                WORD32 base_level;
                WORD32 rice_param = 0;
                WORD32 j;

                /*************************************************************/
                /* encode the coeff signs populated in sign_bins             */
                /*************************************************************/

                if(sign_hidden && i4_sign_data_hiding_flag)
                {
                    sign_bins >>= 1;
                    num_coded--;
                }

                if(num_coded > 0)
                {
                    ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, sign_bins, num_coded);
                }

                AEV_TRACE("sign_flags", sign_bins, ps_cabac->u4_range);

                /*************************************************************/
                /* encode the coeff_abs_level_remaining as TR / EGK bins     */
                /* See section 9.3.2.7 for details                           */
                /*************************************************************/

                /* first remaining coeff baselevel */
                if(first_gt1_coeff > 2)
                {
                    base_level = 3;
                }
                else if(num_coeffs_remaining > num_coeffs_base1)
                {
                    /* atleast one coeff in first 8 is gt > 1 */
                    base_level = 2;
                }
                else
                {
                    /* all coeffs have base of 1 */
                    base_level = 1;
                }

                for(j = 0; j < num_coeffs_remaining; j++)
                {
                    WORD32 abs_coeff = pu2_sig_coeff_buf[0] + 1;
                    WORD32 abs_coeff_rem;
                    WORD32 rice_max = (4 << rice_param);

                    pu2_sig_coeff_buf++;

                    /* sanity check */
                    ASSERT(abs_coeff >= base_level);

                    abs_coeff_rem = (abs_coeff - base_level);

                    /* TODO://HM-8.0-dev uses (3 << rice_param) as rice_max */
                    /* TODO://HM-8.0-dev does either TR or EGK but not both */
                    if(abs_coeff_rem >= rice_max)
                    {
                        UWORD32 u4_suffix = (abs_coeff_rem - rice_max);

                        /* coeff exceeds max rice limit                    */
                        /* encode the TR prefix as tunary code             */
                        /* prefix = 1111 as (rice_max >> rice_praram) = 4  */
                        ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, 0xF, 4);

                        /* encode the exponential golomb code suffix */
                        ret |= ihevce_cabac_encode_egk(ps_cabac, u4_suffix, (rice_param + 1));
                    }
                    else
                    {
                        /* code coeff as truncated rice code  */
                        ret |= ihevce_cabac_encode_trunc_rice(
                            ps_cabac, abs_coeff_rem, rice_param, rice_max);
                    }

                    AEV_TRACE("coeff_abs_level_remaining", abs_coeff_rem, ps_cabac->u4_range);

                    /* update the rice param based on coeff level */
                    if((abs_coeff > (3 << rice_param)) && (rice_param < 4))
                    {
                        rice_param++;
                    }

                    /* change base level to 1 if more than 8 coded coeffs */
                    if((j + 1) < (num_coeffs_remaining - num_coeffs_base1))
                    {
                        base_level = 2;
                    }
                    else
                    {
                        base_level = 1;
                    }
                }
            }
        }
    }
    /*tap texture bits*/
    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_COMPUTE_BITS)
    {  // clang-format off
        ps_cabac->u4_texture_bits_estimated_q12 +=
            (ps_cabac->u4_bits_estimated_q12 -
                ps_cabac->u4_header_bits_estimated_q12);  //(ps_cabac->u4_bits_estimated_q12 - temp_tex_bits_q12);
    }  // clang-format on

    return (ret);
}

/**
******************************************************************************
*
*  @brief Get the bits estimate for a transform residual block as per section
*   7.3.13
*
*  @par   Description
*   The residual block is read from a compressed coeff buffer populated during
*   the scanning of the quantized coeffs. The contents of the buffer are
*   breifly explained in param description of pv_coeff
*
*  @remarks Does not support sign data hiding and transform skip flag currently
*
*  @remarks Need to resolve the differences between JVT-J1003_d7 spec and
*           HM.8.0-dev for related abs_greater_than_1 context initialization
*           and rice_max paramtere used for coeff abs level remaining
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]      pv_coeff
*  Compressed residue buffer containing following information:
*
*  HEADER(4 bytes) : last_coeff_x, last_coeff_y, scantype, last_subblock_num
*
*  For each 4x4 subblock starting from last_subblock_num (in scan order)
*     Read 2 bytes  : MSB 12bits (0xBAD marker), bit0 cur_csbf, bit1-2 nbr csbf
*
*    `If cur_csbf
*      Read 2 bytes : sig_coeff_map (16bits in scan_order 1:coded, 0:not coded)
*      Read 2 bytes : abs_gt1_flags (max of 8 only)
*      Read 2 bytes : coeff_sign_flags
*
*      Based on abs_gt1_flags and sig_coeff_map read remaining abs levels
*      Read 2 bytes : remaining_abs_coeffs_minus1 (this is in a loop)
*
*  @param[in]      log2_tr_size
*  transform size of the current TU
*
*  @param[in]      is_luma
*  boolean indicating if the texture type is luma / chroma
*
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_residue_encode_rdopt(
    entropy_context_t *ps_entropy_ctxt,
    void *pv_coeff,
    WORD32 log2_tr_size,
    WORD32 is_luma,
    WORD32 perform_sbh)
{
    WORD32 ret = IHEVCE_SUCCESS;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;
    UWORD32 temp_tex_bits_q12;
    WORD32 i4_sign_data_hiding_flag, cu_tq_bypass_flag;

    UWORD8 *pu1_coeff_buf_hdr = (UWORD8 *)pv_coeff;
    UWORD16 *pu2_sig_coeff_buf = (UWORD16 *)pv_coeff;

    /* last sig coeff indices in scan order */
    WORD32 last_sig_coeff_x = pu1_coeff_buf_hdr[0];
    WORD32 last_sig_coeff_y = pu1_coeff_buf_hdr[1];

    /* read the scan type : upright diag / horz / vert */
    WORD32 scan_type = pu1_coeff_buf_hdr[2];

    /************************************************************************/
    /* position of the last coded sub block. This sub block contains coeff  */
    /* corresponding to last_sig_coeff_x, last_sig_coeff_y. Althoug this can*/
    /* be derived here it better to be populated by scanning module         */
    /************************************************************************/
    WORD32 last_csb = pu1_coeff_buf_hdr[3];

    WORD32 cur_csbf = 0, nbr_csbf;
    WORD32 sig_coeff_base_ctxt; /* cabac context for sig coeff flag    */
    WORD32 abs_gt1_base_ctxt; /* cabac context for abslevel > 1 flag */

    WORD32 gt1_ctxt = 1; /* required for abs_gt1_ctxt modelling */

    WORD32 i;

    UWORD8 *pu1_ctxt_model = &ps_cabac->au1_ctxt_models[0];

    /* sanity checks */
    /* transform skip not supported */
    ASSERT(0 == ps_entropy_ctxt->ps_pps->i1_transform_skip_enabled_flag);

    cu_tq_bypass_flag = ps_entropy_ctxt->ps_pps->i1_transform_skip_enabled_flag;

    i4_sign_data_hiding_flag = ps_entropy_ctxt->ps_pps->i1_sign_data_hiding_flag;

    {
        temp_tex_bits_q12 = ps_cabac->u4_bits_estimated_q12;
    }

    if(SCAN_VERT == scan_type)
    {
        /* last coeff x and y are swapped for vertical scan */
        SWAP(last_sig_coeff_x, last_sig_coeff_y);
    }

    /* Encode the last_sig_coeff_x and last_sig_coeff_y */
    ret |= ihevce_cabac_encode_last_coeff_x_y(
        ps_cabac, last_sig_coeff_x, last_sig_coeff_y, log2_tr_size, is_luma);

    /*************************************************************************/
    /* derive base context index for sig coeff as per section 9.3.3.1.4      */
    /* TODO; convert to look up based on luma/chroma, scan type and tfr size */
    /*************************************************************************/
    if(is_luma)
    {
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG;
        abs_gt1_base_ctxt = IHEVC_CAB_COEFABS_GRTR1_FLAG;

        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            sig_coeff_base_ctxt += (scan_type == SCAN_DIAG_UPRIGHT) ? 9 : 15;
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

    /* go to csbf flags */
    pu2_sig_coeff_buf = (UWORD16 *)(pu1_coeff_buf_hdr + COEFF_BUF_HEADER_LEN);

    /************************************************************************/
    /* encode the csbf, sig_coeff_map, abs_grt1_flags, abs_grt2_flag, sign  */
    /* and abs_coeff_remaining for each 4x4 starting from last scan to first*/
    /************************************************************************/
    for(i = last_csb; i >= 0; i--)
    {
        UWORD16 u2_marker_csbf;
        WORD32 ctxt_idx;

        u2_marker_csbf = *pu2_sig_coeff_buf;
        pu2_sig_coeff_buf++;

        /* sanity checks for marker present in every csbf flag */
        ASSERT((u2_marker_csbf >> 4) == 0xBAD);

        /* extract the current and neigbour csbf flags */
        cur_csbf = u2_marker_csbf & 0x1;
        nbr_csbf = (u2_marker_csbf >> 1) & 0x3;

        /*********************************************************************/
        /* code the csbf flags; last and first csb not sent as it is derived */
        /*********************************************************************/
        if((i < last_csb) && (i > 0))
        {
            ctxt_idx = IHEVC_CAB_CODED_SUBLK_IDX;

            /* ctxt based on right / bottom avail csbf, section 9.3.3.1.3 */
            ctxt_idx += nbr_csbf ? 1 : 0;
            ctxt_idx += is_luma ? 0 : 2;

            {
                WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                /* increment bits generated based on state and bin encoded */
                ps_cabac->u4_bits_estimated_q12 +=
                    gau2_ihevce_cabac_bin_to_bits[state_mps ^ cur_csbf];

                /* update the context model from state transition LUT */
                pu1_ctxt_model[ctxt_idx] = gau1_ihevc_next_state[(state_mps << 1) | cur_csbf];
            }
        }
        else
        {
            /* sanity check, this csb contains the last_sig_coeff */
            if(i == last_csb)
            {
                ASSERT(cur_csbf == 1);
            }
        }

        if(cur_csbf)
        {
            /*****************************************************************/
            /* encode the sig coeff map as per section 7.3.13                */
            /* significant_coeff_flags: msb=coeff15-lsb=coeff0 in scan order */
            /*****************************************************************/

            /* Added for Sign bit data hiding*/
            WORD32 first_scan_pos = 16;
            WORD32 last_scan_pos = -1;
            WORD32 sign_hidden;

            UWORD16 u2_gt0_flags = *pu2_sig_coeff_buf;
            WORD32 gt1_flags = *(pu2_sig_coeff_buf + 1);
            WORD32 sign_flags = *(pu2_sig_coeff_buf + 2);

            WORD32 sig_coeff_map = u2_gt0_flags;

            WORD32 gt1_bins = 0; /* bins for coeffs with abslevel > 1 */

            WORD32 sign_bins = 0; /* bins for sign flags of coded coeffs  */
            WORD32 num_coded = 0; /* total coeffs coded in 4x4            */

            WORD32 infer_coeff; /* infer when 0,0 is the only coded coeff */
            WORD32 bit; /* temp boolean */

            /* total count of coeffs to be coded as abs level remaining */
            WORD32 num_coeffs_remaining = 0;

            /* count of coeffs to be coded as  abslevel-1 */
            WORD32 num_coeffs_base1 = 0;
            WORD32 scan_pos;
            WORD32 first_gt1_coeff = 0;

            if((i != 0) || (0 == last_csb))
            {
                /* sanity check, atleast one coeff is coded as csbf is set */
                ASSERT(sig_coeff_map != 0);
            }

            pu2_sig_coeff_buf += 3;

            scan_pos = 15;
            if(i == last_csb)
            {
                /*************************************************************/
                /* clear last_scan_pos for last block in scan order as this  */
                /* is communicated  throught last_coeff_x and last_coeff_y   */
                /*************************************************************/
                WORD32 next_sig = CLZ(sig_coeff_map) + 1;

                scan_pos = WORD_SIZE - next_sig;

                /* prepare the bins for gt1 flags */
                EXTRACT_BIT(bit, gt1_flags, scan_pos);

                /* insert gt1 bin in lsb */
                gt1_bins |= bit;

                /* prepare the bins for sign flags */
                EXTRACT_BIT(bit, sign_flags, scan_pos);

                /* insert sign bin in lsb */
                sign_bins |= bit;

                sig_coeff_map = CLEAR_BIT(sig_coeff_map, scan_pos);

                if(-1 == last_scan_pos)
                    last_scan_pos = scan_pos;

                scan_pos--;
                num_coded++;
            }

            /* infer 0,0 coeff for all 4x4 blocks except fitst and last */
            infer_coeff = (i < last_csb) && (i > 0);

            /* encode the required sigcoeff flags (abslevel > 0)   */
            while(scan_pos >= 0)
            {
                WORD32 y_pos_x_pos;
                WORD32 sig_ctxinc = 0; /* 0 is default inc for DC coeff */

                WORD32 sig_coeff;

                EXTRACT_BIT(sig_coeff, sig_coeff_map, scan_pos);

                /* derive the x,y pos */
                y_pos_x_pos = gu1_hevce_scan4x4[scan_type][scan_pos];

                /* derive the context inc as per section 9.3.3.1.4 */
                if(2 == log2_tr_size)
                {
                    /* 4x4 transform size increment uses lookup */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc_tr4[y_pos_x_pos];
                }
                else if(scan_pos || i)
                {
                    /* ctxt for AC coeff depends on curpos and neigbour csbf */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc[nbr_csbf][y_pos_x_pos];

                    /* based on luma subblock pos */
                    sig_ctxinc += (i && is_luma) ? 3 : 0;
                }
                else
                {
                    /* DC coeff has fixed context for luma and chroma */
                    sig_coeff_base_ctxt = is_luma ? IHEVC_CAB_COEFF_FLAG
                                                  : IHEVC_CAB_COEFF_FLAG + 27;
                }

                /*************************************************************/
                /* encode sig coeff only if required                         */
                /* decoder infers 0,0 coeff when all the other coeffs are 0  */
                /*************************************************************/
                if(scan_pos || (!infer_coeff))
                {
                    ctxt_idx = sig_ctxinc + sig_coeff_base_ctxt;

                    //ret |= ihevce_cabac_encode_bin(ps_cabac, sig_coeff, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ sig_coeff];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] =
                            gau1_ihevc_next_state[(state_mps << 1) | sig_coeff];
                    }
                }

                if(sig_coeff)
                {
                    /* prepare the bins for gt1 flags */
                    EXTRACT_BIT(bit, gt1_flags, scan_pos);

                    /* shift and insert gt1 bin in lsb */
                    gt1_bins <<= 1;
                    gt1_bins |= bit;

                    /* prepare the bins for sign flags */
                    EXTRACT_BIT(bit, sign_flags, scan_pos);

                    /* shift and insert sign bin in lsb */
                    sign_bins <<= 1;
                    sign_bins |= bit;

                    num_coded++;

                    /* 0,0 coeff can no more be inferred :( */
                    infer_coeff = 0;

                    if(-1 == last_scan_pos)
                        last_scan_pos = scan_pos;

                    first_scan_pos = scan_pos;
                }

                scan_pos--;
            }

            /* Added for sign bit hiding*/
            sign_hidden =
                (((last_scan_pos - first_scan_pos) > 3 && !cu_tq_bypass_flag) && (perform_sbh));

            /****************************************************************/
            /* encode the abs level greater than 1 bins; Section 7.3.13     */
            /* These have already been prepared during sig_coeff_map encode */
            /* Context modelling done as per section 9.3.3.1.5              */
            /****************************************************************/
            {
                WORD32 j;

                /* context set based on luma subblock pos */
                WORD32 ctxt_set = (i && is_luma) ? 2 : 0;

                /* count of coeffs with abslevel > 1; max of 8 to be coded */
                WORD32 num_gt1_bins = MIN(8, num_coded);

                if(num_coded > 8)
                {
                    /* pull back the bins to required number */
                    gt1_bins >>= (num_coded - 8);

                    num_coeffs_remaining += (num_coded - 8);
                    num_coeffs_base1 = (num_coded - 8);
                }

                /* See section 9.3.3.1.5           */
                ctxt_set += (0 == gt1_ctxt) ? 1 : 0;

                gt1_ctxt = 1;

                for(j = num_gt1_bins - 1; j >= 0; j--)
                {
                    /* Encodet the abs level gt1 bins */
                    ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                    EXTRACT_BIT(bit, gt1_bins, j);

                    //ret |= ihevce_cabac_encode_bin(ps_cabac, bit, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ bit];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] = gau1_ihevc_next_state[(state_mps << 1) | bit];
                    }

                    if(bit)
                    {
                        gt1_ctxt = 0;
                        num_coeffs_remaining++;
                    }
                    else if(gt1_ctxt && (gt1_ctxt < 3))
                    {
                        gt1_ctxt++;
                    }
                }

                /*************************************************************/
                /* encode abs level greater than 2 bin; Section 7.3.13       */
                /*************************************************************/
                if(gt1_bins)
                {
                    WORD32 gt2_bin;

                    first_gt1_coeff = pu2_sig_coeff_buf[0] + 1;
                    gt2_bin = (first_gt1_coeff > 2);

                    /* atleast one level > 2 */
                    ctxt_idx = IHEVC_CAB_COEFABS_GRTR2_FLAG;

                    ctxt_idx += (is_luma) ? ctxt_set : (ctxt_set + 4);

                    //ret |= ihevce_cabac_encode_bin(ps_cabac, gt2_bin, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ gt2_bin];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] =
                            gau1_ihevc_next_state[(state_mps << 1) | gt2_bin];
                    }

                    if(!gt2_bin)
                    {
                        /* sanity check */
                        ASSERT(first_gt1_coeff == 2);

                        /* no need to send this coeff as bypass bins */
                        pu2_sig_coeff_buf++;
                        num_coeffs_remaining--;
                    }
                }
            }

            /*************************************************************/
            /* encode the coeff signs and abs remaing levels             */
            /*************************************************************/
            if(num_coded)
            {
                WORD32 base_level;
                WORD32 rice_param = 0;
                WORD32 j;

                /*************************************************************/
                /* encode the coeff signs populated in sign_bins             */
                /*************************************************************/
                if(sign_hidden && i4_sign_data_hiding_flag)
                {
                    sign_bins >>= 1;
                    num_coded--;
                }

                if(num_coded > 0)
                {
                    /* ret |= ihevce_cabac_encode_bypass_bins(ps_cabac,
                                                       sign_bins,
                                                       num_coded);
                    */

                    /* increment bits generated based on num bypass bins */
                    ps_cabac->u4_bits_estimated_q12 += (num_coded << CABAC_FRAC_BITS_Q);
                }

                /*************************************************************/
                /* encode the coeff_abs_level_remaining as TR / EGK bins     */
                /* See section 9.3.2.7 for details                           */
                /*************************************************************/

                /* first remaining coeff baselevel */
                if(first_gt1_coeff > 2)
                {
                    base_level = 3;
                }
                else if(num_coeffs_remaining > num_coeffs_base1)
                {
                    /* atleast one coeff in first 8 is gt > 1 */
                    base_level = 2;
                }
                else
                {
                    /* all coeffs have base of 1 */
                    base_level = 1;
                }

                for(j = 0; j < num_coeffs_remaining; j++)
                {
                    WORD32 abs_coeff = pu2_sig_coeff_buf[0] + 1;
                    WORD32 abs_coeff_rem;
                    WORD32 rice_max = (4 << rice_param);
                    WORD32 num_bins, unary_length;
                    UWORD32 u4_sym_shiftk_plus1;

                    pu2_sig_coeff_buf++;

                    /* sanity check */
                    ASSERT(abs_coeff >= base_level);

                    abs_coeff_rem = (abs_coeff - base_level);

                    /* TODO://HM-8.0-dev uses (3 << rice_param) as rice_max */
                    /* TODO://HM-8.0-dev does either TR or EGK but not both */
                    if(abs_coeff_rem >= rice_max)
                    {
                        UWORD32 u4_suffix = (abs_coeff_rem - rice_max);

                        /* coeff exceeds max rice limit                    */
                        /* encode the TR prefix as tunary code             */
                        /* prefix = 1111 as (rice_max >> rice_praram) = 4  */
                        /* ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, 0xF, 4); */

                        /* increment bits generated based on num bypass bins */
                        ps_cabac->u4_bits_estimated_q12 += (4 << CABAC_FRAC_BITS_Q);

                        /* encode the exponential golomb code suffix */
                        /*ret |= ihevce_cabac_encode_egk(ps_cabac,
                                                       u4_suffix,
                                                       (rice_param+1)
                                                      ); */

                        /* k = rice_param+1 */
                        /************************************************************************/
                        /* shift symbol by k bits to find unary code prefix (111110)            */
                        /* Use GETRANGE to elminate the while loop in sec 9.3.2.4 of HEVC spec  */
                        /************************************************************************/
                        u4_sym_shiftk_plus1 = (u4_suffix >> (rice_param + 1)) + 1;

                        /* GETRANGE(unary_length, (u4_sym_shiftk_plus1 + 1)); */
                        GETRANGE(unary_length, u4_sym_shiftk_plus1);

                        /* length of the code = 2 *(unary_length - 1) + 1 + k */
                        num_bins = (2 * unary_length) + rice_param;

                        /* increment bits generated based on num bypass bins */
                        ps_cabac->u4_bits_estimated_q12 += (num_bins << CABAC_FRAC_BITS_Q);
                    }
                    else
                    {
                        /* code coeff as truncated rice code  */
                        /* ret |= ihevce_cabac_encode_trunc_rice(ps_cabac,
                                                              abs_coeff_rem,
                                                              rice_param,
                                                              rice_max);
                                                              */

                        /************************************************************************/
                        /* shift symbol by c_rice_param bits to find unary code prefix (111.10) */
                        /************************************************************************/
                        unary_length = (abs_coeff_rem >> rice_param) + 1;

                        /* length of the code */
                        num_bins = unary_length + rice_param;

                        /* increment bits generated based on num bypass bins */
                        ps_cabac->u4_bits_estimated_q12 += (num_bins << CABAC_FRAC_BITS_Q);
                    }

                    /* update the rice param based on coeff level */
                    if((abs_coeff > (3 << rice_param)) && (rice_param < 4))
                    {
                        rice_param++;
                    }

                    /* change base level to 1 if more than 8 coded coeffs */
                    if((j + 1) < (num_coeffs_remaining - num_coeffs_base1))
                    {
                        base_level = 2;
                    }
                    else
                    {
                        base_level = 1;
                    }
                }
            }
        }
    }
    /*tap texture bits*/
    {
        ps_cabac->u4_texture_bits_estimated_q12 +=
            (ps_cabac->u4_bits_estimated_q12 - temp_tex_bits_q12);
    }

    return (ret);
}

/**
******************************************************************************
*
*  @brief Encodes a transform residual block as per section 7.3.13
*
*  @par   Description
*  RDOQ optimization is carried out here. When sub-blk RDOQ is turned on, we calculate
*  the distortion(D) and bits(R) for when the sub blk is coded and when not coded. We
*  then use the D+lambdaR metric to decide whether the sub-blk should be coded or not, and
*  aprropriately signal it. When coeff RDOQ is turned on, we traverse through the TU to
*  find all non-zero coeffs. If the non zero coeff is a 1, then we make a decision(based on D+lambdaR)
*  metric as to whether to code it as a 0 or 1. In case the coeff is > 1(say L where L>1) we choose betweem
*  L and L+1
*
*  @remarks Does not support sign data hiding and transform skip flag currently
*
*  @remarks Need to resolve the differences between JVT-J1003_d7 spec and
*           HM.8.0-dev for related abs_greater_than_1 context initialization
*           and rice_max paramtere used for coeff abs level remaining
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]      pv_coeff
*  Compressed residue buffer containing following information:
*
*
*  HEADER(4 bytes) : last_coeff_x, last_coeff_y, scantype, last_subblock_num
*
*  For each 4x4 subblock starting from last_subblock_num (in scan order)
*     Read 2 bytes  : MSB 12bits (0xBAD marker), bit0 cur_csbf, bit1-2 nbr csbf
*
*    `If cur_csbf
*      Read 2 bytes : sig_coeff_map (16bits in scan_order 1:coded, 0:not coded)
*      Read 2 bytes : abs_gt1_flags (max of 8 only)
*      Read 2 bytes : coeff_sign_flags
*
*      Based on abs_gt1_flags and sig_coeff_map read remaining abs levels
*      Read 2 bytes : remaining_abs_coeffs_minus1 (this is in a loop)
*
*  @param[in]      log2_tr_size
*  transform size of the current TU
*
*  @param[in]      is_luma
*  boolean indicating if the texture type is luma / chroma
*
*  @param[out]    pi4_tu_coded_dist
*  The distortion when the TU is coded(not all coeffs are set to 0) is stored here
*
*  @param[out]    pi4_tu_not_coded_dist
*  The distortion when the entire TU is not coded(all coeffs are set to 0) is stored here
*
*
*  @return      success or failure error code
*
******************************************************************************
*/

WORD32 ihevce_cabac_residue_encode_rdoq(
    entropy_context_t *ps_entropy_ctxt,
    void *pv_coeff,
    WORD32 log2_tr_size,
    WORD32 is_luma,
    void *pv_rdoq_ctxt,
    LWORD64 *pi8_tu_coded_dist,
    LWORD64 *pi8_tu_not_coded_dist,
    WORD32 perform_sbh)
{
    WORD32 *pi4_subBlock2csbfId_map;

    WORD32 ret = IHEVCE_SUCCESS;

    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;
    cab_ctxt_t s_sub_blk_not_coded_cabac_ctxt;
    backup_ctxt_t s_backup_ctxt;
    backup_ctxt_t s_backup_ctxt_sub_blk_not_coded;

    UWORD32 temp_tex_bits_q12;

    UWORD8 *pu1_coeff_buf_hdr = (UWORD8 *)pv_coeff;
    UWORD16 *pu2_sig_coeff_buf = (UWORD16 *)pv_coeff;

    LWORD64 i8_sub_blk_not_coded_dist = 0, i8_sub_blk_coded_dist = 0;
    WORD32 i4_sub_blk_not_coded_bits = 0, i4_sub_blk_coded_bits = 0;
    LWORD64 i8_sub_blk_not_coded_metric, i8_sub_blk_coded_metric;
    LWORD64 i8_tu_not_coded_dist = 0, i8_tu_coded_dist = 0;
    WORD32 i4_tu_coded_bits = 0;
    WORD32 temp_zero_col = 0, temp_zero_row = 0;

    UWORD8 *pu1_last_sig_coeff_x;
    UWORD8 *pu1_last_sig_coeff_y;
    WORD32 scan_type;
    WORD32 last_csb;

    WORD32 cur_csbf = 0, nbr_csbf;
    // WORD32 i4_temp_bits;

    WORD32 sig_coeff_base_ctxt; /* cabac context for sig coeff flag    */
    WORD32 abs_gt1_base_ctxt; /* cabac context for abslevel > 1 flag */

    UWORD8 *pu1_ctxt_model = &ps_cabac->au1_ctxt_models[0];

    rdoq_sbh_ctxt_t *ps_rdoq_ctxt = (rdoq_sbh_ctxt_t *)pv_rdoq_ctxt;
    WORD16 *pi2_coeffs = ps_rdoq_ctxt->pi2_quant_coeffs;
    WORD16 *pi2_tr_coeffs = ps_rdoq_ctxt->pi2_trans_values;
    WORD32 trans_size = ps_rdoq_ctxt->i4_trans_size;
    WORD32 i4_round_val = ps_rdoq_ctxt->i4_round_val_ssd_in_td;
    WORD32 i4_shift_val = ps_rdoq_ctxt->i4_shift_val_ssd_in_td;
    WORD32 scan_idx = ps_rdoq_ctxt->i4_scan_idx;

    UWORD8 *pu1_csb_table, *pu1_trans_table;
    WORD32 shift_value, mask_value;

    WORD32 gt1_ctxt = 1; /* required for abs_gt1_ctxt modelling */
    WORD32 temp_gt1_ctxt = gt1_ctxt;

    WORD32 i;
#if DISABLE_ZCSBF
    WORD32 i4_skip_zero_cbf = 0;
    WORD32 i4_skip_zero_csbf = 0;
    WORD32 i4_num_abs_1_coeffs = 0;
#endif
    (void)perform_sbh;
    pi4_subBlock2csbfId_map = ps_rdoq_ctxt->pi4_subBlock2csbfId_map;

    /* scan order inside a csb */
    pu1_csb_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);
    /*Initializing the backup_ctxt structures*/
    s_backup_ctxt.i4_num_bits = 0;
    s_backup_ctxt_sub_blk_not_coded.i4_num_bits = 0;

    memset(&s_backup_ctxt.au1_ctxt_to_backup, 0, MAX_NUM_CONTEXT_ELEMENTS);
    memset(&s_backup_ctxt_sub_blk_not_coded.au1_ctxt_to_backup, 0, MAX_NUM_CONTEXT_ELEMENTS);

    pu1_coeff_buf_hdr = (UWORD8 *)pv_coeff;
    pu2_sig_coeff_buf = (UWORD16 *)pv_coeff;

    /* last sig coeff indices in scan order */
    pu1_last_sig_coeff_x = &pu1_coeff_buf_hdr[0];
    pu1_last_sig_coeff_y = &pu1_coeff_buf_hdr[1];

    /* read the scan type : upright diag / horz / vert */
    scan_type = pu1_coeff_buf_hdr[2];

    /************************************************************************/
    /* position of the last coded sub block. This sub block contains coeff  */
    /* corresponding to last_sig_coeff_x, last_sig_coeff_y. Althoug this can*/
    /* be derived here it better to be populated by scanning module         */
    /************************************************************************/
    last_csb = pu1_coeff_buf_hdr[3];

    shift_value = ps_rdoq_ctxt->i4_log2_trans_size + 1;
    /* for finding. row no. from scan index */
    shift_value = shift_value - 3;
    /*for finding the col. no. from scan index*/
    mask_value = (ps_rdoq_ctxt->i4_trans_size / 4) - 1;

    switch(ps_rdoq_ctxt->i4_trans_size)
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

    /* sanity checks */
    /* transform skip not supported */
    ASSERT(0 == ps_entropy_ctxt->ps_pps->i1_transform_skip_enabled_flag);
    {
        temp_tex_bits_q12 = ps_cabac->u4_bits_estimated_q12;
    }
    /*************************************************************************/
    /* derive base context index for sig coeff as per section 9.3.3.1.4      */
    /* TODO; convert to look up based on luma/chroma, scan type and tfr size */
    /*************************************************************************/
    if(is_luma)
    {
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG;
        abs_gt1_base_ctxt = IHEVC_CAB_COEFABS_GRTR1_FLAG;

        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            sig_coeff_base_ctxt += (scan_type == SCAN_DIAG_UPRIGHT) ? 9 : 15;
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

    /* go to csbf flags */
    pu2_sig_coeff_buf = (UWORD16 *)(pu1_coeff_buf_hdr + COEFF_BUF_HEADER_LEN);

    /*Calculating the distortion produced by all the zero coeffs in the TU*/
    for(i = (trans_size * trans_size) - 1; i >= 0; i--)
    {
        WORD32 i4_dist;
        WORD16 *pi2_orig_coeff = ps_rdoq_ctxt->pi2_trans_values;

        if(pi2_coeffs[i] == 0)
        {
            i4_dist = CALC_SSD_IN_TRANS_DOMAIN(pi2_orig_coeff[i], 0, 0, 0);
            i8_tu_not_coded_dist += i4_dist;
            i8_tu_coded_dist += i4_dist;
        }
    }

    /*Backup of the various cabac ctxts*/
    memcpy(&s_sub_blk_not_coded_cabac_ctxt, ps_cabac, sizeof(cab_ctxt_t));
    /************************************************************************/
    /* encode the csbf, sig_coeff_map, abs_grt1_flags, abs_grt2_flag, sign  */
    /* and abs_coeff_remaining for each 4x4 starting from last scan to first*/
    /************************************************************************/

    for(i = last_csb; i >= 0; i--)
    {
        UWORD16 u2_marker_csbf;
        WORD32 ctxt_idx;
        WORD32 i4_sub_blk_is_coded = 0;
        WORD32 blk_row, blk_col;
        WORD32 scaled_blk_row;
        WORD32 scaled_blk_col;
        WORD32 infer_coeff;

        gt1_ctxt = temp_gt1_ctxt;
#if DISABLE_ZCSBF
        /*Initialize skip zero cbf flag to 0*/
        i4_skip_zero_csbf = 0;
        i4_num_abs_1_coeffs = 0;
#endif

#if OPT_MEMCPY
        ihevce_copy_backup_ctxt(
            (void *)&s_sub_blk_not_coded_cabac_ctxt,
            (void *)ps_cabac,
            (void *)&s_backup_ctxt_sub_blk_not_coded,
            (void *)&s_backup_ctxt);
        memset(s_backup_ctxt_sub_blk_not_coded.au1_ctxt_to_backup, 0, 5);
        memset(s_backup_ctxt.au1_ctxt_to_backup, 0, 5);
#else
        memcpy(&s_sub_blk_not_coded_cabac_ctxt, ps_cabac, sizeof(cab_ctxt_t));
#endif
        // i4_temp_bits = s_sub_blk_not_coded_cabac_ctxt.u4_bits_estimated_q12;

        blk_row = pu1_trans_table[i] >> shift_value; /*row of csb*/
        blk_col = pu1_trans_table[i] & mask_value; /*col of csb*/

        scaled_blk_row = blk_row << 2;
        scaled_blk_col = blk_col << 2;

        infer_coeff = (i < last_csb) && (i > 0);
        u2_marker_csbf = *pu2_sig_coeff_buf;

        if((blk_col + 1 < trans_size / 4)) /* checking right boundary */
        {
            if(!ps_rdoq_ctxt
                    ->pu1_csbf_buf[pi4_subBlock2csbfId_map[blk_row * trans_size / 4 + blk_col + 1]])
            {
                /* clear the 2nd bit if the right csb is 0 */
                u2_marker_csbf = u2_marker_csbf & (~(1 << 1));
            }
        }
        if((blk_row + 1 < trans_size / 4)) /* checking bottom boundary */
        {
            if(!ps_rdoq_ctxt
                    ->pu1_csbf_buf[pi4_subBlock2csbfId_map[(blk_row + 1) * trans_size / 4 + blk_col]])
            {
                /* clear the 3rd bit if the bottom csb is 0*/
                u2_marker_csbf = u2_marker_csbf & (~(1 << 2));
            }
        }
        pu2_sig_coeff_buf++;

        /* sanity checks for marker present in every csbf flag */
        ASSERT((u2_marker_csbf >> 4) == 0xBAD);

        /* extract the current and neigbour csbf flags */
        cur_csbf = u2_marker_csbf & 0x1;
        nbr_csbf = (u2_marker_csbf >> 1) & 0x3;

        if((i < last_csb) && (i > 0))
        {
            ctxt_idx = IHEVC_CAB_CODED_SUBLK_IDX;

            /* ctxt based on right / bottom avail csbf, section 9.3.3.1.3 */
            ctxt_idx += nbr_csbf ? 1 : 0;
            ctxt_idx += is_luma ? 0 : 2;

            ret |= ihevce_cabac_encode_bin(ps_cabac, cur_csbf, ctxt_idx);

            s_backup_ctxt.au1_ctxt_to_backup[SUB_BLK_CODED_FLAG] = 1;

            if(cur_csbf)
            {
                ret |= ihevce_cabac_encode_bin(&s_sub_blk_not_coded_cabac_ctxt, 0, ctxt_idx);
                // clang-format off
                i4_sub_blk_not_coded_bits =
                    s_sub_blk_not_coded_cabac_ctxt.u4_bits_estimated_q12;  // - i4_temp_bits;
                s_backup_ctxt_sub_blk_not_coded.au1_ctxt_to_backup[SUB_BLK_CODED_FLAG] = 1;
                // clang-format on
            }
        }
        else
        {
            /* sanity check, this csb contains the last_sig_coeff */
            if(i == last_csb)
            {
                ASSERT(cur_csbf == 1);
            }
        }
        /*If any block in the TU is coded and the 0th block is not coded, the 0th
          block is still signalled as csbf = 1, and with all sig_coeffs sent as
          0(HEVC requirement)*/
        if((ps_rdoq_ctxt->i1_tu_is_coded == 1) && (i == 0))
        {
            i4_sub_blk_not_coded_bits = ihevce_code_all_sig_coeffs_as_0_explicitly(
                (void *)ps_rdoq_ctxt,
                i,
                pu1_trans_table,
                is_luma,
                scan_type,
                infer_coeff,
                nbr_csbf,
                &s_sub_blk_not_coded_cabac_ctxt);
        }

        if(i == last_csb)
        {
            WORD32 i4_last_x = *pu1_last_sig_coeff_x;
            WORD32 i4_last_y = *pu1_last_sig_coeff_y;
            if(SCAN_VERT == scan_type)
            {
                /* last coeff x and y are swapped for vertical scan */
                SWAP(i4_last_x, i4_last_y);
            }
            /* Encode the last_sig_coeff_x and last_sig_coeff_y */
            ret |= ihevce_cabac_encode_last_coeff_x_y(
                ps_cabac, i4_last_x, i4_last_y, log2_tr_size, is_luma);
            s_backup_ctxt.au1_ctxt_to_backup[LASTXY] = 1;
        }

        if(cur_csbf)
        {
            /*****************************************************************/
            /* encode the sig coeff map as per section 7.3.13                */
            /* significant_coeff_flags: msb=coeff15-lsb=coeff0 in scan order */
            /*****************************************************************/

            WORD32 i4_bit_depth;
            WORD32 i4_shift_iq;
            WORD32 i4_dequant_val;
            WORD32 bit; /* temp boolean */

            UWORD16 u2_gt0_flags = *pu2_sig_coeff_buf;
            WORD32 sig_coeff_map = u2_gt0_flags;
            WORD32 gt1_flags = *(pu2_sig_coeff_buf + 1);
            WORD32 sign_flags = *(pu2_sig_coeff_buf + 2);

            WORD32 gt1_bins = 0; /* bins for coeffs with abslevel > 1 */

            WORD16 *pi2_dequant_coeff = ps_rdoq_ctxt->pi2_dequant_coeff;
            WORD16 i2_qp_rem = ps_rdoq_ctxt->i2_qp_rem;
            WORD32 i4_qp_div = ps_rdoq_ctxt->i4_qp_div;

            WORD32 sign_bins = 0; /* bins for sign flags of coded coeffs  */
            WORD32 num_coded = 0; /* total coeffs coded in 4x4            */

            /* total count of coeffs to be coded as abs level remaining */
            WORD32 num_coeffs_remaining = 0;

            /* count of coeffs to be coded as  abslevel-1 */
            WORD32 num_coeffs_base1 = 0;
            WORD32 scan_pos;
            WORD32 first_gt1_coeff = 0;

            i4_bit_depth = ps_entropy_ctxt->ps_sps->i1_bit_depth_luma_minus8 + 8;
            i4_shift_iq = i4_bit_depth + ps_rdoq_ctxt->i4_log2_trans_size - 5;

            i4_sub_blk_is_coded = 1;

            if((i != 0) || (0 == last_csb))
            {
                /* sanity check, atleast one coeff is coded as csbf is set */
                ASSERT(sig_coeff_map != 0);
            }
            /*Calculating the distortions produced*/
            {
                WORD32 k, j;
                WORD16 *pi2_temp_coeff =
                    &pi2_coeffs[scaled_blk_col + (scaled_blk_row * trans_size)];
                WORD16 *pi2_temp_tr_coeff =
                    &pi2_tr_coeffs[scaled_blk_col + (scaled_blk_row * trans_size)];
                WORD16 *pi2_temp_dequant_coeff =
                    &pi2_dequant_coeff[scaled_blk_col + (scaled_blk_row * trans_size)];

                for(k = 0; k < 4; k++)
                {
                    for(j = 0; j < 4; j++)
                    {
                        if(*pi2_temp_coeff)
                        {
                            /*Inverse quantizing for distortion calculation*/
                            if(ps_rdoq_ctxt->i4_trans_size != 4)
                            {
                                IQUANT(
                                    i4_dequant_val,
                                    *pi2_temp_coeff,
                                    *pi2_temp_dequant_coeff * g_ihevc_iquant_scales[i2_qp_rem],
                                    i4_shift_iq,
                                    i4_qp_div);
                            }
                            else
                            {
                                IQUANT_4x4(
                                    i4_dequant_val,
                                    *pi2_temp_coeff,
                                    *pi2_temp_dequant_coeff * g_ihevc_iquant_scales[i2_qp_rem],
                                    i4_shift_iq,
                                    i4_qp_div);
                            }

                            i8_sub_blk_coded_dist +=
                                CALC_SSD_IN_TRANS_DOMAIN(*pi2_temp_tr_coeff, i4_dequant_val, 0, 0);

                            i8_sub_blk_not_coded_dist +=
                                CALC_SSD_IN_TRANS_DOMAIN(*pi2_temp_tr_coeff, 0, 0, 0);
                        }
#if DISABLE_ZCSBF
                        if(abs(*pi2_temp_coeff) > 1)
                        {
                            i4_skip_zero_csbf = 1;
                        }
                        else if(abs(*pi2_temp_coeff) == 1)
                        {
                            i4_num_abs_1_coeffs++;
                        }
#endif
                        pi2_temp_coeff++;
                        pi2_temp_tr_coeff++;
                        pi2_temp_dequant_coeff++;
                    }
                    pi2_temp_tr_coeff += ps_rdoq_ctxt->i4_trans_size - 4;
                    pi2_temp_coeff += ps_rdoq_ctxt->i4_q_data_strd - 4;
                    pi2_dequant_coeff += ps_rdoq_ctxt->i4_trans_size - 4;
                }
            }

#if DISABLE_ZCSBF
            i4_skip_zero_csbf = i4_skip_zero_csbf || (i4_num_abs_1_coeffs > 3);
#endif
            pu2_sig_coeff_buf += 3;

            scan_pos = 15;
            if(i == last_csb)
            {
                /*************************************************************/
                /* clear last_scan_pos for last block in scan order as this  */
                /* is communicated  throught last_coeff_x and last_coeff_y   */
                /*************************************************************/
                WORD32 next_sig = CLZ(sig_coeff_map) + 1;

                scan_pos = WORD_SIZE - next_sig;

                /* prepare the bins for gt1 flags */
                EXTRACT_BIT(bit, gt1_flags, scan_pos);

                /* insert gt1 bin in lsb */
                gt1_bins |= bit;

                /* prepare the bins for sign flags */
                EXTRACT_BIT(bit, sign_flags, scan_pos);

                /* insert sign bin in lsb */
                sign_bins |= bit;

                sig_coeff_map = CLEAR_BIT(sig_coeff_map, scan_pos);

                scan_pos--;
                num_coded++;
            }

            /* encode the required sigcoeff flags (abslevel > 0)   */
            while(scan_pos >= 0)
            {
                WORD32 y_pos_x_pos;
                WORD32 sig_ctxinc = 0; /* 0 is default inc for DC coeff */

                WORD32 sig_coeff;

                EXTRACT_BIT(sig_coeff, sig_coeff_map, scan_pos);

                /* derive the x,y pos */
                y_pos_x_pos = gu1_hevce_scan4x4[scan_type][scan_pos];

                /* derive the context inc as per section 9.3.3.1.4 */
                if(2 == log2_tr_size)
                {
                    /* 4x4 transform size increment uses lookup */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc_tr4[y_pos_x_pos];
                }
                else if(scan_pos || i)
                {
                    /* ctxt for AC coeff depends on curpos and neigbour csbf */
                    sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc[nbr_csbf][y_pos_x_pos];

                    /* based on luma subblock pos */
                    sig_ctxinc += (i && is_luma) ? 3 : 0;
                }
                else
                {
                    /* DC coeff has fixed context for luma and chroma */
                    sig_coeff_base_ctxt = is_luma ? IHEVC_CAB_COEFF_FLAG
                                                  : IHEVC_CAB_COEFF_FLAG + 27;
                }

                /*************************************************************/
                /* encode sig coeff only if required                         */
                /* decoder infers 0,0 coeff when all the other coeffs are 0  */
                /*************************************************************/
                if(scan_pos || (!infer_coeff))
                {
                    ctxt_idx = sig_ctxinc + sig_coeff_base_ctxt;
                    //ret |= ihevce_cabac_encode_bin(ps_cabac, sig_coeff, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ sig_coeff];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] =
                            gau1_ihevc_next_state[(state_mps << 1) | sig_coeff];
                    }
                }

                if(sig_coeff)
                {
                    /* prepare the bins for gt1 flags */
                    EXTRACT_BIT(bit, gt1_flags, scan_pos);

                    /* shift and insert gt1 bin in lsb */
                    gt1_bins <<= 1;
                    gt1_bins |= bit;

                    /* prepare the bins for sign flags */
                    EXTRACT_BIT(bit, sign_flags, scan_pos);

                    /* shift and insert sign bin in lsb */
                    sign_bins <<= 1;
                    sign_bins |= bit;

                    num_coded++;

                    /* 0,0 coeff can no more be inferred :( */
                    infer_coeff = 0;
                }

                scan_pos--;
            }

            s_backup_ctxt.au1_ctxt_to_backup[SIG_COEFF] = 1;

            /****************************************************************/
            /* encode the abs level greater than 1 bins; Section 7.3.13     */
            /* These have already been prepared during sig_coeff_map encode */
            /* Context modelling done as per section 9.3.3.1.5              */
            /****************************************************************/
            {
                WORD32 j;

                /* context set based on luma subblock pos */
                WORD32 ctxt_set = (i && is_luma) ? 2 : 0;

                /* count of coeffs with abslevel > 1; max of 8 to be coded */
                WORD32 num_gt1_bins = MIN(8, num_coded);

                if(num_coded > 8)
                {
                    /* pull back the bins to required number */
                    gt1_bins >>= (num_coded - 8);

                    num_coeffs_remaining += (num_coded - 8);
                    num_coeffs_base1 = (num_coded - 8);
                }

                /* See section 9.3.3.1.5           */
                ctxt_set += (0 == gt1_ctxt) ? 1 : 0;

                gt1_ctxt = 1;

                for(j = num_gt1_bins - 1; j >= 0; j--)
                {
                    /* Encodet the abs level gt1 bins */
                    ctxt_idx = (ctxt_set * 4) + abs_gt1_base_ctxt + gt1_ctxt;

                    EXTRACT_BIT(bit, gt1_bins, j);

                    //ret |= ihevce_cabac_encode_bin(ps_cabac, bit, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ bit];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] = gau1_ihevc_next_state[(state_mps << 1) | bit];
                    }

                    if(bit)
                    {
                        gt1_ctxt = 0;
                        num_coeffs_remaining++;
                    }
                    else if(gt1_ctxt && (gt1_ctxt < 3))
                    {
                        gt1_ctxt++;
                    }
                }
                s_backup_ctxt.au1_ctxt_to_backup[GRTR_THAN_1] = 1;
                /*************************************************************/
                /* encode abs level greater than 2 bin; Section 7.3.13       */
                /*************************************************************/
                if(gt1_bins)
                {
                    WORD32 gt2_bin;

                    first_gt1_coeff = pu2_sig_coeff_buf[0] + 1;
                    gt2_bin = (first_gt1_coeff > 2);

                    /* atleast one level > 2 */
                    ctxt_idx = IHEVC_CAB_COEFABS_GRTR2_FLAG;

                    ctxt_idx += (is_luma) ? ctxt_set : (ctxt_set + 4);

                    //ret |= ihevce_cabac_encode_bin(ps_cabac, gt2_bin, ctxt_idx);
                    {
                        WORD32 state_mps = pu1_ctxt_model[ctxt_idx];

                        /* increment bits generated based on state and bin encoded */
                        ps_cabac->u4_bits_estimated_q12 +=
                            gau2_ihevce_cabac_bin_to_bits[state_mps ^ gt2_bin];

                        /* update the context model from state transition LUT */
                        pu1_ctxt_model[ctxt_idx] =
                            gau1_ihevc_next_state[(state_mps << 1) | gt2_bin];
                    }

                    if(!gt2_bin)
                    {
                        /* sanity check */
                        ASSERT(first_gt1_coeff == 2);

                        /* no need to send this coeff as bypass bins */
                        pu2_sig_coeff_buf++;
                        num_coeffs_remaining--;
                    }
                    s_backup_ctxt.au1_ctxt_to_backup[GRTR_THAN_2] = 1;
                }
            }

            /*************************************************************/
            /* encode the coeff signs and abs remaing levels             */
            /*************************************************************/
            if(num_coded)
            {
                WORD32 base_level;
                WORD32 rice_param = 0;
                WORD32 j;

                /*************************************************************/
                /* encode the coeff signs populated in sign_bins             */
                /*************************************************************/
                if(num_coded > 0)
                {
                    ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, sign_bins, num_coded);
                }
                /*************************************************************/
                /* encode the coeff_abs_level_remaining as TR / EGK bins     */
                /* See section 9.3.2.7 for details                           */
                /*************************************************************/

                /* first remaining coeff baselevel */
                if(first_gt1_coeff > 2)
                {
                    base_level = 3;
                }
                else if(num_coeffs_remaining > num_coeffs_base1)
                {
                    /* atleast one coeff in first 8 is gt > 1 */
                    base_level = 2;
                }
                else
                {
                    /* all coeffs have base of 1 */
                    base_level = 1;
                }

                for(j = 0; j < num_coeffs_remaining; j++)
                {
                    WORD32 abs_coeff = pu2_sig_coeff_buf[0] + 1;
                    WORD32 abs_coeff_rem;
                    WORD32 rice_max = (4 << rice_param);

                    pu2_sig_coeff_buf++;

                    /* sanity check */
                    ASSERT(abs_coeff >= base_level);

                    abs_coeff_rem = (abs_coeff - base_level);

                    /* TODO://HM-8.0-dev uses (3 << rice_param) as rice_max */
                    /* TODO://HM-8.0-dev does either TR or EGK but not both */
                    if(abs_coeff_rem >= rice_max)
                    {
                        UWORD32 u4_suffix = (abs_coeff_rem - rice_max);

                        /* coeff exceeds max rice limit                    */
                        /* encode the TR prefix as tunary code             */
                        /* prefix = 1111 as (rice_max >> rice_praram) = 4  */
                        ret |= ihevce_cabac_encode_bypass_bins(ps_cabac, 0xF, 4);

                        /* encode the exponential golomb code suffix */
                        ret |= ihevce_cabac_encode_egk(ps_cabac, u4_suffix, (rice_param + 1));
                    }
                    else
                    {
                        /* code coeff as truncated rice code  */
                        ret |= ihevce_cabac_encode_trunc_rice(
                            ps_cabac, abs_coeff_rem, rice_param, rice_max);
                    }

                    /* update the rice param based on coeff level */
                    if((abs_coeff > (3 << rice_param)) && (rice_param < 4))
                    {
                        rice_param++;
                    }

                    /* change base level to 1 if more than 8 coded coeffs */
                    if((j + 1) < (num_coeffs_remaining - num_coeffs_base1))
                    {
                        base_level = 2;
                    }
                    else
                    {
                        base_level = 1;
                    }
                }
            }

            i4_sub_blk_coded_bits = ps_cabac->u4_bits_estimated_q12;
            /**********************************************************/
            /**********************************************************/
            /**********************************************************/
            /*Decide whether sub block should be coded or not*/
            /**********************************************************/
            /**********************************************************/
            /**********************************************************/
            i8_sub_blk_coded_metric = CALC_CUMMUL_SSD_IN_TRANS_DOMAIN(
                                          i8_sub_blk_coded_dist, 0, i4_round_val, i4_shift_val) +
                                      COMPUTE_RATE_COST_CLIP30_RDOQ(
                                          i4_sub_blk_coded_bits,
                                          ps_rdoq_ctxt->i8_cl_ssd_lambda_qf,
                                          (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
            i8_sub_blk_not_coded_metric =
                CALC_CUMMUL_SSD_IN_TRANS_DOMAIN(
                    i8_sub_blk_not_coded_dist, 0, i4_round_val, i4_shift_val) +
                COMPUTE_RATE_COST_CLIP30_RDOQ(
                    i4_sub_blk_not_coded_bits,
                    ps_rdoq_ctxt->i8_cl_ssd_lambda_qf,
                    (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));

#if DISABLE_ZCSBF
            if(((i8_sub_blk_not_coded_metric < i8_sub_blk_coded_metric) ||
                (i4_sub_blk_is_coded == 0)) &&
               (i4_skip_zero_csbf == 0))
#else
            if((i8_sub_blk_not_coded_metric < i8_sub_blk_coded_metric) ||
               (i4_sub_blk_is_coded == 0))
#endif
            {
#if OPT_MEMCPY
                ihevce_copy_backup_ctxt(
                    (void *)ps_cabac,
                    (void *)&s_sub_blk_not_coded_cabac_ctxt,
                    (void *)&s_backup_ctxt,
                    (void *)&s_backup_ctxt_sub_blk_not_coded);
#else
                memcpy(ps_cabac, &s_sub_blk_not_coded_cabac_ctxt, sizeof(cab_ctxt_t));
#endif
                scan_pos = 15;
                i4_sub_blk_is_coded = 0;

                {
                    WORD32 k, j;
                    WORD16 *pi2_temp_coeff =
                        &pi2_coeffs[scaled_blk_col + (scaled_blk_row * ps_rdoq_ctxt->i4_q_data_strd)];
                    WORD16 *pi2_temp_iquant_coeff =
                        &ps_rdoq_ctxt->pi2_iquant_coeffs
                             [scaled_blk_col + (scaled_blk_row * ps_rdoq_ctxt->i4_iq_data_strd)];
                    for(k = 0; k < 4; k++)
                    {
                        for(j = 0; j < 4; j++)
                        {
                            *pi2_temp_coeff = 0;
                            *pi2_temp_iquant_coeff = 0;

                            pi2_temp_coeff++;
                            pi2_temp_iquant_coeff++;
                        }
                        pi2_temp_coeff += ps_rdoq_ctxt->i4_q_data_strd - 4;
                        pi2_temp_iquant_coeff += ps_rdoq_ctxt->i4_iq_data_strd - 4;
                    }
                }

                /* If the csb to be masked is the last csb, then we should
                 * signal last x and last y from the next coded sub_blk */
                if(i == last_csb)
                {
                    pu1_coeff_buf_hdr = (UWORD8 *)pu2_sig_coeff_buf;

                    ps_rdoq_ctxt->pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[i]]] = 0;
                    last_csb = ihevce_find_new_last_csb(
                        pi4_subBlock2csbfId_map,
                        i,
                        (void *)ps_rdoq_ctxt,
                        pu1_trans_table,
                        pu1_csb_table,
                        pi2_coeffs,
                        shift_value,
                        mask_value,
                        &pu1_coeff_buf_hdr);
                    /*We are in a for loop. This means that the decrement to i happens immediately right
                      at the end of the for loop. This would decrement the value of i to (last_csb - 1).
                      Hence we increment i by 1, so that after the decrement i becomes last_csb.*/
                    i = last_csb + 1;
                    pu1_last_sig_coeff_x = &pu1_coeff_buf_hdr[0];
                    pu1_last_sig_coeff_y = &pu1_coeff_buf_hdr[1];
                    scan_type = pu1_coeff_buf_hdr[2];
                    pu2_sig_coeff_buf = (UWORD16 *)(pu1_coeff_buf_hdr + 4);
                }
                i8_tu_coded_dist += i8_sub_blk_not_coded_dist;
                i4_tu_coded_bits += i4_sub_blk_not_coded_bits;
            }
            else
            {
                ps_rdoq_ctxt->i1_tu_is_coded = 1;
                temp_gt1_ctxt = gt1_ctxt;

                i8_tu_coded_dist += i8_sub_blk_coded_dist;
                i4_tu_coded_bits += i4_sub_blk_coded_bits;
            }
#if DISABLE_ZCSBF
            i4_skip_zero_cbf = i4_skip_zero_cbf || i4_skip_zero_csbf;
#endif
            /*Cumulating the distortion for the entire TU*/
            i8_tu_not_coded_dist += i8_sub_blk_not_coded_dist;
            //i4_tu_coded_dist                += i4_sub_blk_coded_dist;
            //i4_tu_coded_bits                += i4_sub_blk_coded_bits;
            i8_sub_blk_not_coded_dist = 0;
            i4_sub_blk_not_coded_bits = 0;
            i8_sub_blk_coded_dist = 0;
            i4_sub_blk_coded_bits = 0;

            if(i4_sub_blk_is_coded)
            {
                ps_rdoq_ctxt->pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[i]]] = 1;
                temp_zero_col = (temp_zero_col) | (0xF << scaled_blk_col);
                temp_zero_row = (temp_zero_row) | (0xF << scaled_blk_row);
            }
            else
            {
                if(!((ps_rdoq_ctxt->i1_tu_is_coded == 1) && (i == 0)))
                {
                    ps_rdoq_ctxt->pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[i]]] = 0;
                }
            }
        }
    }

    /*tap texture bits*/
    {
        ps_cabac->u4_texture_bits_estimated_q12 +=
            (ps_cabac->u4_bits_estimated_q12 - temp_tex_bits_q12);
    }

    i8_tu_not_coded_dist =
        CALC_CUMMUL_SSD_IN_TRANS_DOMAIN(i8_tu_not_coded_dist, 0, i4_round_val, i4_shift_val);

    /* i4_tu_coded_dist = CALC_CUMMUL_SSD_IN_TRANS_DOMAIN(
        i4_tu_coded_dist, 0, i4_round_val, i4_shift_val); */
    *pi8_tu_coded_dist = i8_tu_coded_dist;
    *pi8_tu_not_coded_dist = i8_tu_not_coded_dist;
#if DISABLE_ZCSBF
    if(i4_skip_zero_cbf == 1)
    {
        *pi8_tu_not_coded_dist = 0x7FFFFFFF;
    }
#endif

    *ps_rdoq_ctxt->pi4_zero_col = ~temp_zero_col;
    *ps_rdoq_ctxt->pi4_zero_row = ~temp_zero_row;

    return (ret);
}

/**
******************************************************************************
*
*  @brief Codes all the sig coeffs as 0
*
*  @param[in]   i
*  Index of the current csb
*
*  @param[in]   pu1_trans_table
*  Pointer to the trans table
*
*  @param[in]  scan_type
*  Determines the scan order
*
*  @param[in]  infer_coeff
*  Indicates whether the 0,0 coeff can be inferred or not
*
*  @param[in]   nbr_csbf
*  Talks about if the neighboour csbs(right and bottom) are coded or not
*
*  @param[in]    ps_cabac
*  Cabac state
*
*  @param[out]    pi4_tu_not_coded_dist
*  The distortion when the entire TU is not coded(all coeffs are set to 0) is stored here
*
*  @return    The number of bits generated when the 0th sub blk is coded as all 0s
*             This is the cumulate bits(i.e. for all blocks in the TU), and not only
*             the bits generated for this block
*
******************************************************************************
*/
WORD32 ihevce_code_all_sig_coeffs_as_0_explicitly(
    void *pv_rdoq_ctxt,
    WORD32 i,
    UWORD8 *pu1_trans_table,
    WORD32 is_luma,
    WORD32 scan_type,
    WORD32 infer_coeff,
    WORD32 nbr_csbf,
    cab_ctxt_t *ps_cabac)
{
    WORD32 sig_coeff_base_ctxt;
    WORD32 scan_pos = 15;
    WORD32 ctxt_idx;
    WORD32 ret = 0;

    rdoq_sbh_ctxt_t *ps_rdoq_ctxt = (rdoq_sbh_ctxt_t *)pv_rdoq_ctxt;

    WORD32 log2_tr_size = ps_rdoq_ctxt->i4_log2_trans_size;

    (void)pu1_trans_table;
    if(is_luma)
    {
        sig_coeff_base_ctxt = IHEVC_CAB_COEFF_FLAG;
        if(3 == log2_tr_size)
        {
            /* 8x8 transform size */
            sig_coeff_base_ctxt += (scan_type == SCAN_DIAG_UPRIGHT) ? 9 : 15;
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
    while(scan_pos >= 0)
    {
        WORD32 sig_ctxinc = 0; /* 0 is default inc for DC coeff */
        WORD32 sig_coeff = 0;
        /* derive the x,y pos */
        WORD32 y_pos_x_pos = gu1_hevce_scan4x4[scan_type][scan_pos];

        /* derive the context inc as per section 9.3.3.1.4 */
        if(2 == log2_tr_size)
        {
            /* 4x4 transform size increment uses lookup */
            sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc_tr4[y_pos_x_pos];
        }
        else if(scan_pos || i)
        {
            /* ctxt for AC coeff depends on curpos and neigbour csbf */
            sig_ctxinc = gu1_hevce_sigcoeff_ctxtinc[nbr_csbf][y_pos_x_pos];

            /* based on luma subblock pos */
            sig_ctxinc += (i && is_luma) ? 3 : 0;
        }
        else
        {
            /* DC coeff has fixed context for luma and chroma */
            sig_coeff_base_ctxt = is_luma ? IHEVC_CAB_COEFF_FLAG : IHEVC_CAB_COEFF_FLAG + 27;
        }

        if(scan_pos || (!infer_coeff))
        {
            ctxt_idx = sig_ctxinc + sig_coeff_base_ctxt;
            ret |= ihevce_cabac_encode_bin(ps_cabac, sig_coeff, ctxt_idx);
            AEV_TRACE("significant_coeff_flag", sig_coeff, ps_cabac->u4_range);
        }
        scan_pos--;
    }
    return (ps_cabac->u4_bits_estimated_q12);  // - i4_temp_bits);
}

/**
******************************************************************************
*
*  @brief Finds the next csb with a non-zero coeff
*
*  @paramp[in]  cur_last_csb_pos
*  The index of the current csb with a non-zero coeff
*
*  @param[inout]   pv_rdoq_ctxt
*  RODQ context structure
*
*  @param[in]   pu1_trans_table
*  Pointer to the trans table
*
*  @param[in]   pi2_coeffs
*  Pointer to all the quantized coefficients
*
*  @param[in]  shift_value
*  Determines the shifting value for determining appropriate position of coeff
*
*  @param[in]  mask_value
*  Determines the masking value for determining appropriate position of coeff
*
*  @param[in]   nbr_csbf
*  Talks about if the neighboour csbs(right and bottom) are coded or not
*
*  @param[in]    ps_cabac
*  Cabac state
*
*  @param[inout] ppu1_addr
*  Pointer to the header(i.e. pointer used for traversing the ecd data generated
*  in ihevce_scan_coeffs)
*
*  @return    The index of the csb with the next non-zero coeff
*
******************************************************************************
*/
WORD32 ihevce_find_new_last_csb(
    WORD32 *pi4_subBlock2csbfId_map,
    WORD32 cur_last_csb_pos,
    void *pv_rdoq_ctxt,
    UWORD8 *pu1_trans_table,
    UWORD8 *pu1_csb_table,
    WORD16 *pi2_coeffs,
    WORD32 shift_value,
    WORD32 mask_value,
    UWORD8 **ppu1_addr)
{
    WORD32 blk_row;
    WORD32 blk_col;
    WORD32 x_pos;
    WORD32 y_pos;
    WORD32 i;
    WORD32 j;
    UWORD16 *pu2_out_data_coeff;
    rdoq_sbh_ctxt_t *ps_rdoq_ctxt = (rdoq_sbh_ctxt_t *)pv_rdoq_ctxt;
    WORD32 trans_size = ps_rdoq_ctxt->i4_trans_size;
    UWORD8 *pu1_out_data_header = *ppu1_addr;

    for(i = cur_last_csb_pos - 1; i >= 0; i--)
    {
        /* check for the first csb flag in our scan order */
        if(ps_rdoq_ctxt->pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[i]]])
        {
            UWORD8 u1_last_x, u1_last_y;
            WORD32 quant_coeff;

            pu1_out_data_header -= 4;  //To move the pointer back to the appropriate position
            /* row of csb */
            blk_row = pu1_trans_table[i] >> shift_value;
            /* col of csb */
            blk_col = pu1_trans_table[i] & mask_value;

            /*check for the 1st non-0 values inside the csb in our scan order*/
            for(j = 15; j >= 0; j--)
            {
                x_pos = (pu1_csb_table[j] & 0x3) + blk_col * 4;
                y_pos = (pu1_csb_table[j] >> 2) + blk_row * 4;

                quant_coeff = pi2_coeffs[x_pos + (y_pos * trans_size)];

                if(quant_coeff != 0)
                    break;
            }

            ASSERT(j >= 0);

            u1_last_x = x_pos;
            u1_last_y = y_pos;

            /* storing last_x and last_y */
            *(pu1_out_data_header) = u1_last_x;
            *(pu1_out_data_header + 1) = u1_last_y;

            /* storing the scan order */
            *(pu1_out_data_header + 2) = ps_rdoq_ctxt->i4_scan_idx;

            /* storing last_sub_block pos. in scan order count */
            *(pu1_out_data_header + 3) = i;

            /*stored the first 4 bytes, now all are word16. So word16 pointer*/
            pu2_out_data_coeff = (UWORD16 *)(pu1_out_data_header + 4);

            *pu2_out_data_coeff = 0xBAD0 | 1; /*since right&bottom csbf is 0*/
            *ppu1_addr = pu1_out_data_header;

            break; /*We just need this loop for finding 1st non-zero csb only*/
        }
        else
            pu1_out_data_header += 2;
    }
    return i;
}

/**
******************************************************************************
*
*  @brief Used to optimize the memcpy of cabac states. It copies only those
*  states in the cabac context which have been altered.
*
*  @paramp[inout]  pv_dest
*  Pointer to desitination cabac state.
*
*  @param[inout]   pv_backup_ctxt_dest
*  Pointer to destination backup context
*
*  @param[inout]   pv_backup_ctxt_src
*  Pointer to source backup context
*
*  @Desc:
*  We go through each element in the backup_ctxt structure which will tell us
*  if the states corresponding to lastxlasty, sigcoeffs, grtr_than_1_bins,
*  grtr_than_2_bins and sub_blk_coded_flag(i.e. 0xBAD0) context elements
*  have been altered. If they have been altered, we will memcpy the states
*  corresponding to these context elements alone
*
*  @return  Nothing
*
******************************************************************************
*/
void ihevce_copy_backup_ctxt(
    void *pv_dest, void *pv_src, void *pv_backup_ctxt_dest, void *pv_backup_ctxt_src)
{
    UWORD8 *pu1_dest = (UWORD8 *)(((cab_ctxt_t *)pv_dest)->au1_ctxt_models);
    UWORD8 *pu1_src = (UWORD8 *)(((cab_ctxt_t *)pv_src)->au1_ctxt_models);
    backup_ctxt_t *ps_backup_dest_ctxt = ((backup_ctxt_t *)pv_backup_ctxt_dest);
    backup_ctxt_t *ps_backup_src_ctxt = ((backup_ctxt_t *)pv_backup_ctxt_src);
    WORD32 i4_i;

    /*
    0       IHEVC_CAB_COEFFX_PREFIX         lastx last y has been coded
    1       IHEVC_CAB_CODED_SUBLK_IDX       sub-blk coded or not flag has been coded
    2       IHEVC_CAB_COEFF_FLAG            sigcoeff has been coded
    3       IHEVC_CAB_COEFABS_GRTR1_FLAG    greater than 1 bin has been coded
    4       IHEVC_CAB_COEFABS_GRTR2_FLAG    greater than 2 bin has been coded*/
    assert(MAX_NUM_CONTEXT_ELEMENTS == 5);
    for(i4_i = 0; i4_i < MAX_NUM_CONTEXT_ELEMENTS; i4_i++)
    {
        if((ps_backup_src_ctxt->au1_ctxt_to_backup[SIG_COEFF]) ||
           (ps_backup_dest_ctxt->au1_ctxt_to_backup[SIG_COEFF]))
        {
            memcpy(&pu1_dest[IHEVC_CAB_COEFF_FLAG], &pu1_src[IHEVC_CAB_COEFF_FLAG], 42);
            ps_backup_dest_ctxt->au1_ctxt_to_backup[SIG_COEFF] = 0;
            ps_backup_src_ctxt->au1_ctxt_to_backup[SIG_COEFF] = 0;
        }
        if((ps_backup_src_ctxt->au1_ctxt_to_backup[GRTR_THAN_1]) ||
           (ps_backup_dest_ctxt->au1_ctxt_to_backup[GRTR_THAN_1]))
        {
            memcpy(
                &pu1_dest[IHEVC_CAB_COEFABS_GRTR1_FLAG],
                &pu1_src[IHEVC_CAB_COEFABS_GRTR1_FLAG],
                24);
            ps_backup_dest_ctxt->au1_ctxt_to_backup[GRTR_THAN_1] = 0;
            ps_backup_src_ctxt->au1_ctxt_to_backup[GRTR_THAN_1] = 0;
        }
        if((ps_backup_src_ctxt->au1_ctxt_to_backup[GRTR_THAN_2]) ||
           (ps_backup_dest_ctxt->au1_ctxt_to_backup[GRTR_THAN_2]))
        {
            memcpy(
                &pu1_dest[IHEVC_CAB_COEFABS_GRTR2_FLAG], &pu1_src[IHEVC_CAB_COEFABS_GRTR2_FLAG], 6);
            ps_backup_dest_ctxt->au1_ctxt_to_backup[GRTR_THAN_2] = 0;
            ps_backup_src_ctxt->au1_ctxt_to_backup[GRTR_THAN_2] = 0;
        }
        if((ps_backup_src_ctxt->au1_ctxt_to_backup[SUB_BLK_CODED_FLAG]) ||
           (ps_backup_dest_ctxt->au1_ctxt_to_backup[SUB_BLK_CODED_FLAG]))
        {
            memcpy(&pu1_dest[IHEVC_CAB_CODED_SUBLK_IDX], &pu1_src[IHEVC_CAB_CODED_SUBLK_IDX], 4);
            ps_backup_dest_ctxt->au1_ctxt_to_backup[SUB_BLK_CODED_FLAG] = 0;
            ps_backup_src_ctxt->au1_ctxt_to_backup[SUB_BLK_CODED_FLAG] = 0;
        }
        if((ps_backup_src_ctxt->au1_ctxt_to_backup[LASTXY]) ||
           (ps_backup_dest_ctxt->au1_ctxt_to_backup[LASTXY]))
        {
            memcpy(&pu1_dest[IHEVC_CAB_COEFFX_PREFIX], &pu1_src[IHEVC_CAB_COEFFX_PREFIX], 36);
            ps_backup_dest_ctxt->au1_ctxt_to_backup[LASTXY] = 0;
            ps_backup_src_ctxt->au1_ctxt_to_backup[LASTXY] = 0;
        }
    }
    ((cab_ctxt_t *)pv_dest)->u4_bits_estimated_q12 = ((cab_ctxt_t *)pv_src)->u4_bits_estimated_q12;
}
