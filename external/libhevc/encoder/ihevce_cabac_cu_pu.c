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
* @file ihevce_cabac_cu_pu.c
*
* @brief
*  This file contains function definitions for cabac entropy coding of CU
*  and PU structures in HEVC syntax
*
* @author
*  ittiam
*
* @List of Functions
*  ihevce_cabac_encode_intra_pu()
*  ihevce_cabac_encode_skip_flag()
*  ihevce_cabac_encode_part_mode()
*  ihevce_cabac_encode_merge_idx()
*  ihevce_cabac_encode_inter_pred_idc()
*  ihevce_cabac_encode_refidx()
*  ihevce_cabac_encode_mvd()
*  ihevce_cabac_encode_inter_pu()
*  ihevce_cabac_encode_coding_unit()
*  ihevce_cabac_encode_sao()
*  ihevce_encode_coding_quadtree()
*  ihevce_encode_slice_data()
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
#include "ihevce_trace.h"

#define TEST_CABAC_BITESTIMATE 0

// clang-format off
/**
******************************************************************************
* @brief  LUT for binarization of inter partmode bins for cu size > mincu size
*  as per Table9-34 of spec
*
* @input   : amp_enable flag  and part_mode
*
* @output  : packed bins and count of bins as per following bit packed format
*            Bins      : (bits3-bit0) first bin starts from bit3
*            Bins Count: (bits7-bit4)
*            0xFF in the following table is invalid entry
*
* @remarks See Table 9-34 of HEVC spec for Binarization of part_mode
*******************************************************************************
*/
#define INVALID 0xFF
const UWORD8 gu1_hevce_inter_part_mode_bins[2][8] = {

    /*  cusize > minCUsize, no amp       */
    { 0x18, 0x24, 0x20, INVALID, INVALID, INVALID, INVALID, INVALID, },

    /*  cusize > minCUsize, amp enable, minCUsize > 8 (irrelevant)  */
    { 0x18, 0x36, 0x32, INVALID, 0x44, 0x45, 0x40, 0x41, },

};

/**
******************************************************************************
* @brief  LUT for binarization of inter partmode bins for cu size = mincu size
*  as per Table9-34 of spec
*
* @input   : mincusize==8 flag  and part_mode
*
* @output  : packed bins and count of bins as per following bit packed format
*            Bins      : (bits3-bit0) first bin starts from bit3
*            Bins Count: (bits7-bit4)
*            0xFF in the following table is invalid entry
*
* @remarks See Table 9-34 of HEVC spec for Binarization of part_mode
*******************************************************************************
*/
const UWORD8 gu1_hevce_inter_part_mode_bins_mincu[2][4] = {

    /*  cusize == minCUsize,  minCUsize > 8      */
    { 0x18, 0x24, 0x32, 0x30, },

    /* cusize == minCUsize,   minCUsize = 8       */
    { 0x18, 0x24, 0x20, INVALID },

};
// clang-format on

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/
/**
******************************************************************************
*
*  @brief Entropy encoding of luma and chroma intra pred modes
*
*  @par   Description
*  Encodes prev_intra_ped_mode, mpm_idx and rem_intra_pred_mode for each
*  luma partition and chrom intra pred of cu as per section:7.3.9.1
*
*  Binzarization, context model as per Table 9-32 for luma
*  Binzarization, context model as per Table 9-35, section 9.3.2.8 for chroma
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]   part_mode
*  indicates whether the mode is 2Nx2N or NxN luma parition
*
*  @param[in]   ps_enc_cu
*  pointer to the intra cu whose luma and chroma pred modes are encoded
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_intra_pu(
    entropy_context_t *ps_entropy_ctxt, WORD32 part_mode, cu_enc_loop_out_t *ps_enc_cu)
{
    WORD32 error = IHEVCE_SUCCESS;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;
    intra_prev_rem_flags_t *ps_prev_mpm_rem_flags = &ps_enc_cu->as_prev_rem[0];
    WORD32 i, num_parts;

    /* intra can only be 2Nx2N partition or a NxN partition     */
    num_parts = (PART_NxN == part_mode) ? 4 : 1;

    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
    {
        WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;

        /*PIC_INFO : INTRA CU in frame*/
        ps_entropy_ctxt->ps_pic_level_info->i8_total_intra_cu++;
        ps_entropy_ctxt->ps_pic_level_info->i8_total_pu += num_parts;
        ps_entropy_ctxt->ps_pic_level_info->i8_total_intra_pu += num_parts;
        /*PIC_INFO : Total CU in frame based on cu size */

        if(PART_2Nx2N == part_mode)
        {
            // clang-format off
            if(cu_size == 64)
                ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_intra_pu[3]++;
            else
                ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_intra_pu[cu_size >> 4]++;
            // clang-format on
        }
        else if(PART_NxN == part_mode)
        {
            ps_entropy_ctxt->ps_pic_level_info->i8_total_nxn_intra_pu++;
        }
    }
    /* encode prev intra pred mode flags  : context model based */
    for(i = 0; i < num_parts; i++)
    {
        WORD32 prev_intra_pred_flag = ps_prev_mpm_rem_flags[i].b1_prev_intra_luma_pred_flag;
        error |=
            ihevce_cabac_encode_bin(ps_cabac, prev_intra_pred_flag, IHEVC_CAB_INTRA_LUMA_PRED_FLAG);
        AEV_TRACE("prev_intra_pred_luma_flag", prev_intra_pred_flag, ps_cabac->u4_range);
    }

    /* encode mpm_idx or rem_intra_pred_mode bypass bins */
    for(i = 0; i < num_parts; i++)
    {
        if(ps_prev_mpm_rem_flags[i].b1_prev_intra_luma_pred_flag)
        {
            WORD32 mpm_idx = ps_prev_mpm_rem_flags[i].b2_mpm_idx;

            /* tunary bins for cmax = 2 */
            WORD32 num_bins = mpm_idx ? 2 : 1;
            UWORD32 bins = mpm_idx ? ((1 << 1) | (mpm_idx - 1)) : 0;

            ASSERT(mpm_idx < 3);

            error |= ihevce_cabac_encode_bypass_bins(ps_cabac, bins, num_bins);
            AEV_TRACE("mpm_idx", mpm_idx, ps_cabac->u4_range);
        }
        else
        {
            WORD32 rem_intra_pred_mode = ps_prev_mpm_rem_flags[i].b5_rem_intra_pred_mode;
            error |= ihevce_cabac_encode_bypass_bins(ps_cabac, rem_intra_pred_mode, 5);
            AEV_TRACE("rem_intra_luma_pred_mode", rem_intra_pred_mode, ps_cabac->u4_range);
        }
    }

    /************************************************************************/
    /* encode the chroma intra prediction mode as per Table 9-35            */
    /* First bin is context model based prefix : 0 if chroma_mode==4 else 1 */
    /* If chroma pred mode is not 4, suffix bins are coded as bypass bins   */
    /************************************************************************/
    {
        WORD32 chroma_pred_mode = ps_enc_cu->b3_chroma_intra_pred_mode;
        WORD32 prefix_bin = (chroma_pred_mode == 4) ? 0 : 1;

        /* encode prefix bin */
        error |= ihevce_cabac_encode_bin(ps_cabac, prefix_bin, IHEVC_CAB_CHROMA_PRED_MODE);

        /* encode suffix bins */
        if(prefix_bin)
        {
            error |= ihevce_cabac_encode_bypass_bins(ps_cabac, chroma_pred_mode, 2);
        }
        AEV_TRACE("intra_chroma_pred_mode", chroma_pred_mode, ps_cabac->u4_range);
    }

    return (error);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of skip flag (Coding Unit syntax)
*
*  @par   Description
*  context increment for skip flag is derived based on left and top skip flag
*  as per section 9.3.3.1.1, Table 9-38
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]   ps_enc_cu
*  pointer to inter cu whose skip flag is to be coded
*
*  @param[in]   top_avail
*  top availabilty flag for current cu (boolean)
*
*  @param[in]   left_avail
*  left availabilty flag for current cu (boolean)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_skip_flag(
    entropy_context_t *ps_entropy_ctxt,
    cu_enc_loop_out_t *ps_enc_cu,
    WORD32 top_avail,
    WORD32 left_avail)

{
    WORD32 error = IHEVCE_SUCCESS;
    WORD32 skip_flag = ps_enc_cu->b1_skip_flag;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    /* CU top left co-ordinates w.r.t ctb */
    WORD32 cu_x0 = ps_enc_cu->b3_cu_pos_x << 3;
    WORD32 cu_y0 = ps_enc_cu->b3_cu_pos_y << 3;

    /* CU size in pels */
    WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;

    /* CU x co-ordinate w.r.t frame start           */
    WORD32 ctb_x0_frm = (ps_entropy_ctxt->i4_ctb_x << ps_entropy_ctxt->i1_log2_ctb_size);

    WORD32 cu_x0_frm = cu_x0 + ctb_x0_frm;

    /* bit postion from where top skip flag is extracted; 1bit per 8 pel   */
    WORD32 x_pos = ((cu_x0_frm >> 3) & 0x7);

    /* bit postion from where left skip flag is extracted; 1bit per 8 pel  */
    WORD32 y_pos = ((cu_y0 >> 3) & 0x7);

    /* top and left skip flags computed based on nbr availability */
    UWORD8 *pu1_top_skip_flags = ps_entropy_ctxt->pu1_skip_cu_top + (cu_x0_frm >> 6);
    UWORD32 u4_skip_left_flags = ps_entropy_ctxt->u4_skip_cu_left;

    /* context incerements based on top and left neigbours */
    UWORD32 ctxt_inc = 0;

    if(top_avail)
    {
        WORD32 val;
        EXTRACT_BIT(val, pu1_top_skip_flags[0], x_pos);
        ctxt_inc += val;
    }

    if(left_avail)
    {
        WORD32 val;
        EXTRACT_BIT(val, u4_skip_left_flags, y_pos);
        ctxt_inc += val;
    }

    if(CABAC_MODE_COMPUTE_BITS == ps_cabac->e_cabac_op_mode)
    {
        //ASSERT(ctxt_inc == ps_entropy_ctxt->i4_num_nbr_skip_cus);
        ctxt_inc = ps_entropy_ctxt->i4_num_nbr_skip_cus;
        ASSERT(ctxt_inc < 3);
        ASSERT((WORD32)ctxt_inc <= (top_avail + left_avail));
    }

    /* encode the skip flag */
    error |= ihevce_cabac_encode_bin(ps_cabac, skip_flag, (IHEVC_CAB_SKIP_FLAG + ctxt_inc));

    AEV_TRACE("cu_skip_flag", skip_flag, ps_cabac->u4_range);

    if(CABAC_MODE_ENCODE_BITS == ps_cabac->e_cabac_op_mode)
    {
        /* update top and left skip flags only in encode mode */
        if(skip_flag)
        {
            SET_BITS(pu1_top_skip_flags[0], x_pos, (cu_size >> 3));
            SET_BITS(u4_skip_left_flags, y_pos, (cu_size >> 3));
        }
        else
        {
            CLEAR_BITS(pu1_top_skip_flags[0], x_pos, (cu_size >> 3));
            CLEAR_BITS(u4_skip_left_flags, y_pos, (cu_size >> 3));
        }

        ps_entropy_ctxt->u4_skip_cu_left = u4_skip_left_flags;
    }

    return (error);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of partition mode (Coding Unit syntax)
*
*  @par   Description
*  Binarization process and context modelling of partition mode is done as per
*  section 9.3.2.6 (Table 9-34) and se
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]   intra
*  boolean indicating if current cu is intra cu
*
*  @param[in]   is_mincu
*  boolean indicating if current cu size is equal to mincu
*
*  @param[in]   amp_enabled
*  flag to indicate if AMP(Assymetric motion partition) is enabled at sps level
*
*  @param[in]   cu_eq_8
*  boolean indicating if current cu size is equal to 8
*
*  @param[in]   part_mode
*  partition mode of current CU
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_part_mode(
    cab_ctxt_t *ps_cabac,
    WORD32 intra,
    WORD32 is_mincu,
    WORD32 amp_enabled,
    WORD32 cu_eq_8,
    WORD32 part_mode)
{
    /* Binarization depends on intra/inter, is_mincu, amp flag, cbsize == 8 */
    WORD32 bins;
    WORD32 bin_count, i;
    WORD32 error = IHEVCE_SUCCESS;

    (void)is_mincu;
    (void)amp_enabled;
    (void)cu_eq_8;
    if(intra)
    {
        /* sanity checks for intra part mode */
        ASSERT(is_mincu);
        ASSERT((part_mode == SIZE_NxN) || (part_mode == SIZE_2Nx2N));

        bins = (part_mode == SIZE_2Nx2N) ? 1 : 0;
        error |= ihevce_cabac_encode_bin(ps_cabac, bins, IHEVC_CAB_PART_MODE);
    }
    else
    {
        /* sanity checks for inter part mode....Too many but good to have  */
        ASSERT((amp_enabled == 0) || (amp_enabled == 1));
        ASSERT((is_mincu == 0) || (is_mincu == 1));
        ASSERT((cu_eq_8 == 0) || (cu_eq_8 == 1));
        ASSERT((part_mode <= SIZE_nRx2N) && (part_mode >= SIZE_2Nx2N));
        if(!amp_enabled)
            ASSERT(part_mode <= SIZE_NxN);
        if(!is_mincu)
            ASSERT(part_mode != SIZE_NxN);
        if(is_mincu)
            ASSERT(part_mode <= SIZE_NxN);
        if(cu_eq_8)
            ASSERT(part_mode < SIZE_NxN);
        if(cu_eq_8)
            ASSERT(is_mincu);

        /* look up table for bins and number of bins for inter pred mode    */
        if(!is_mincu)
        {
            bins = gu1_hevce_inter_part_mode_bins[amp_enabled][part_mode];
        }
        else
        {
            bins = gu1_hevce_inter_part_mode_bins_mincu[cu_eq_8][part_mode];
        }

        bin_count = (bins >> 4) & 0xF;

        /* Encode the context model based bins, max of 3 */
        for(i = 0; i < MIN(bin_count, 3); i++)
        {
            //TODO: HM-8.0-dev uses 0 context increment for bin2 (i===2) when amp is enabled
            WORD32 ctxt_inc = IHEVC_CAB_PART_MODE + i;
            WORD32 bin = (bins >> (3 - i)) & 0x1;
            error |= ihevce_cabac_encode_bin(ps_cabac, bin, ctxt_inc);
        }

        /* Encode the last bin as bypass bin for amp partitions */
        if(bin_count == 4)
        {
            error |= ihevce_cabac_encode_bypass_bin(ps_cabac, (bins & 0x1));
        }
    }
    AEV_TRACE("part_mode", part_mode, ps_cabac->u4_range);
    return (error);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of merge_idx of inter prediction unit as per sec
*  as per sec 9.3.2 Table9-32. (tunary binarization)
*
*  @par   Description
*  trunacted unary binarization is done based on max merge candidates
*  First bin is context modelled bin and the rest are coded as bypass
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]   merge_idx
*  merge idx of the pu to be encoded;
*
*  @param[in]   max_merge_cand
*  maximum merge candidates signalled in the slice header*
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_merge_idx(cab_ctxt_t *ps_cabac, WORD32 merge_idx, WORD32 max_merge_cand)
{
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 ctxt_inc = IHEVC_CAB_MERGE_IDX_EXT;

    /* sanity checks */
    ASSERT((merge_idx >= 0) && (merge_idx < max_merge_cand));

    /* encode the merge idx only if required */
    if(max_merge_cand > 1)
    {
        /* encode the context modelled first bin */
        ret |= ihevce_cabac_encode_bin(ps_cabac, (merge_idx > 0), ctxt_inc);

        /* encode the remaining bins as bypass tunary */
        if((max_merge_cand > 2) && (merge_idx > 0))
        {
            ret |=
                ihevce_cabac_encode_tunary_bypass(ps_cabac, (merge_idx - 1), (max_merge_cand - 2));
        }

        AEV_TRACE("merge_idx", merge_idx, ps_cabac->u4_range);
    }

    return (ret);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of inter_pred_idc for prediction unit of B slice as
*   per sec 9.3.2.9 Table9-36
*
*  @par   Description
*  Max of two context modelled bins coded for pu size > 8x4 or 4x8
*  one context modelled bin coded for pu size = 8x4 or 4x8; bipred not allowed
*  for 8x4 or 4x8.
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]   inter_pred_idc
*  inter pred mode  to be encoded; shall be PRED_L0 or PRED_L1 or PRED_BI
*
*  @param[in]   cu_depth
*  depth of the cu to which current pu belongs (required for context increment)
*
*  @param[in]   pu_w_plus_pu_h
*  required to check if pu_w_plus_pu_h is 12 (8x4PU or 4x8PU)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_inter_pred_idc(
    cab_ctxt_t *ps_cabac, WORD32 inter_pred_idc, WORD32 cu_depth, WORD32 pu_w_plus_pu_h)
{
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 ctxt_inc;

    ASSERT(inter_pred_idc <= PRED_BI);

    /* check if PU is 8x4/4x8  */
    if(pu_w_plus_pu_h == 12)
    {
        /* case of 8x4 or 4x8 where bi_pred is not allowed */
        ASSERT((inter_pred_idc == PRED_L0) || (inter_pred_idc == PRED_L1));

        ctxt_inc = IHEVC_CAB_INTER_PRED_IDC + 4;
        ret |= ihevce_cabac_encode_bin(ps_cabac, inter_pred_idc, ctxt_inc);
    }
    else
    {
        /* larger PUs can be encoded as bi_pred/l0/l1 inter_pred_idc */
        WORD32 is_bipred = (inter_pred_idc == PRED_BI);

        ctxt_inc = IHEVC_CAB_INTER_PRED_IDC + cu_depth;
        ret |= ihevce_cabac_encode_bin(ps_cabac, is_bipred, ctxt_inc);

        if(!is_bipred)
        {
            ctxt_inc = IHEVC_CAB_INTER_PRED_IDC + 4;
            ret |= ihevce_cabac_encode_bin(ps_cabac, inter_pred_idc, ctxt_inc);
        }
    }

    AEV_TRACE("inter_pred_idc", inter_pred_idc, ps_cabac->u4_range);

    return (ret);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of refidx for prediction unit; Binarization done as
*   tunary code as per sec 9.3.2 Table9-32
*
*  @par   Description
*  First two bins are context modelled while the rest are coded as bypass
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]   ref_idx
*  ref idx of partition unit
*
*  @param[in]   active_refs
*  max number of active references signalled in slice header
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_refidx(cab_ctxt_t *ps_cabac, WORD32 ref_idx, WORD32 active_refs)
{
    /************************************************************/
    /* encode ref_idx as tunary binarization Table 9-32         */
    /* First 2 bin use context model and rest coded as  bypass  */
    /************************************************************/
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 ctxt_inc = IHEVC_CAB_INTER_REF_IDX;

    /* sanity checks */
    ASSERT((ref_idx >= 0) && (ref_idx < active_refs));

    /* encode the ref idx only if required */
    if(active_refs > 1)
    {
        /* encode the context modelled first bin */
        ret |= ihevce_cabac_encode_bin(ps_cabac, (ref_idx > 0), ctxt_inc);

        if((active_refs > 2) && (ref_idx > 0))
        {
            /* encode the context modelled second bin */
            ctxt_inc++;
            ret |= ihevce_cabac_encode_bin(ps_cabac, (ref_idx > 1), ctxt_inc);
        }

        if((active_refs > 3) && (ref_idx > 1))
        {
            /* encode remaining bypass bins */
            ret |= ihevce_cabac_encode_tunary_bypass(ps_cabac, (ref_idx - 2), (active_refs - 3));
        }

        AEV_TRACE("ref_idx", ref_idx, ps_cabac->u4_range);
    }

    return (ret);
}

/**
******************************************************************************
*
*  @brief Entropy encoding of mvd for inter pu as per section 7.3.10.2
*
*  @par   Description
*  syntax coded as per section 7.3.10.2 for mvdx and mvdy
*  context modeling of abs_mvd_greater0 abs_mvd_greater1 done as per Table 9-32
*  binazrization of abs_mvd_minus2 is done as done as EG1 code section 9.3.2.4
*
*  @param[inout]   ps_cabac
*  pointer to cabac encoding context (handle)
*
*  @param[in]   ps_mvd
*  pointer to mvd struct containing mvdx and mvdy
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_mvd(cab_ctxt_t *ps_cabac, mv_t *ps_mvd)
{
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 mvd_x = ps_mvd->i2_mvx;
    WORD32 mvd_y = ps_mvd->i2_mvy;

    WORD32 abs_mvd_x = ABS(mvd_x);
    WORD32 abs_mvd_y = ABS(mvd_y);

    WORD32 abs_mvd_x_gt0 = abs_mvd_x > 0;
    WORD32 abs_mvd_y_gt0 = abs_mvd_y > 0;

    WORD32 abs_mvd_x_gt1 = abs_mvd_x > 1;
    WORD32 abs_mvd_y_gt1 = abs_mvd_y > 1;

    WORD32 ctxt_inc = IHEVC_CAB_MVD_GRT0;

    /* encode absmvd_x > 0 */
    ret |= ihevce_cabac_encode_bin(ps_cabac, abs_mvd_x_gt0, ctxt_inc);
    AEV_TRACE("abs_mvd_greater0_flag[0]", abs_mvd_x_gt0, ps_cabac->u4_range);

    /* encode absmvd_y > 0 */
    ret |= ihevce_cabac_encode_bin(ps_cabac, abs_mvd_y_gt0, ctxt_inc);
    AEV_TRACE("abs_mvd_greater0_flag[1]", abs_mvd_y_gt0, ps_cabac->u4_range);

    ctxt_inc = IHEVC_CAB_MVD_GRT1;

    /* encode abs_mvd_x > 1 iff (abs_mvd_x > 0) */
    if(abs_mvd_x_gt0)
    {
        ret |= ihevce_cabac_encode_bin(ps_cabac, abs_mvd_x_gt1, ctxt_inc);
        AEV_TRACE("abs_mvd_greater1_flag[0]", abs_mvd_x_gt1, ps_cabac->u4_range);
    }

    /* encode abs_mvd_y > 1 iff (abs_mvd_y > 0) */
    if(abs_mvd_y_gt0)
    {
        ret |= ihevce_cabac_encode_bin(ps_cabac, abs_mvd_y_gt1, ctxt_inc);
        AEV_TRACE("abs_mvd_greater1_flag[1]", abs_mvd_y_gt1, ps_cabac->u4_range);
    }

    /* encode abs_mvd_x - 2 iff (abs_mvd_x > 1) */
    if(abs_mvd_x_gt1)
    {
        ret |= ihevce_cabac_encode_egk(ps_cabac, (abs_mvd_x - 2), 1);
        AEV_TRACE("abs_mvd_minus2[0]", (abs_mvd_x - 2), ps_cabac->u4_range);
    }

    /* encode mvd_x sign iff (abs_mvd_x > 0) */
    if(abs_mvd_x_gt0)
    {
        ret |= ihevce_cabac_encode_bypass_bin(ps_cabac, (mvd_x < 0));
        AEV_TRACE("mvd_sign_flag[0]", (mvd_x < 0), ps_cabac->u4_range);
    }

    /* encode abs_mvd_y - 2 iff (abs_mvd_y > 1) */
    if(abs_mvd_y_gt1)
    {
        ret |= ihevce_cabac_encode_egk(ps_cabac, (abs_mvd_y - 2), 1);
        AEV_TRACE("abs_mvd_minus2[1]", (abs_mvd_y - 2), ps_cabac->u4_range);
    }

    /* encode mvd_y sign iff (abs_mvd_y > 0) */
    if(abs_mvd_y_gt0)
    {
        ret |= ihevce_cabac_encode_bypass_bin(ps_cabac, (mvd_y < 0));
        AEV_TRACE("mvd_sign_flag[1]", (mvd_y < 0), ps_cabac->u4_range);
    }

    return ret;
}

/**
******************************************************************************
*
*  @brief Entropy encoding of all syntax elements of inter PUs in a CU
*
*  @par   Description
*  syntax coded as per section 7.3.10.1 for inter prediction unit
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]   ps_enc_cu
*  pointer to current cu whose inter prediction units are to be encoded
*
*  @param[in]   cu_depth
*  depth of the the current cu in coding tree
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_inter_pu(
    entropy_context_t *ps_entropy_ctxt, cu_enc_loop_out_t *ps_enc_cu, WORD32 cu_depth)
{
    WORD32 ret = IHEVCE_SUCCESS;

    slice_header_t *ps_slice_hdr = ps_entropy_ctxt->ps_slice_hdr;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;
    pu_t *ps_pu = ps_enc_cu->ps_pu;

    WORD32 merge_idx = ps_pu->b3_merge_idx;
    WORD32 max_merge_cand = ps_slice_hdr->i1_max_num_merge_cand;
    WORD32 ctxt_inc;

    if(ps_enc_cu->b1_skip_flag)
    {
        WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;
        /*PIC_INFO : SKIP CU in frame*/
        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {
            ps_entropy_ctxt->ps_pic_level_info->i8_total_skip_cu++;
            ps_entropy_ctxt->ps_pic_level_info->i8_total_pu++;
            if(cu_size == 64)
                ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_inter_pu[3]++;
            else
                ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_inter_pu[cu_size >> 4]++;
        }
        /* encode the merge idx for skip cu and return */
        ret |= ihevce_cabac_encode_merge_idx(ps_cabac, merge_idx, max_merge_cand);
    }
    else
    {
        /* MODE_INTER */
        WORD32 part_mode = ps_enc_cu->b3_part_mode;
        WORD32 num_parts, i;

        num_parts = (part_mode == SIZE_2Nx2N) ? 1 : ((part_mode == SIZE_NxN) ? 4 : 2);

        /*PIC_INFO : INTER CU in frame*/
        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {
            WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;
            ps_entropy_ctxt->ps_pic_level_info->i8_total_inter_cu++;
            ps_entropy_ctxt->ps_pic_level_info->i8_total_pu += num_parts;

            // clang-format off
            if(PART_2Nx2N == part_mode)
            {
                if(cu_size == 64)
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_inter_pu[3]++;
                else
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_2nx2n_inter_pu[cu_size >> 4]++;
            }
            else if((PART_2NxN == part_mode) || (PART_Nx2N == part_mode))
            {
                if(cu_size == 64)
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_smp_inter_pu[3]++;
                else
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_smp_inter_pu[cu_size >> 4]++;
            }
            else if((PART_2NxnU == part_mode) || (PART_2NxnD == part_mode) ||
                    (PART_nLx2N == part_mode) || (PART_nRx2N == part_mode))
            {
                ps_entropy_ctxt->ps_pic_level_info->i8_total_amp_inter_pu[cu_size >> 5]++;
            }
            else
            {
                ps_entropy_ctxt->ps_pic_level_info->i8_total_nxn_inter_pu[cu_size >> 5]++;
            }
            // clang-format on
        }

        /* encode each pu partition */
        for(i = 0; i < num_parts; i++)
        {
            /* encode the merge flag context modelled bin */
            WORD32 merge_flag;
            UWORD32 u4_bits_estimated_merge_flag = 0;
            ps_pu = ps_enc_cu->ps_pu + i;

            /* encode the merge flag context modelled bin */
            merge_flag = ps_pu->b1_merge_flag;
            u4_bits_estimated_merge_flag = ps_cabac->u4_bits_estimated_q12;
            ctxt_inc = IHEVC_CAB_MERGE_FLAG_EXT;
            ret |= ihevce_cabac_encode_bin(ps_cabac, merge_flag, ctxt_inc);

            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {
                // clang-format off
                /*PIC INFO : Populate merge flag */
                ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_merge_flag =
                    (ps_cabac->u4_bits_estimated_q12 -
                        u4_bits_estimated_merge_flag);
                // clang-format on
            }
            AEV_TRACE("merge_flag", merge_flag, ps_cabac->u4_range);

            if(merge_flag)
            {
                merge_idx = ps_pu->b3_merge_idx;
                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_merge_pu++;
                /* encode the merge idx for the pu */
                ret |= ihevce_cabac_encode_merge_idx(ps_cabac, merge_idx, max_merge_cand);
            }
            else
            {
                /* encode the inter_pred_idc, ref_idx and mvd */
                WORD32 inter_pred_idc = ps_pu->b2_pred_mode;
                WORD32 ref_l0_active = ps_slice_hdr->i1_num_ref_idx_l0_active;
                WORD32 ref_l1_active = ps_slice_hdr->i1_num_ref_idx_l1_active;

                /*PIC_INFO : L0 L1 BI ro r1.. in frame*/
                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_non_skipped_inter_pu++;
                    // clang-format off
                    if(inter_pred_idc == PRED_L0)
                    {
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_L0_mode++;
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_L0_ref_idx[ps_pu->mv.i1_l0_ref_idx]++;
                    }
                    else if(inter_pred_idc == PRED_L1)
                    {
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_L1_mode++;
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_L1_ref_idx[ps_pu->mv.i1_l1_ref_idx]++;
                    }
                    else if(inter_pred_idc == PRED_BI)
                    {
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_BI_mode++;
                        if(inter_pred_idc != PRED_L1)
                            ps_entropy_ctxt->ps_pic_level_info->i8_total_L0_ref_idx[ps_pu->mv.i1_l0_ref_idx]++;
                        if(inter_pred_idc != PRED_L0)
                            ps_entropy_ctxt->ps_pic_level_info->i8_total_L1_ref_idx[ps_pu->mv.i1_l1_ref_idx]++;
                    }
                    // clang-format on
                }
                if(ps_slice_hdr->i1_slice_type == BSLICE)
                {
                    /* Encode inter_pred_idc as per sec 9.3.2.9 Table9-36 */
                    WORD32 pu_w_plus_pu_h;
                    WORD32 inter_pred_idc = ps_pu->b2_pred_mode;

                    /* required to check if w+h==12 case */
                    pu_w_plus_pu_h = ((ps_pu->b4_wd + 1) << 2) + ((ps_pu->b4_ht + 1) << 2);

                    ret |= ihevce_cabac_encode_inter_pred_idc(
                        ps_cabac, inter_pred_idc, cu_depth, pu_w_plus_pu_h);
                }
                else
                {
                    ASSERT(inter_pred_idc == 0);
                }

                /* Decode ref idx and mvd  for L0 (PRED_L0 or PRED_BI) */
                if(inter_pred_idc != PRED_L1)
                {
                    UWORD32 u4_bits_estimated_prev_mvd_ref_id;
                    /* encode L0 ref_idx  */
                    WORD32 ref_idx_l0 = ps_pu->mv.i1_l0_ref_idx;

                    /*PIC INFO : Populate Ref Indx L0 Bits*/
                    u4_bits_estimated_prev_mvd_ref_id = ps_cabac->u4_bits_estimated_q12;
                    ret |= ihevce_cabac_encode_refidx(ps_cabac, ref_idx_l0, ref_l0_active);

                    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                    {
                        // clang-format off
                        ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_ref_id +=
                            (ps_cabac->u4_bits_estimated_q12 -
                                u4_bits_estimated_prev_mvd_ref_id);
                        // clang-format on
                    }
                    /* Encode the mvd for L0 */
                    /*PIC INFO : Populate MVD Bits*/
                    u4_bits_estimated_prev_mvd_ref_id = ps_cabac->u4_bits_estimated_q12;

                    ret |= ihevce_cabac_encode_mvd(ps_cabac, &ps_pu->mv.s_l0_mv);

                    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                    {  // clang-format off
                        ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_mvd +=
                            (ps_cabac->u4_bits_estimated_q12 -
                                u4_bits_estimated_prev_mvd_ref_id);
                        // clang-format on
                    }

                    /* Encode the mvp_l0_flag */
                    ctxt_inc = IHEVC_CAB_MVP_L0L1;
                    ret |= ihevce_cabac_encode_bin(ps_cabac, ps_pu->b1_l0_mvp_idx, ctxt_inc);

                    AEV_TRACE("mvp_l0/l1_flag", ps_pu->b1_l0_mvp_idx, ps_cabac->u4_range);
                }

                /* Encode ref idx and MVD for L1  (PRED_L1 or PRED_BI) */
                if(inter_pred_idc != PRED_L0)
                {
                    /* encode L1 ref_idx  */
                    WORD32 ref_idx_l1 = ps_pu->mv.i1_l1_ref_idx;

                    UWORD32 u4_bits_estimated_prev_mvd_ref_id;
                    /*PIC INFO : Populate Ref Indx L1 Bits*/
                    u4_bits_estimated_prev_mvd_ref_id = ps_cabac->u4_bits_estimated_q12;

                    ret |= ihevce_cabac_encode_refidx(ps_cabac, ref_idx_l1, ref_l1_active);

                    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                    {  // clang-format off
                        ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_ref_id +=
                            (ps_cabac->u4_bits_estimated_q12 -
                                u4_bits_estimated_prev_mvd_ref_id);
                    }  // clang-format on

                    /* Check for zero mvd in case of bi_pred */
                    if(ps_slice_hdr->i1_mvd_l1_zero_flag && inter_pred_idc == PRED_BI)
                    {
                        ASSERT(ps_pu->mv.s_l1_mv.i2_mvx == 0);
                        ASSERT(ps_pu->mv.s_l1_mv.i2_mvy == 0);
                    }
                    else
                    {
                        /* Encode the mvd for L1 */
                        /*PIC INFO : Populate MVD Bits*/
                        u4_bits_estimated_prev_mvd_ref_id = ps_cabac->u4_bits_estimated_q12;

                        /* Encode the mvd for L1 */
                        ret |= ihevce_cabac_encode_mvd(ps_cabac, &ps_pu->mv.s_l1_mv);

                        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                        {
                            ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_mvd +=
                                (ps_cabac->u4_bits_estimated_q12 -
                                 u4_bits_estimated_prev_mvd_ref_id);
                        }
                    }

                    /* Encode the mvp_l1_flag */
                    ctxt_inc = IHEVC_CAB_MVP_L0L1;
                    ret |= ihevce_cabac_encode_bin(ps_cabac, ps_pu->b1_l1_mvp_idx, ctxt_inc);

                    AEV_TRACE("mvp_l0/l1_flag", ps_pu->b1_l1_mvp_idx, ps_cabac->u4_range);
                }
            }
        }
    }

    return ret;
}

/**
******************************************************************************
*
*  @brief Entropy encoding of coding unit (Coding Unit syntax)
*
*  @par   Description
*  Entropy encode of  coding unit (Coding Unit syntax) as per section:7.3.9.1
*  General Coding unit syntax
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]   ps_enc_cu
*  pointer to current cu whose entropy encode is done
*
*  @param[in]   cu_depth
*  depth of the the current cu in coding tree
*
*  @param[in]   top_avail
*  top availabilty flag for current cu (boolean)
*
*  @param[in]   left_avail
*  left availabilty flag for current cu (boolean)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_coding_unit(
    entropy_context_t *ps_entropy_ctxt,
    cu_enc_loop_out_t *ps_enc_cu,
    WORD32 cu_depth,
    WORD32 top_avail,
    WORD32 left_avail)
{
    WORD32 ret = IHEVCE_SUCCESS;
    sps_t *ps_sps = ps_entropy_ctxt->ps_sps;
    pps_t *ps_pps = ps_entropy_ctxt->ps_pps;
    slice_header_t *ps_slice_hdr = ps_entropy_ctxt->ps_slice_hdr;

    WORD32 skip_flag = 0;
    WORD32 no_res_flag = 0;

    /* CU top left co-ordinates w.r.t ctb */
    WORD32 cu_x0 = ps_enc_cu->b3_cu_pos_x << 3;
    WORD32 cu_y0 = ps_enc_cu->b3_cu_pos_y << 3;

    /* CU size in pels */
    WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;
    WORD32 log2_cb_size;

    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    UWORD32 u4_header_bits_temp = ps_cabac->u4_bits_estimated_q12;

    (void)cu_depth;
    (void)top_avail;
    (void)left_avail;
    /* Sanity checks */
    ASSERT((cu_x0 + cu_size) <= (1 << ps_entropy_ctxt->i1_log2_ctb_size));
    ASSERT((cu_y0 + cu_size) <= (1 << ps_entropy_ctxt->i1_log2_ctb_size));

    /* code tq bypass flag */
    ASSERT(ps_pps->i1_transquant_bypass_enable_flag == 0);

    /* log2_cb_size based on cu size */
    GETRANGE(log2_cb_size, cu_size);
    log2_cb_size -= 1;

    if(ps_pps->i1_transquant_bypass_enable_flag)
    {
        ihevce_cabac_encode_bin(
            ps_cabac, ps_enc_cu->b1_tq_bypass_flag, IHEVC_CAB_CU_TQ_BYPASS_FLAG);

        AEV_TRACE("cu_transquant_bypass_flag", ps_enc_cu->b1_tq_bypass_flag, ps_cabac->u4_range);
    }
    /* code the skip flag for inter slices */
    if(ps_slice_hdr->i1_slice_type != ISLICE)
    {
        skip_flag = ps_enc_cu->b1_skip_flag;

        ret |= ihevce_cabac_encode_skip_flag(ps_entropy_ctxt, ps_enc_cu, top_avail, left_avail);
    }
    /*PIC_INFO : Total CU in frame based on cu size */
    if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
    {
        // clang-format off
        if(cu_size == 64)
            ps_entropy_ctxt->ps_pic_level_info->i8_total_cu_based_on_size[3]++;
        else
            ps_entropy_ctxt->ps_pic_level_info->i8_total_cu_based_on_size[cu_size >> 4]++;
        // clang-format on
    }
    if(skip_flag)
    {
        /* encode merge idx for the skip cu */
        ret |= ihevce_cabac_encode_inter_pu(ps_entropy_ctxt, ps_enc_cu, cu_depth);

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {
            /*PIC INFO: Populated non-coded TUs in CU*/
            ps_entropy_ctxt->ps_pic_level_info->i8_total_non_coded_tu +=
                ps_enc_cu->u2_num_tus_in_cu;
            // clang-format off
            if(cu_size == 64)
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[3] +=
                    ps_enc_cu->u2_num_tus_in_cu;
            else if(cu_size == 32)
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[3] +=
                    ps_enc_cu->u2_num_tus_in_cu;
            else
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[cu_size >> 3] +=
                    ps_enc_cu->u2_num_tus_in_cu;
            // clang-format on

            /*PIC INFO: Populate cu header bits*/
            ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cu_hdr_bits +=
                (ps_cabac->u4_bits_estimated_q12 - u4_header_bits_temp);
        }
    }
    else
    {
        WORD32 pred_mode = PRED_MODE_INTRA;
        WORD32 part_mode = ps_enc_cu->b3_part_mode;
        WORD32 pcm_flag = ps_enc_cu->b1_pcm_flag;
        WORD32 is_mincu;
        WORD32 is_intra;

        is_mincu = (cu_size == (1 << ps_sps->i1_log2_min_coding_block_size));
        /* encode pred mode flag for inter slice */
        if(ps_slice_hdr->i1_slice_type != ISLICE)
        {
            pred_mode = ps_enc_cu->b1_pred_mode_flag;

            ret |= ihevce_cabac_encode_bin(ps_cabac, pred_mode, IHEVC_CAB_PRED_MODE);

            AEV_TRACE("pred_mode_flag", pred_mode, ps_cabac->u4_range);
        }
        is_intra = (PRED_MODE_INTRA == pred_mode);

        /* encode partition mode for inter pred or smallest intra pred cu */
        if((!is_intra) || is_mincu)
        {
            WORD32 amp_enabled = ps_sps->i1_amp_enabled_flag;
            WORD32 cusize_8 = (cu_size == 8);

            ret |= ihevce_cabac_encode_part_mode(
                ps_cabac, is_intra, is_mincu, amp_enabled, cusize_8, part_mode);
        }
        else
        {
            ASSERT(part_mode == SIZE_2Nx2N);
        }

        /* encode intra / inter pu modes of the current CU */
        if(is_intra)
        {
            /* NOTE: I_PCM not supported in encoder */
            ASSERT(0 == pcm_flag);
            ASSERT(0 == ps_sps->i1_pcm_enabled_flag);

            ret |= ihevce_cabac_encode_intra_pu(ps_entropy_ctxt, part_mode, ps_enc_cu);
        }
        else
        {
            ret |= ihevce_cabac_encode_inter_pu(ps_entropy_ctxt, ps_enc_cu, cu_depth);
        }
        /* encode no residue syntax flag and transform tree conditionally */
        if(!pcm_flag)
        {
            pu_t *ps_pu = &ps_enc_cu->ps_pu[0];
            WORD32 merge_cu;
            /* Encode residue syntax flag for inter cus not merged as 2Nx2N */
            if(!is_intra)
                merge_cu = (part_mode == PART_2Nx2N) && ps_pu->b1_merge_flag;

            if(!is_intra && !merge_cu)
            {
                no_res_flag = ps_enc_cu->b1_no_residual_syntax_flag;

#if 1 /* HACK FOR COMPLIANCE WITH HM REFERENCE DECODER */
                /*********************************************************/
                /* currently the HM decoder expects qtroot cbf instead of */
                /* no_residue_flag which has opposite meaning             */
                /* This will be fixed once the software / spec is fixed   */
                /*********************************************************/
                ret |= ihevce_cabac_encode_bin(ps_cabac, !no_res_flag, IHEVC_CAB_NORES_IDX);

                AEV_TRACE("no_residual_syntax_flag (HACKY)", !no_res_flag, ps_cabac->u4_range);
#else
                ret |= ihevce_cabac_encode_bin(ps_cabac, no_res_flag, IHEVC_CAB_NORES_IDX);

                AEV_TRACE("no_residual_syntax_flag", no_res_flag, ps_cabac->u4_range);
#endif
            }
            /*initialize header bits*/
            ps_cabac->u4_header_bits_estimated_q12 = ps_cabac->u4_bits_estimated_q12;

            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {  // clang-format off
                /*PIC INFO: Populate cu header bits*/
                ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_cu_hdr_bits +=
                    (ps_cabac->u4_bits_estimated_q12 - u4_header_bits_temp);
            }  // clang-format on

            ps_cabac->u4_true_tu_split_flag_q12 = 0;
            /* encode transform tree if no_residue_flag is 0 */
            if(!no_res_flag)
            {
                ps_entropy_ctxt->i4_tu_idx = 0;

                ret |= ihevce_encode_transform_tree(
                    ps_entropy_ctxt, cu_x0, cu_y0, log2_cb_size, 0, 0, ps_enc_cu);
            }
            else
            {
                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {
                    /*PIC INFO: Populated non-coded TUs in CU*/
                    ps_entropy_ctxt->ps_pic_level_info->i8_total_non_coded_tu +=
                        ps_enc_cu->u2_num_tus_in_cu;
                    // clang-format off
                    if(cu_size == 64)
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[3] +=
                            ps_enc_cu->u2_num_tus_in_cu;
                    else if(cu_size == 32)
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[3] +=
                            ps_enc_cu->u2_num_tus_in_cu;
                    else
                        ps_entropy_ctxt->ps_pic_level_info->i8_total_tu_based_on_size[cu_size >> 3] +=
                            ps_enc_cu->u2_num_tus_in_cu;
                    // clang-format on
                }
            }
            ps_cabac->u4_cbf_bits_q12 = ps_cabac->u4_bits_estimated_q12 -
                                        ps_cabac->u4_header_bits_estimated_q12 -
                                        ps_cabac->u4_true_tu_split_flag_q12;
        }
    }

    /*duplicate the qp values for 8x8 CU array to maintain neighbour qp*/
    if(CABAC_MODE_ENCODE_BITS == ps_entropy_ctxt->s_cabac_ctxt.e_cabac_op_mode)
    {
        WORD32 i, j;
        WORD32 cur_cu_offset, cur_qp, qp_left, qp_top;
        WORD32 is_last_blk_in_qg;
        /* CU x co-ordinate w.r.t frame start           */
        WORD32 ctb_x0_frm = (ps_entropy_ctxt->i4_ctb_x << ps_entropy_ctxt->i1_log2_ctb_size);

        WORD32 cu_x0_frm = cu_x0 + ctb_x0_frm;

        /* CU y co-ordinate w.r.t frame start           */
        WORD32 ctb_y0_frm = (ps_entropy_ctxt->i4_ctb_y << ps_entropy_ctxt->i1_log2_ctb_size);

        WORD32 cu_y0_frm = cu_y0 + ctb_y0_frm;

        WORD32 pic_width = ps_sps->i2_pic_width_in_luma_samples;
        WORD32 pic_height = ps_sps->i2_pic_height_in_luma_samples;

        /* Added code for handling the QP neighbour population depending
            on the diff_cu_qp_delta_depth: Lokesh  */
        /* is_last_blk_in_qg variables is to find if the coding block is the last CU in the Quantization group
            3 - i1_diff_cu_qp_delta_depth is done as the cu_pos_x and cu_pos_y are in terms of 8x8 positions in the CTB: Lokesh*/
        WORD32 log2_min_cu_qp_delta_size =
            ps_entropy_ctxt->i1_log2_ctb_size - ps_entropy_ctxt->ps_pps->i1_diff_cu_qp_delta_depth;
        UWORD32 min_cu_qp_delta_size = 1 << log2_min_cu_qp_delta_size;

        WORD32 block_addr_align = 15 << (log2_min_cu_qp_delta_size - 3);

        ps_entropy_ctxt->i4_qg_pos_x = ps_enc_cu->b3_cu_pos_x & block_addr_align;
        ps_entropy_ctxt->i4_qg_pos_y = ps_enc_cu->b3_cu_pos_y & block_addr_align;

        /* Condition for detecting last cu in a qp group.                                       */
        /* Case 1: Current cu position + size exceed or meets the next qp group start location  */
        /* Case 2: Current cu position + size hits the incomplete ctb boundary in atleast one   */
        /*         direction and the qp grp limit in other direction                            */

        /* case 1 */
        is_last_blk_in_qg =
            ((cu_x0 + cu_size) >=
                 ((ps_entropy_ctxt->i4_qg_pos_x << 3) + (WORD32)min_cu_qp_delta_size) &&
             (cu_y0 + cu_size) >=
                 ((ps_entropy_ctxt->i4_qg_pos_y << 3) + (WORD32)min_cu_qp_delta_size));

        /* case 2 : x direction incomplete ctb */
        if((cu_x0_frm + cu_size) >= pic_width)
        {
            is_last_blk_in_qg |=
                ((cu_y0 + cu_size) >=
                 ((ps_entropy_ctxt->i4_qg_pos_y << 3) + (WORD32)min_cu_qp_delta_size));
        }

        /* case 2 : y direction incomplete ctb */
        if((cu_y0_frm + cu_size) >= pic_height)
        {
            is_last_blk_in_qg |=
                ((cu_x0 + cu_size) >=
                 ((ps_entropy_ctxt->i4_qg_pos_x << 3) + (WORD32)min_cu_qp_delta_size));
        }

        cur_cu_offset = ps_enc_cu->b3_cu_pos_x + (ps_enc_cu->b3_cu_pos_y * 8);

        if((ps_entropy_ctxt->i4_is_cu_cbf_zero || no_res_flag || skip_flag) &&
           ((ps_entropy_ctxt->i1_encode_qp_delta)))
        {
            {  // clang-format off
                /*it should remember average of qp_top and qp_left*/
                if(ps_entropy_ctxt->i4_qg_pos_x > 0)
                {
                    qp_left =
                        ps_entropy_ctxt->ai4_8x8_cu_qp[(ps_entropy_ctxt->i4_qg_pos_x - 1) +
                                            (ps_entropy_ctxt->i4_qg_pos_y * 8)];
                }
                if(ps_entropy_ctxt->i4_qg_pos_y > 0)
                {
                    qp_top =
                        ps_entropy_ctxt->ai4_8x8_cu_qp[ps_entropy_ctxt->i4_qg_pos_x +
                                            (ps_entropy_ctxt->i4_qg_pos_y - 1) *
                                                8];
                }  // clang-format on
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
                cur_qp = (qp_top + qp_left + 1) >> 1;
                /*In case of skip or zero cbf CU the previous qp used has to be updated*/
                if(is_last_blk_in_qg)
                    ps_entropy_ctxt->i1_cur_qp = cur_qp;
            }
        }
        else
        {
            cur_qp = (WORD32)ps_enc_cu->ps_enc_tu->s_tu.b7_qp;
        }

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {
            WORD32 temp = 0;
            /*PIC_INFO: Accumalate average qp, min qp and max qp*/
            ps_entropy_ctxt->ps_pic_level_info->i8_total_qp += cur_qp;
            if(cu_size == 64)
                temp = 6;
            else if(cu_size == 32)
                temp = 4;
            else if(cu_size == 16)
                temp = 2;
            else if(cu_size == 8)
                temp = 0;

            ps_entropy_ctxt->ps_pic_level_info->i8_total_qp_min_cu += (cur_qp * (1 << temp));
            if(cur_qp < ps_entropy_ctxt->ps_pic_level_info->i4_min_qp)
                ps_entropy_ctxt->ps_pic_level_info->i4_min_qp = cur_qp;
            if(cur_qp > ps_entropy_ctxt->ps_pic_level_info->i4_max_qp)
                ps_entropy_ctxt->ps_pic_level_info->i4_max_qp = cur_qp;
        }

        for(i = 0; i < (WORD32)ps_enc_cu->b4_cu_size; i++)
        {
            for(j = 0; j < (WORD32)ps_enc_cu->b4_cu_size; j++)
            {
                ps_entropy_ctxt->ai4_8x8_cu_qp[cur_cu_offset + (i * 8) + j] = cur_qp;
            }
        }
        ps_entropy_ctxt->i4_is_cu_cbf_zero = 1;
    }

    return ret;
}

/**
******************************************************************************
*
*  @brief Entropy encoding of SAO related syntax elements as per sec 7.3.8.3
*
*  @par   Description
*  Encoding of sao related syntax elements at ctb level.
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]   ps_ctb_enc_loop_out
*  pointer to ctb level output structure from enc loop
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_cabac_encode_sao(
    entropy_context_t *ps_entropy_ctxt, ctb_enc_loop_out_t *ps_ctb_enc_loop_out)
{
    WORD32 error = IHEVCE_SUCCESS;
    sao_enc_t *ps_sao;
    nbr_avail_flags_t *ps_ctb_nbr_avail_flags;
    slice_header_t *ps_slice_hdr = ps_entropy_ctxt->ps_slice_hdr;
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    UWORD8 u1_left_avail, u1_top_avail;

    ps_ctb_nbr_avail_flags = &ps_ctb_enc_loop_out->s_ctb_nbr_avail_flags;

    ps_sao = &ps_ctb_enc_loop_out->s_sao;

    ASSERT(ps_sao->b1_sao_merge_left_flag < 2);

    u1_left_avail = ps_ctb_nbr_avail_flags->u1_left_avail;
    u1_top_avail = ps_ctb_nbr_avail_flags->u1_top_avail;

    if(u1_left_avail == 1)
    {
        /*Encode the sao_merge_left_flag as FL as per table 9-32*/
        error |=
            ihevce_cabac_encode_bin(ps_cabac, ps_sao->b1_sao_merge_left_flag, IHEVC_CAB_SAO_MERGE);

        AEV_TRACE("sao_merge_flag", ps_sao->b1_sao_merge_left_flag, ps_cabac->u4_range);
    }

    if((u1_top_avail == 1) && (!ps_sao->b1_sao_merge_left_flag))
    {
        /*Encode the sao_merge_up_flag as FL as per table 9-32*/
        error |=
            ihevce_cabac_encode_bin(ps_cabac, ps_sao->b1_sao_merge_up_flag, IHEVC_CAB_SAO_MERGE);

        AEV_TRACE("sao_merge_flag", ps_sao->b1_sao_merge_up_flag, ps_cabac->u4_range);
    }

    if((!ps_sao->b1_sao_merge_left_flag) && (!ps_sao->b1_sao_merge_up_flag))
    {
        WORD32 c_idx;
        WORD32 sao_type_idx = ps_sao->b3_y_type_idx;

        /*Run a loop for y,cb and cr to encode the type idx for luma and chroma*/
        for(c_idx = 0; c_idx < 3; c_idx++)
        {
            if((ps_slice_hdr->i1_slice_sao_luma_flag && c_idx == 0) ||
               (ps_slice_hdr->i1_slice_sao_chroma_flag && c_idx > 0))
            {
                WORD32 ctxt_bin;

                /**************************************************************************/
                /* encode the sao_type_idx as per Table 9-33                              */
                /* First bin is context model based prefix : 1 if sao_type_idx > 0 else 0 */
                /* Second bin is coded as bypass bin if sao_type_ide > 0                  */
                /**************************************************************************/

                if(c_idx < 2)
                {
                    WORD32 sao_type_idx_temp;

                    ASSERT(ps_sao->b3_cb_type_idx == ps_sao->b3_cr_type_idx);

                    sao_type_idx = c_idx ? ps_sao->b3_cb_type_idx : ps_sao->b3_y_type_idx;

                    ctxt_bin = sao_type_idx ? 1 : 0;

                    if(sao_type_idx > 1)
                    {
                        sao_type_idx_temp = 2;
                    }
                    else
                    {
                        sao_type_idx_temp = sao_type_idx;
                    }

                    ASSERT(sao_type_idx_temp < 3);

                    /*Encode the first bin as context bin as per table 9-37*/
                    error |= ihevce_cabac_encode_bin(ps_cabac, ctxt_bin, IHEVC_CAB_SAO_TYPE);

                    if(sao_type_idx_temp)
                    {
                        /*Binarisation for sao_type_idx is TR(truncated rice) process as per
                            * table 9-32 with cMax=2 and cRiceParam=0
                            */

                        /* Encode the second bin as bypass bin as per below table*/
                        /*
                            |Symbol | Prefix |Prefix length |Prefix bins|
                            |   0   |    0   |     1        |     0     |
                            |   1   |    1   |     2        |     10    |
                            |   2   |    2   |     2        |     11    |

                            Since cRiceParam=0, there is no suffix code
                            */

                        error |= ihevce_cabac_encode_bypass_bin(ps_cabac, sao_type_idx_temp - 1);
                    }
                    AEV_TRACE("sao_type_idx", sao_type_idx_temp, ps_cabac->u4_range);
                }

                if(sao_type_idx != 0)
                {
                    WORD32 i;
                    UWORD8 u1_bit_depth = ps_entropy_ctxt->ps_sps->i1_bit_depth_luma_minus8 + 8;
                    WORD8 *sao_offset;
                    WORD32 sao_band_position;
                    WORD32 c_max = (1 << (MIN(u1_bit_depth, 10) - 5)) -
                                   1;  //( 1 << (MIN(BIT_DEPTH, 10) - 5)) - 1;

                    if(c_idx == 0)
                    {
                        //sao_offset[0] = ps_sao->b4_y_offset_1;
                        //sao_offset[1] = ps_sao->b4_y_offset_2;
                        //sao_offset[2] = ps_sao->b4_y_offset_3;
                        //sao_offset[3] = ps_sao->b4_y_offset_4;
                        sao_offset = &ps_sao->u1_y_offset[1];
                        sao_band_position = ps_sao->b5_y_band_pos;
                    }
                    else if(c_idx == 1)
                    {
                        //sao_offset[0] = ps_sao->b4_cb_offset_1;
                        //sao_offset[1] = ps_sao->b4_cb_offset_2;
                        //sao_offset[2] = ps_sao->b4_cb_offset_3;
                        //sao_offset[3] = ps_sao->b4_cb_offset_4;
                        sao_offset = &ps_sao->u1_cb_offset[1];
                        sao_band_position = ps_sao->b5_cb_band_pos;
                    }
                    else
                    {
                        //sao_offset[0] = ps_sao->b4_cr_offset_1;
                        //sao_offset[1] = ps_sao->b4_cr_offset_2;
                        //sao_offset[2] = ps_sao->b4_cr_offset_3;
                        //sao_offset[3] = ps_sao->b4_cr_offset_4;
                        sao_offset = &ps_sao->u1_cr_offset[1];
                        sao_band_position = ps_sao->b5_cr_band_pos;
                    }

                    for(i = 0; i < 4; i++)
                    {
                        /*Encode the sao offset value as tunary bypass*/
                        error |=
                            ihevce_cabac_encode_tunary_bypass(ps_cabac, abs(sao_offset[i]), c_max);

                        AEV_TRACE("sao_offset_abs", abs(sao_offset[i]), ps_cabac->u4_range);
                    }

                    /*Band offset case*/
                    if(sao_type_idx == 1)
                    {
                        for(i = 0; i < 4; i++)
                        {
                            if(sao_offset[i] != 0)
                            {
                                /*Encode the sao offset sign as FL as per table 9-32*/
                                error |= ihevce_cabac_encode_bypass_bin(
                                    ps_cabac,
                                    (abs(sao_offset[i]) + sao_offset[i] == 0));  //,
                                //IHEVC_CAB_SAO_MERGE
                                //);

                                AEV_TRACE(
                                    "sao_offset_sign",
                                    (abs(sao_offset[i]) + sao_offset[i] == 0),
                                    ps_cabac->u4_range);
                            }
                        }

                        /*Encode the sao band position as FL as per table 9-32*/
                        error |= ihevce_cabac_encode_bypass_bins(ps_cabac, sao_band_position, 5);
                        AEV_TRACE("sao_band_position", sao_band_position, ps_cabac->u4_range);
                    }
                    else
                    {
                        /*Encode the sao edge offset class for luma and chroma as FL as per table 9-32*/
                        if(c_idx == 0)
                        {
                            error |= ihevce_cabac_encode_bypass_bins(
                                ps_cabac, (ps_sao->b3_y_type_idx - 2), 2);
                            AEV_TRACE(
                                "sao_eo_class", (ps_sao->b3_y_type_idx - 2), ps_cabac->u4_range);
                        }

                        if(c_idx == 1)
                        {
                            ASSERT(ps_sao->b3_cb_type_idx == ps_sao->b3_cr_type_idx);
                            error |= ihevce_cabac_encode_bypass_bins(
                                ps_cabac, (ps_sao->b3_cb_type_idx - 2), 2);
                            AEV_TRACE(
                                "sao_eo_class", (ps_sao->b3_cb_type_idx - 2), ps_cabac->u4_range);
                        }
                    }
                }
            }
        }
    }

    return (error);
}

/**
******************************************************************************
*
*  @brief Encodes a coding quad tree (QuadTree syntax) as per section 7.3.8
*
*  @par   Description
*  Entropy encode of coding quad tree based on cu split flags of ctb as per
*  section:7.3.8
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @param[in]      x0_frm
*  x co-ordinate of current cu node of coding tree
*
*  @param[in]      y0_frm
*  y co-ordinate of current cu node of coding tree
*
*  @param[in]      log2_cb_size
*  current cu node block size
*
*  @param[in]      ct_depth
*  depth of current cu node w.r.t ctb
*
*  @param[in]      ps_ctb
*  pointer to current ctb structure
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_encode_coding_quadtree(
    entropy_context_t *ps_entropy_ctxt,
    WORD32 x0_frm,
    WORD32 y0_frm,
    WORD32 log2_cb_size,
    WORD32 ct_depth,
    ctb_enc_loop_out_t *ps_ctb,
    ihevce_tile_params_t *ps_tile_params)
{
    WORD32 ret = IHEVCE_SUCCESS;
    sps_t *ps_sps = ps_entropy_ctxt->ps_sps;
    pps_t *ps_pps = ps_entropy_ctxt->ps_pps;
    WORD32 split_cu_flag;
    WORD32 cu_idx = ps_entropy_ctxt->i4_cu_idx;
    cu_enc_loop_out_t *ps_enc_cu = ps_ctb->ps_enc_cu + cu_idx;

    /* CU size in pels */
    WORD32 cu_size = ps_enc_cu->b4_cu_size << 3;

    WORD32 pic_width = ps_tile_params->i4_curr_tile_width;
    WORD32 pic_height = ps_tile_params->i4_curr_tile_height;

    WORD32 log2_min_cb_size = ps_sps->i1_log2_min_coding_block_size;
    WORD32 ctb_size = (1 << (log2_cb_size + ct_depth));
    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    /* top row cu depth stored for frm_width (1byte per mincusize=8) */
    UWORD8 *pu1_cu_depth_top = ps_entropy_ctxt->pu1_cu_depth_top;

    /* left cu depth stored for one ctb column (1byte per mincusize=8) */
    UWORD8 *pu1_cu_depth_left = &ps_entropy_ctxt->au1_cu_depth_left[0];

    /* calculation of top and left nbr availability */
    WORD32 top_avail;
    WORD32 left_avail;

    /* top and left cu within ctb  or outside ctb boundary */
    left_avail = (x0_frm & (ctb_size - 1)) ? 1 : ps_ctb->s_ctb_nbr_avail_flags.u1_left_avail;
    top_avail = (y0_frm & (ctb_size - 1)) ? 1 : ps_ctb->s_ctb_nbr_avail_flags.u1_top_avail;

    /* Sanity checks */
    ASSERT(ct_depth <= 3);
    ASSERT((cu_idx >= 0) && (cu_idx < ps_ctb->u1_num_cus_in_ctb));
    ASSERT(cu_size >= (1 << log2_min_cb_size));
    ASSERT(((ps_enc_cu->b3_cu_pos_x << 3) + cu_size) <= (UWORD32)ctb_size);
    ASSERT(((ps_enc_cu->b3_cu_pos_y << 3) + cu_size) <= (UWORD32)ctb_size);

    /* Encode cu split flags based on following conditions; See section 7.3.8*/
    if(((x0_frm + (1 << log2_cb_size)) <= pic_width) &&
       ((y0_frm + (1 << log2_cb_size)) <= pic_height) && (log2_cb_size > log2_min_cb_size) &&
       (ps_entropy_ctxt->i1_ctb_num_pcm_blks == 0))
    {
        /* encode the split cu flag */
        WORD32 ctxt_inc = IHEVC_CAB_SPLIT_CU_FLAG;
        UWORD32 u4_bits_estimated_prev;
        /* Context increment for skip flag as per Table 9-38 */
        if(top_avail)
        {
            ctxt_inc += (pu1_cu_depth_top[x0_frm >> 3] > ct_depth);
        }

        if(left_avail)
        {
            ctxt_inc += (pu1_cu_depth_left[(y0_frm >> 3) & 0x7] > ct_depth);
        }

        /* split if actual cu size is smaller than target cu size */
        split_cu_flag = cu_size < (1 << log2_cb_size);
        u4_bits_estimated_prev = ps_cabac->u4_bits_estimated_q12;
        ret |= ihevce_cabac_encode_bin(ps_cabac, split_cu_flag, ctxt_inc);

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {  // clang-format off
            /*PIC INFO : populate cu split flag*/
            ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_split_cu_flag +=
                (ps_cabac->u4_bits_estimated_q12 - u4_bits_estimated_prev);
        }  // clang-format on

        AEV_TRACE("split_cu_flag", split_cu_flag, ps_cabac->u4_range);
        if(split_cu_flag == 0)
        {
            AEV_TRACE("split_cu_flag : X0", (x0_frm >> 6) << 6, ps_cabac->u4_range);
            AEV_TRACE("split_cu_flag : Y0", (y0_frm >> 6) << 6, ps_cabac->u4_range);
        }
    }
    else
    {
        /*********************************************************************/
        /* split cu is implicitly derived as 1 in frame/slice boundary case  */
        /* else split cu is implicitly derived as 0 if mincu size is reached */
        /*********************************************************************/
        if(log2_cb_size > ps_sps->i1_log2_min_coding_block_size)
            split_cu_flag = 1;
        else
            split_cu_flag = 0;
    }

    /************************************************************************/
    /* Reset qp delata coded flag appropriately so as to signal qp rightly  */
    /* during transform coding                                              */
    /************************************************************************/
    if((ps_pps->i1_cu_qp_delta_enabled_flag) && (ct_depth <= (ps_pps->i1_diff_cu_qp_delta_depth)))

    {
        ps_entropy_ctxt->i1_encode_qp_delta = 1;
    }
    /*else
    {
        ps_entropy_ctxt->i1_encode_qp_delta = 0;
    }*/

    if(split_cu_flag)
    {
        /* recurse quad tree till a leaf node is reached */
        WORD32 x1_frm = x0_frm + ((1 << log2_cb_size) >> 1);
        WORD32 y1_frm = y0_frm + ((1 << log2_cb_size) >> 1);

        /* node0 of quad tree */
        ret |= ihevce_encode_coding_quadtree(
            ps_entropy_ctxt, x0_frm, y0_frm, log2_cb_size - 1, ct_depth + 1, ps_ctb, ps_tile_params);

        if(x1_frm < pic_width)
        { /* node1 of quad tree */
            ret |= ihevce_encode_coding_quadtree(
                ps_entropy_ctxt,
                x1_frm,
                y0_frm,
                log2_cb_size - 1,
                ct_depth + 1,
                ps_ctb,
                ps_tile_params);
        }

        if(y1_frm < pic_height)
        {
            /* node2 of quad tree */
            ret |= ihevce_encode_coding_quadtree(
                ps_entropy_ctxt,
                x0_frm,
                y1_frm,
                log2_cb_size - 1,
                ct_depth + 1,
                ps_ctb,
                ps_tile_params);
        }

        if((x1_frm < pic_width) && (y1_frm < pic_height))
        {
            /* node3 of quad tree */
            ret |= ihevce_encode_coding_quadtree(
                ps_entropy_ctxt,
                x1_frm,
                y1_frm,
                log2_cb_size - 1,
                ct_depth + 1,
                ps_ctb,
                ps_tile_params);
        }
    }
    else
    {
        /* leaf node is reached! Encode the CU */
        WORD32 i;

        /* sanity checks */
        ASSERT(ps_entropy_ctxt->i1_ctb_num_pcm_blks == 0);

        if(ps_entropy_ctxt->i1_ctb_num_pcm_blks == 0)
        {
            UWORD32 u4_bits_eztimated = ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12;
            /* Encode a non-PCM CU */
            /*PCM INFO: populate total TUs*/
            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {
                ps_entropy_ctxt->ps_pic_level_info->i8_total_tu += ps_enc_cu->u2_num_tus_in_cu;
            }

            ret |= ihevce_cabac_encode_coding_unit(
                ps_entropy_ctxt, ps_enc_cu, ct_depth, top_avail, left_avail);

            if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
            {
                // clang-format off
                if(PRED_MODE_INTRA == ps_enc_cu->b1_pred_mode_flag)
                {
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_intra +=
                        (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                            u4_bits_eztimated);
                }
                else
                {
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_inter +=
                        (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                            u4_bits_eztimated);
                }
                // clang-format on
            }
        }
        else
        {  //TODO: //PCM not supported in this encoder
        }

        /* update cu_idx, left and top arrays for cudepth  after encoding cu */
        ps_entropy_ctxt->i4_cu_idx++;
        for(i = 0; i < (cu_size >> 3); i++)
        {
            pu1_cu_depth_top[(x0_frm >> 3) + i] = ct_depth;
            pu1_cu_depth_left[((y0_frm >> 3) & 0x7) + i] = ct_depth;
        }
    }

    return ret;
}

/**
******************************************************************************
*
*  @brief Encodes slice data (General Slice syntax) as per section 7.3.6.1
*
*  @par   Description
*  Entropy encode of all ctbs in a slice as per section 7.3.6.1
*
*  @param[inout]   ps_entropy_ctxt
*  pointer to entropy context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_encode_slice_data(
    entropy_context_t *ps_entropy_ctxt,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 *pi4_end_of_slice_flag)
{
    WORD32 ret = IHEVCE_SUCCESS;
    WORD32 end_of_slice_seg_flag = 0;
    sps_t *ps_sps = ps_entropy_ctxt->ps_sps;
    pps_t *ps_pps = ps_entropy_ctxt->ps_pps;
    slice_header_t *ps_slice_hdr = ps_entropy_ctxt->ps_slice_hdr;

    cab_ctxt_t *ps_cabac = &ps_entropy_ctxt->s_cabac_ctxt;

    /* State of previous CTB before it's terminate bin is encoded */
    cab_ctxt_t s_cabac_prev_ctb;

    /* State after current CTB's encoding is complete but before
    the termintate bin encoding */
    cab_ctxt_t s_cabac_after_ctb;

    /* Storing the last 4 bytes before adding terminate bin
    as these 4 bytes might get corrupted while encoding terminate bin */
    UWORD32 u4_prev_ctb_temp, u4_cur_ctb_temp;
    WORD8 i1_last_cu_qp = 0;
    bitstrm_t *ps_bit_strm = &ps_entropy_ctxt->s_bit_strm;

    WORD32 log2_ctb_size, ctb_size;
    //WORD32 pic_width  = ps_sps->i2_pic_width_in_luma_samples;
    //WORD32 pic_height = ps_sps->i2_pic_height_in_luma_samples;
    WORD32 pic_width = ps_tile_params->i4_curr_tile_width;
    WORD32 pic_height = ps_tile_params->i4_curr_tile_height;
    WORD32 num_ctb_in_row;

    WORD32 i4_curr_ctb_x, i4_curr_ctb_y;
    UWORD32 u4_slice_seg_hdr_size = (UWORD32)ps_entropy_ctxt->i4_slice_seg_len;
    UWORD32 u4_slice_start_offset = ps_bit_strm->u4_strm_buf_offset - u4_slice_seg_hdr_size;

    WORD32 ctb_slice_address = ps_slice_hdr->i2_slice_address;
    WORD32 slice_qp = ps_slice_hdr->i1_slice_qp_delta + ps_pps->i1_pic_init_qp;
    WORD32 cabac_init_idc;
    WORD32 x0_frm, y0_frm;
    ctb_enc_loop_out_t *ps_first_ctb;  // Points to first CTB of ctb-row
    ctb_enc_loop_out_t *ps_ctb;
    WORD32 ctb_ctr = 0;  //count ctb encoded in a ctb-row

    ihevce_sys_api_t *ps_sys_api = (ihevce_sys_api_t *)ps_entropy_ctxt->pv_sys_api;

    /* Structure to backup pic info in case we need to revert back to pervious
    CTB when i4_slice_segment_mode is 2 */
    s_pic_level_acc_info_t s_pic_level_info_backup;  // info before

    /* Initialize the CTB size from sps parameters */
    log2_ctb_size =
        ps_sps->i1_log2_min_coding_block_size + ps_sps->i1_log2_diff_max_min_coding_block_size;

    ctb_size = (1 << log2_ctb_size);

    /* sanity checks */
    ASSERT((log2_ctb_size >= 3) && (log2_ctb_size <= 6));

    ps_entropy_ctxt->i1_log2_ctb_size = (WORD8)log2_ctb_size;

    /* Initiallise before starting slice. For single slice case both
    x and y will be set to zero */
    ps_entropy_ctxt->i4_ctb_x = ps_entropy_ctxt->i4_next_slice_seg_x;
    ps_entropy_ctxt->i4_ctb_y = ps_entropy_ctxt->i4_next_slice_seg_y;
    num_ctb_in_row = (ps_sps->i2_pic_width_in_luma_samples + ctb_size - 1) >> log2_ctb_size;

    /* initialize the cabac init idc based on slice type */
    if(ps_slice_hdr->i1_slice_type == ISLICE)
    {
        cabac_init_idc = 0;
    }
    else if(ps_slice_hdr->i1_slice_type == PSLICE)
    {
        cabac_init_idc = ps_slice_hdr->i1_cabac_init_flag ? 2 : 1;
    }
    else
    {
        cabac_init_idc = ps_slice_hdr->i1_cabac_init_flag ? 1 : 2;
    }
    ps_cabac->i1_entropy_coding_sync_enabled_flag = ps_pps->i1_entropy_coding_sync_enabled_flag;

    /* Dependent slices should be ON only when slice segment mode is enabled */
    if(ps_slice_hdr->i1_dependent_slice_flag == 1)
    {
        ASSERT(
            (ps_entropy_ctxt->i4_slice_segment_mode == 1) ||
            (ps_entropy_ctxt->i4_slice_segment_mode == 2));
    }

    /* initialize the cabac engine. For dependent slice segments
    cabac context models will not be reset */
    if(ps_slice_hdr->i1_dependent_slice_flag == 1)
    {
        ret = ihevce_cabac_reset(ps_cabac, ps_bit_strm, CABAC_MODE_ENCODE_BITS);
    }
    else
    {
        ret = ihevce_cabac_init(
            ps_cabac,
            ps_bit_strm,
            CLIP3(slice_qp, 0, IHEVC_MAX_QP),
            cabac_init_idc,
            CABAC_MODE_ENCODE_BITS);

        /* initialize qp to slice start qp */
        ps_entropy_ctxt->i1_cur_qp = slice_qp;
    }

    /* initialize slice x and y offset in pels w.r.t top left conrner */
    x0_frm = ps_entropy_ctxt->i4_ctb_x << log2_ctb_size;
    y0_frm = ps_entropy_ctxt->i4_ctb_y << log2_ctb_size;

    /* Pointing ctb structure to the correct CTB in frame based on
    slice address */
    ps_first_ctb = ps_entropy_ctxt->ps_frm_ctb + ctb_slice_address;
    ps_ctb = ps_first_ctb - 1;

    //ps_entropy_ctxt->i4_ctb_slice_x = 0;
    //ps_entropy_ctxt->i4_ctb_slice_y = 0;

    /* Setting to NULL to detect if first CTB of slice itself
    exceeds the i4_slice_segment_max_length. Will be used only if
    i4_slice_segment_mode is non-zero */
    s_cabac_prev_ctb.pu1_strm_buffer = NULL;

    do
    {
        UWORD8 au1_cu_depth_top[8] = { 0 }, au1_cu_depth_left[8] = { 0 };
        UWORD8 u1_skip_cu_top = 0;
        UWORD32 u4_skip_cu_left = 0;

        /* By default assume that slice-segment is going to end after
        current CTB */
        end_of_slice_seg_flag = 1;

        i4_curr_ctb_x = ps_entropy_ctxt->i4_ctb_x;
        i4_curr_ctb_y = ps_entropy_ctxt->i4_ctb_y;

        if(1 == ps_tile_params->i4_tiles_enabled_flag)
        {
            ps_ctb = ps_first_ctb + ctb_ctr;
        }
        else
        {
            ps_ctb++;
        }

        /* Store some parameters. Will be used if current CTB's encoding
        has to be reverted in the event of overflow beyond i4_slice_segment_max_length */
        if(2 == ps_entropy_ctxt->i4_slice_segment_mode)
        {
            /* Store CU depths flag */
            memcpy(au1_cu_depth_top, &ps_entropy_ctxt->pu1_cu_depth_top[i4_curr_ctb_x * 8], 8);
            memcpy(au1_cu_depth_left, ps_entropy_ctxt->au1_cu_depth_left, 8);

            /* Store CU skip flags */
            u1_skip_cu_top = *(ps_entropy_ctxt->pu1_skip_cu_top + i4_curr_ctb_x);
            u4_skip_cu_left = ps_entropy_ctxt->u4_skip_cu_left;

            /* Backup current state of pic info */
            s_pic_level_info_backup = *(ps_entropy_ctxt->ps_pic_level_info);
        }

        /* Section:7.3.7 Coding tree unit syntax */
        /* coding_tree_unit() inlined here */
        ps_entropy_ctxt->i1_ctb_num_pcm_blks = 0;

        /* Simple Neigbour avail calculation */
        ps_ctb->s_ctb_nbr_avail_flags.u1_left_avail = (x0_frm > 0);
        ps_ctb->s_ctb_nbr_avail_flags.u1_top_avail = (y0_frm > 0);

        ps_entropy_ctxt->i4_cu_idx = 0;

        /* Encode SAO syntax as per section 7.3.8.3 */
        if(ps_sps->i1_sample_adaptive_offset_enabled_flag)
        {
            if((ps_slice_hdr->i1_slice_sao_luma_flag) || (ps_slice_hdr->i1_slice_sao_chroma_flag))
            {
                /*PIC INFO: SAO encode biys*/
                UWORD32 u4_bits_estimated_prev =
                    ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12;

                ret |= ihevce_cabac_encode_sao(ps_entropy_ctxt, ps_ctb);

                if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
                {
                    ps_entropy_ctxt->ps_pic_level_info->u8_bits_estimated_sao +=
                        (ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 -
                         u4_bits_estimated_prev);
                }
            }
        }

        ps_entropy_ctxt->s_cabac_ctxt.u4_bits_estimated_q12 = 0;

        if(ps_cabac->e_cabac_op_mode == CABAC_MODE_ENCODE_BITS)
        {
            /*PIC_INFO: Update total no.of CUS*/
            ps_entropy_ctxt->ps_pic_level_info->i8_total_cu += ps_ctb->u1_num_cus_in_ctb;
        }
        /* call recursive coding tree structure to encode all cus in ctb */
        ret |= ihevce_encode_coding_quadtree(
            ps_entropy_ctxt, x0_frm, y0_frm, log2_ctb_size, 0, ps_ctb, ps_tile_params);

        /* post ctb encode increments */
        ctb_ctr++;
        x0_frm += ctb_size;
        ps_entropy_ctxt->i4_ctb_x++;
        //ps_entropy_ctxt->i4_ctb_slice_x++;

        if(ps_pps->i1_entropy_coding_sync_enabled_flag && ps_entropy_ctxt->i4_ctb_x == 2)
        {
            /*backup cabac context at end of second CTB(top right neighbour for start of bottom row)*/
            ihevce_cabac_ctxt_backup(ps_cabac);
        }

        /* end of row check using x0_frm offset */
        if(x0_frm >= pic_width)
        {
            ctb_ctr = 0;
            ps_first_ctb += num_ctb_in_row;
            x0_frm = 0;
            y0_frm += ctb_size;

            ps_entropy_ctxt->i4_ctb_x = 0;
            ps_entropy_ctxt->i4_ctb_y++;
            //ps_entropy_ctxt->i4_ctb_slice_y++;
        }

        /* Detect end of slice. Which would mean end-of-slice-segment too */
        *pi4_end_of_slice_flag = (y0_frm >= pic_height);

        if(0 == ps_entropy_ctxt->i4_slice_segment_mode)
        {
            /* If slice ends then so does slice segment  */
            end_of_slice_seg_flag = *pi4_end_of_slice_flag;

            /*  encode terminate bin */
            ret |= ihevce_cabac_encode_terminate(ps_cabac, end_of_slice_seg_flag, 0);
        }
        else if(1 == ps_entropy_ctxt->i4_slice_segment_mode)
        {
            ps_entropy_ctxt->i4_slice_seg_len++;
            if((ps_entropy_ctxt->i4_slice_seg_len) >= ps_entropy_ctxt->i4_slice_segment_max_length)
            {
                /* Store the address of CTB from where next slice segment will start */
                ps_entropy_ctxt->i4_next_slice_seg_x = ps_entropy_ctxt->i4_ctb_x;
                ps_entropy_ctxt->i4_next_slice_seg_y = ps_entropy_ctxt->i4_ctb_y;
            }
            else
            {
                /* If slice ends then so does slice segment  */
                end_of_slice_seg_flag = *pi4_end_of_slice_flag;
            }

            /*  encode terminate bin */
            ret |= ihevce_cabac_encode_terminate(ps_cabac, end_of_slice_seg_flag, 0);
        }
        else if(2 == ps_entropy_ctxt->i4_slice_segment_mode)
        {
            //WORD32 i4_slice_seg_len_prev = i4_slice_seg_len;

            /* Store some parameters. Will be used to revert back to this state if
            i4_slice_segment_max_length is not exceeded after encoding end-of-slice */
            s_cabac_after_ctb = *ps_cabac;
            u4_cur_ctb_temp =
                *((UWORD32 *)(ps_cabac->pu1_strm_buffer + ps_cabac->u4_strm_buf_offset - 4));

            /*  encode terminate bin. For dependent slices, always simulate
            end-of-slice to check if i4_slice_segment_max_length is surpassed */
            ret |= ihevce_cabac_encode_terminate(ps_cabac, 1, 0);

            //i4_slice_seg_len_prev   = i4_slice_seg_len;
            ps_entropy_ctxt->i4_slice_seg_len =
                (WORD32)(ps_cabac->u4_strm_buf_offset - u4_slice_start_offset);

            //ps_entropy_ctxt->i4_slice_seg_len = i4_slice_seg_len; //No need to update it.

            if(ps_entropy_ctxt->i4_slice_seg_len > ps_entropy_ctxt->i4_slice_segment_max_length)
            {
                if(s_cabac_prev_ctb.pu1_strm_buffer == NULL)
                {
                    /* Bytes in a single CTB has exceeded the i4_slice_segment_max_length
                    set by the user. Close the slice-segment and print a warning */

                    /* Store the address of CTB from where next slice segment will start */
                    ps_entropy_ctxt->i4_next_slice_seg_x = ps_entropy_ctxt->i4_ctb_x;
                    ps_entropy_ctxt->i4_next_slice_seg_y = ps_entropy_ctxt->i4_ctb_y;

                    ps_sys_api->ihevce_printf(
                        ps_sys_api->pv_cb_handle,
                        "IHEVCE_WARNING: CTB(%2d, %2d) encoded using %d bytes; "
                        "this exceeds max slice segment size %d as requested "
                        "by the user\n",
                        i4_curr_ctb_x,
                        i4_curr_ctb_y,
                        ps_entropy_ctxt->i4_slice_seg_len,
                        ps_entropy_ctxt->i4_slice_segment_max_length);
                }
                else /* Revert back to previous CTB's state and close current slice */
                {
                    *ps_cabac = s_cabac_prev_ctb;
                    *((UWORD32 *)(ps_cabac->pu1_strm_buffer + ps_cabac->u4_strm_buf_offset - 4)) =
                        u4_prev_ctb_temp;

                    memcpy(
                        &ps_entropy_ctxt->pu1_cu_depth_top[i4_curr_ctb_x * 8], au1_cu_depth_top, 8);
                    memcpy(ps_entropy_ctxt->au1_cu_depth_left, au1_cu_depth_left, 8);

                    *(ps_entropy_ctxt->pu1_skip_cu_top + i4_curr_ctb_x) = u1_skip_cu_top;
                    ps_entropy_ctxt->u4_skip_cu_left = u4_skip_cu_left;

                    ps_entropy_ctxt->i1_cur_qp = i1_last_cu_qp;

                    /* Restore pic info */
                    *(ps_entropy_ctxt->ps_pic_level_info) = s_pic_level_info_backup;

                    /*  encode terminate bin with end-of-slice */
                    ret |= ihevce_cabac_encode_terminate(ps_cabac, 1, 0);

                    /* Store the address of CTB from where next slice segment will start */
                    ps_entropy_ctxt->i4_next_slice_seg_x = i4_curr_ctb_x;
                    ps_entropy_ctxt->i4_next_slice_seg_y = i4_curr_ctb_y;

                    /* As we are reverted back to the previous CTB, force end of slice to zero */
                    *pi4_end_of_slice_flag = 0;
                }
            }
            else if(0 == *pi4_end_of_slice_flag)
            {
                /* As this is not the end of slice, therefore revert back
                the end-of-slice encoding and then add terminate bit */

                /* Signal that this is not slice segment end */
                end_of_slice_seg_flag = 0;

                *ps_cabac = s_cabac_after_ctb;
                *((UWORD32 *)(ps_cabac->pu1_strm_buffer + ps_cabac->u4_strm_buf_offset - 4)) =
                    u4_cur_ctb_temp;

                /*  encode terminate bin */
                ret |= ihevce_cabac_encode_terminate(ps_cabac, 0, 0);
            }

            /* Update variables storing previous CTB's state in order to be
            able to revert to previous CTB's state */
            s_cabac_prev_ctb = s_cabac_after_ctb;
            u4_prev_ctb_temp = u4_cur_ctb_temp;

            i1_last_cu_qp = ps_entropy_ctxt->i1_cur_qp;
        }
        else  //No other slice segment mode supported
        {
            ASSERT(0);
        }

        AEV_TRACE("end_of_slice_flag", end_of_slice_seg_flag, ps_cabac->u4_range);

        if((0 == ps_entropy_ctxt->i4_ctb_x) && (!end_of_slice_seg_flag) &&
           (ps_pps->i1_entropy_coding_sync_enabled_flag))
        {
            /* initialize qp to slice start qp */
            ps_entropy_ctxt->i1_cur_qp = slice_qp;

            /* flush and align to byte bounary for entropy sync every row */
            ret |= ihevce_cabac_encode_terminate(ps_cabac, 1, 1);

            /*This will be entered only during row end, tap bits generated in that row to cal entry point offset*/
            /*add error check to make sure row count doesnt exceed the size of array allocated*/
            ASSERT(ps_entropy_ctxt->i4_ctb_y < MAX_NUM_CTB_ROWS_FRM);
            ps_slice_hdr->pu4_entry_point_offset[ps_entropy_ctxt->i4_ctb_y] =
                ps_cabac->u4_strm_buf_offset;

            /*init the cabac context with top right neighbour*/
            ret |= ihevce_cabac_ctxt_row_init(ps_cabac);
        }

    } while(!end_of_slice_seg_flag);

    if(end_of_slice_seg_flag && ps_pps->i1_entropy_coding_sync_enabled_flag)
    {
        ps_slice_hdr->pu4_entry_point_offset[ps_entropy_ctxt->i4_ctb_y] =
            ps_cabac->u4_strm_buf_offset;
    }

    return ret;
}
