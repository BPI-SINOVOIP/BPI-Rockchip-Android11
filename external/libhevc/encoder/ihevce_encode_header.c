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
* @file ihevce_encode_header.c
*
* @brief
*   This file contains function definitions related to header encoding
*
* @author
*   Ittiam
*
* List of Functions
*   ihevce_generate_nal_unit_header
*   ihevce_generate_when_profile_present
*   ihevce_generate_profile_tier_level
*   ihevce_short_term_ref_pic_set
*   ihevce_generate_bit_rate_pic_rate_info
*   ihevce_generate_aud
*   ihevce_generate_eos
*   ihevce_generate_vps
*   ihevce_generate_sps
*   ihevce_generate_pps
*   ihevce_generate_slice_header
*   ihevce_populate_vps
*   ihevce_populate_sps
*   ihevce_populate_pps
*   ihevce_populate_slice_header
*   ihevce_insert_entry_offset_slice_header
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
#include "ihevc_trans_tables.h"
#include "ihevc_trans_macros.h"

#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_error_checks.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_encode_header.h"
#include "ihevce_encode_header_sei_vui.h"
#include "ihevce_trace.h"

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define CU_LEVEL_QP_LIMIT_8x8 3
#define CU_LEVEL_QP_LIMIT_16x16 2
#define CU_LEVEL_QP_LIMIT_32x32 1

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Generate nal unit header in the stream as per section 7.3.1.2
*
*  @par   Description
*  Inserts the nal type and temporal id plus 1 as per section 7.3.1.2 Nal unit
*  header syntax
*
*  @param[inout]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   nal_unit_type
*  nal type to be inserted
*
*  @param[in]   temporal id
*  temporal id to be inserted
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_nal_unit_header(
    bitstrm_t *ps_bitstrm, WORD32 nal_unit_type, WORD32 nuh_temporal_id)
{
    WORD32 return_status = IHEVCE_SUCCESS;

    /* sanity checks */
    ASSERT((nal_unit_type >= 0) && (nal_unit_type < 64));
    ASSERT((nuh_temporal_id >= 0) && (nuh_temporal_id < 7));

    /* forbidden_zero_bit + nal_unit_type */
    PUT_BITS(
        ps_bitstrm,
        nal_unit_type,
        (1 + 6), /*extra 1 is for forbidden zero bit */
        return_status);

    /* nuh_reserved_zero_6bits */
    PUT_BITS(ps_bitstrm, 0, 6, return_status);

    /* nuh_temporal_id_plus1 */
    PUT_BITS(ps_bitstrm, (nuh_temporal_id + 1), 3, return_status);

    return (return_status);
}

/**
******************************************************************************
*
*  @brief Generates fields related to Profile, Tier and Level data.
*
*  @par   Description
*  Generates fields related to Profile, Tier and Level data.
*  Called when profile_present flag is 1
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_ptl
*  pointer to structure containing Profile, Tier and Level data data
*
*  @return      success or failure error code
*
******************************************************************************
*/
static WORD32
    ihevce_generate_when_profile_present(bitstrm_t *ps_bitstrm, profile_tier_lvl_t *ps_ptl)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    WORD32 i;

    /* XXX_profile_space[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_profile_space, 2, return_status);
    ENTROPY_TRACE("XXX_profile_space[]", ps_ptl->i1_profile_space);

    /* XXX_tier_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_tier_flag, 1, return_status);
    ENTROPY_TRACE("XXX_tier_flag[]", ps_ptl->i1_tier_flag);

    /* XXX_profile_idc[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_profile_idc, 5, return_status);
    ENTROPY_TRACE("XXX_profile_idc[]", ps_ptl->i1_profile_idc);

    for(i = 0; i < MAX_PROFILE_COMPATBLTY; i++)
    {
        /* XXX_profile_compatibility_flag[][j] */
        PUT_BITS(ps_bitstrm, ps_ptl->ai1_profile_compatibility_flag[i], 1, return_status);
        ENTROPY_TRACE(
            "XXX_profile_compatibility_flag[][j]", ps_ptl->ai1_profile_compatibility_flag[i]);
    }

    /* XXX_progressive_source_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_progressive_source_flag, 1, return_status);
    ENTROPY_TRACE("XXX_progressive_source_flag[]", ps_ptl->i1_general_progressive_source_flag);

    /* XXX_interlaced_source_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_interlaced_source_flag, 1, return_status);
    ENTROPY_TRACE("XXX_interlaced_source_flag[]", ps_ptl->i1_general_interlaced_source_flag);

    /* XXX_non_packed_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_non_packed_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_non_packed_constraint_flag[]", ps_ptl->i1_general_non_packed_constraint_flag);

    /* XXX_frame_only_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_frame_only_constraint_flag, 1, return_status);
    ENTROPY_TRACE("XXX_frame_only_constraint_flag[]", ps_ptl->i1_frame_only_constraint_flag);

    /* XXX_general_max_12bit_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_12bit_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_12bit_constraint_flag[]", ps_ptl->i1_general_max_12bit_constraint_flag);

    /* XXX_general_max_10bit_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_10bit_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_10bit_constraint_flag[]", ps_ptl->i1_general_max_10bit_constraint_flag);

    /* XXX_general_max_8bit_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_8bit_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_8bit_constraint_flag[]", ps_ptl->i1_general_max_8bit_constraint_flag);

    /* XXX_general_max_422chroma_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_422chroma_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_422chroma_constraint_flag[]",
        ps_ptl->i1_general_max_422chroma_constraint_flag);

    /* XXX_general_max_420chroma_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_420chroma_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_420chroma_constraint_flag[]",
        ps_ptl->i1_general_max_420chroma_constraint_flag);

    /* XXX_general_max_monochrome_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_max_monochrome_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_max_monochrome_constraint_flag[]",
        ps_ptl->i1_general_max_monochrome_constraint_flag);

    /* XXX_general_intra_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_intra_constraint_flag, 1, return_status);
    ENTROPY_TRACE("XXX_general_intra_constraint_flag[]", ps_ptl->i1_general_intra_constraint_flag);

    /* XXX_general_one_picture_only_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_one_picture_only_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_one_picture_only_constraint_flag[]",
        ps_ptl->i1_general_one_picture_only_constraint_flag);

    /* XXX_general_lower_bit_rate_constraint_flag[] */
    PUT_BITS(ps_bitstrm, ps_ptl->i1_general_lower_bit_rate_constraint_flag, 1, return_status);
    ENTROPY_TRACE(
        "XXX_general_lower_bit_rate_constraint_flag[]",
        ps_ptl->i1_general_lower_bit_rate_constraint_flag);

    /* XXX_reserved_zero_35bits[] */
    PUT_BITS(ps_bitstrm, 0, 16, return_status);
    PUT_BITS(ps_bitstrm, 0, 16, return_status);
    PUT_BITS(ps_bitstrm, 0, 3, return_status);
    ENTROPY_TRACE("XXX_reserved_zero_35bits[]", 0);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates Profile, Tier and Level data
*
*  @par   Description
*  Generates Profile, Tier and Level data as per Section 7.3.3
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_ptl
*  pointer to structure containing Profile, Tier and Level data data
*
*  @param[in]   i1_profile_present_flag
*  flag that indicates whether profile-related data is present
*
*  @param[in]   i1_vps_max_sub_layers_minus1
*  (Maximum number of sub_layers present) minus 1
*
*  @return      success or failure error code
*
******************************************************************************
*/
static WORD32 ihevce_generate_profile_tier_level(
    bitstrm_t *ps_bitstrm,
    profile_tier_lvl_info_t *ps_ptl,
    WORD8 i1_profile_present_flag,
    WORD8 i1_max_sub_layers_minus1)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;

    if(i1_profile_present_flag)
    {
        ihevce_generate_when_profile_present(ps_bitstrm, &ps_ptl->s_ptl_gen);
    }

    /* general_level_idc */
    PUT_BITS(ps_bitstrm, ps_ptl->s_ptl_gen.u1_level_idc, 8, return_status);
    ENTROPY_TRACE("general_level_idc", ps_ptl->s_ptl_gen.u1_level_idc);

    for(i = 0; i < i1_max_sub_layers_minus1; i++)
    {
        /* sub_layer_profile_present_flag[i] */
        PUT_BITS(ps_bitstrm, ps_ptl->ai1_sub_layer_profile_present_flag[i], 1, return_status);
        ENTROPY_TRACE(
            "sub_layer_profile_present_flag[i]", ps_ptl->ai1_sub_layer_profile_present_flag[i]);

        /* sub_layer_level_present_flag[i] */
        PUT_BITS(ps_bitstrm, ps_ptl->ai1_sub_layer_level_present_flag[i], 1, return_status);
        ENTROPY_TRACE(
            "sub_layer_level_present_flag[i]", ps_ptl->ai1_sub_layer_level_present_flag[i]);
    }

    if(i1_max_sub_layers_minus1 > 0)
    {
        for(i = i1_max_sub_layers_minus1; i < 8; i++)
        {
            /* reserved_zero_2bits[i] */
            PUT_BITS(ps_bitstrm, 0, 2, return_status);
            ENTROPY_TRACE("reserved_zero_2bits[i]", 0);
        }
    }

    for(i = 0; i < i1_max_sub_layers_minus1; i++)
    {
        if(ps_ptl->ai1_sub_layer_profile_present_flag[i])
        {
            ihevce_generate_when_profile_present(ps_bitstrm, &ps_ptl->as_ptl_sub[i]);
        }

        if(ps_ptl->ai1_sub_layer_level_present_flag[i])  //TEMPORALA_SCALABILITY CHANGES BUG_FIX
        {
            /* sub_layer_level_idc[i] */
            PUT_BITS(ps_bitstrm, ps_ptl->as_ptl_sub[i].u1_level_idc, 8, return_status);
            ENTROPY_TRACE("sub_layer_level_idc[i]", ps_ptl->as_ptl_sub[i].u1_level_idc);
        }
    }

    return return_status;
}

/**
*******************************************************************************
*
* @brief
*  Generates short term reference picture set
*
* @par   Description
*  Generates short term reference picture set as per section 7.3.5.2.
*  Can be called by either SPS or Slice header parsing modules.
*
* @param[in] ps_bitstrm
*  Pointer to bitstream structure
*
* @param[out] ps_stref_picset_base
*  Pointer to first short term ref pic set structure
*
* @param[in] num_short_term_ref_pic_sets
*  Number of short term reference pic sets
*
* @param[in] idx
*  Current short term ref pic set id
*
* @returns Error code from WORD32
*
*
*******************************************************************************
*/
static WORD32 ihevce_short_term_ref_pic_set(
    bitstrm_t *ps_bitstrm,
    stref_picset_t *ps_stref_picset_base,
    WORD32 num_short_term_ref_pic_sets,
    WORD32 idx,
    WORD32 *pi4_NumPocTotalCurr)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    stref_picset_t *ps_stref_picset = ps_stref_picset_base + idx;

    (void)num_short_term_ref_pic_sets;
    if(idx > 0)
    {
        /* inter_ref_pic_set_prediction_flag */
        PUT_BITS(
            ps_bitstrm, ps_stref_picset->i1_inter_ref_pic_set_prediction_flag, 1, return_status);
        ENTROPY_TRACE(
            "inter_ref_pic_set_prediction_flag",
            ps_stref_picset->i1_inter_ref_pic_set_prediction_flag);
    }

    /* This flag is assumed to be 0 for now */
    ASSERT(0 == ps_stref_picset->i1_inter_ref_pic_set_prediction_flag);

    /* num_negative_pics */
    PUT_BITS_UEV(ps_bitstrm, ps_stref_picset->i1_num_neg_pics, return_status);
    ENTROPY_TRACE("num_negative_pics", ps_stref_picset->i1_num_neg_pics);

    /* num_positive_pics */
    PUT_BITS_UEV(ps_bitstrm, ps_stref_picset->i1_num_pos_pics, return_status);
    ENTROPY_TRACE("num_positive_pics", ps_stref_picset->i1_num_pos_pics);

    for(i = 0; i < ps_stref_picset->i1_num_neg_pics; i++)
    {
        /* delta_poc_s0_minus1 */
        PUT_BITS_UEV(ps_bitstrm, ps_stref_picset->ai2_delta_poc[i] - 1, return_status);
        ENTROPY_TRACE("delta_poc_s0_minus1", ps_stref_picset->ai2_delta_poc[i] - 1);

        /* used_by_curr_pic_s0_flag */
        PUT_BITS(ps_bitstrm, ps_stref_picset->ai1_used[i], 1, return_status);
        ENTROPY_TRACE("used_by_curr_pic_s0_flag", ps_stref_picset->ai1_used[i]);
        /*get the num pocs used for cur pic*/
        if(ps_stref_picset->ai1_used[i])
        {
            *pi4_NumPocTotalCurr += 1;
        }
    }

    for(; i < (ps_stref_picset->i1_num_pos_pics + ps_stref_picset->i1_num_neg_pics); i++)
    {
        /* delta_poc_s1_minus1 */
        PUT_BITS_UEV(ps_bitstrm, ps_stref_picset->ai2_delta_poc[i] - 1, return_status);
        ENTROPY_TRACE("delta_poc_s1_minus1", ps_stref_picset->ai2_delta_poc[i] - 1);

        /* used_by_curr_pic_s1_flag */
        PUT_BITS(ps_bitstrm, ps_stref_picset->ai1_used[i], 1, return_status);
        ENTROPY_TRACE("used_by_curr_pic_s1_flag", ps_stref_picset->ai1_used[i]);
        /*get the num pocs used for cur pic*/
        if(ps_stref_picset->ai1_used[i])
        {
            *pi4_NumPocTotalCurr += 1;
        }
    }

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates ref pic list modification
*
*  @par   Description
*  Generate ref pic list modification syntax as per Section 7.3.6.2
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_slice_hdr
*  pointer to structure containing slice header
*
*  @return      success or failure error code
*
******************************************************************************
*/
static WORD32 ref_pic_list_modification(
    bitstrm_t *ps_bitstrm, slice_header_t *ps_slice_hdr, WORD32 i4_NumPocTotalCurr)
{
    WORD32 return_status = IHEVCE_SUCCESS;
    WORD32 i;

    /* ref_pic_list_modification_flag_l0 */
    PUT_BITS(
        ps_bitstrm, ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l0, 1, return_status);
    ENTROPY_TRACE(
        "ref_pic_list_modification_flag_l0",
        ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l0);

    if(ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l0)
    {
        for(i = 0; i <= (ps_slice_hdr->i1_num_ref_idx_l0_active - 1); i++)
        {
            WORD32 num_bits = 32 - CLZ(i4_NumPocTotalCurr - 1);

            /* list_entry_l0[ i ] */
            PUT_BITS(ps_bitstrm, ps_slice_hdr->s_rplm.i1_list_entry_l0[i], num_bits, return_status);
            ENTROPY_TRACE("list_entry_l0", ps_slice_hdr->s_rplm.i1_list_entry_l0[i]);
        }
    }

    if((BSLICE == ps_slice_hdr->i1_slice_type))
    {
        /* ref_pic_list_modification_flag_l1 */
        PUT_BITS(
            ps_bitstrm, ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l1, 1, return_status);
        ENTROPY_TRACE(
            "ref_pic_list_modification_flag_l1",
            ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l1);

        if(ps_slice_hdr->s_rplm.i1_ref_pic_list_modification_flag_l1)
        {
            for(i = 0; i <= (ps_slice_hdr->i1_num_ref_idx_l1_active - 1); i++)
            {
                WORD32 num_bits = 32 - CLZ(i4_NumPocTotalCurr - 1);

                /* list_entry_l1[ i ] */
                PUT_BITS(
                    ps_bitstrm, ps_slice_hdr->s_rplm.i1_list_entry_l1[i], num_bits, return_status);
                ENTROPY_TRACE("list_entry_l1", ps_slice_hdr->s_rplm.i1_list_entry_l1[i]);
            }
        }
    } /*end of B slice check*/

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates Pred Weight Table
*
*  @par   Description
*  Generate Pred Weight Table as per Section 7.3.5.4
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sps
*  pointer to structure containing SPS data
*
*  @param[in]   ps_pps
*  pointer to structure containing PPS data
*
*  @param[in]   ps_slice_hdr
*  pointer to structure containing slice header
*
*  @return      success or failure error code
*
******************************************************************************
*/
static WORD32 ihevce_generate_pred_weight_table(
    bitstrm_t *ps_bitstrm, sps_t *ps_sps, pps_t *ps_pps, slice_header_t *ps_slice_hdr)
{
    WORD32 i;
    WORD32 delta_luma_weight;
    WORD32 delta_chroma_weight;
    WORD32 return_status = IHEVCE_SUCCESS;
    pred_wt_ofst_t *ps_wt_ofst = &ps_slice_hdr->s_wt_ofst;
    UWORD32 u4_luma_log2_weight_denom = ps_wt_ofst->i1_luma_log2_weight_denom;
    WORD32 chroma_log2_weight_denom = (ps_wt_ofst->i1_chroma_log2_weight_denom);
    WORD32 i4_wght_count = 0;

    (void)ps_pps;
    /* luma_log2_weight_denom */
    PUT_BITS_UEV(ps_bitstrm, u4_luma_log2_weight_denom, return_status);
    ENTROPY_TRACE("luma_log2_weight_denom", u4_luma_log2_weight_denom);

    if(ps_sps->i1_chroma_format_idc != 0)
    {
        /* delta_chroma_log2_weight_denom */
        PUT_BITS_SEV(
            ps_bitstrm, chroma_log2_weight_denom - u4_luma_log2_weight_denom, return_status);
        ENTROPY_TRACE(
            "delta_chroma_log2_weight_denom", chroma_log2_weight_denom - u4_luma_log2_weight_denom);
    }

    for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l0_active; i++)
    {
        /* luma_weight_l0_flag[ i ] */
        PUT_BITS(ps_bitstrm, ps_wt_ofst->i1_luma_weight_l0_flag[i], 1, return_status);
        i4_wght_count += ps_wt_ofst->i1_luma_weight_l0_flag[i];
        assert(i4_wght_count <= 24);
        ENTROPY_TRACE("luma_weight_l0_flag[ i ]", ps_wt_ofst->i1_luma_weight_l0_flag[i]);
    }

    if(ps_sps->i1_chroma_format_idc != 0)
    {
        for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l0_active; i++)
        {
            /* chroma_weight_l0_flag[ i ] */
            PUT_BITS(ps_bitstrm, ps_wt_ofst->i1_chroma_weight_l0_flag[i], 1, return_status);
            i4_wght_count += 2 * ps_wt_ofst->i1_chroma_weight_l0_flag[i];
            assert(i4_wght_count <= 24);
            ENTROPY_TRACE("chroma_weight_l0_flag[ i ]", ps_wt_ofst->i1_chroma_weight_l0_flag[i]);
        }
    }

    delta_luma_weight = (1 << u4_luma_log2_weight_denom);
    delta_chroma_weight = (1 << chroma_log2_weight_denom);

    for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l0_active; i++)
    {
        if(ps_wt_ofst->i1_luma_weight_l0_flag[i])
        {
            /* delta_luma_weight_l0[ i ] */
            PUT_BITS_SEV(
                ps_bitstrm, ps_wt_ofst->i2_luma_weight_l0[i] - delta_luma_weight, return_status);
            ENTROPY_TRACE(
                "delta_luma_weight_l0[ i ]", ps_wt_ofst->i2_luma_weight_l0[i] - delta_luma_weight);

            /* luma_offset_l0[ i ] */
            PUT_BITS_SEV(ps_bitstrm, ps_wt_ofst->i2_luma_offset_l0[i], return_status);
            ENTROPY_TRACE("luma_offset_l0[ i ]", ps_wt_ofst->i2_luma_offset_l0[i]);
        }

        if(ps_wt_ofst->i1_chroma_weight_l0_flag[i])
        {
            WORD32 shift = (1 << (BIT_DEPTH_CHROMA - 1));
            WORD32 delta_chroma_weight_l0[2];
            WORD32 delta_chroma_offset_l0[2];

            delta_chroma_weight_l0[0] = ps_wt_ofst->i2_chroma_weight_l0_cb[i] - delta_chroma_weight;
            delta_chroma_weight_l0[1] = ps_wt_ofst->i2_chroma_weight_l0_cr[i] - delta_chroma_weight;

            delta_chroma_offset_l0[0] =
                ps_wt_ofst->i2_chroma_offset_l0_cb[i] +
                ((shift * ps_wt_ofst->i2_chroma_weight_l0_cb[i]) >> chroma_log2_weight_denom) -
                shift;
            delta_chroma_offset_l0[1] =
                ps_wt_ofst->i2_chroma_offset_l0_cr[i] +
                ((shift * ps_wt_ofst->i2_chroma_weight_l0_cr[i]) >> chroma_log2_weight_denom) -
                shift;

            /* delta_chroma_weight_l0[ i ][j] */
            PUT_BITS_SEV(ps_bitstrm, delta_chroma_weight_l0[0], return_status);
            ENTROPY_TRACE("delta_chroma_weight_l0[ i ]", delta_chroma_weight_l0[0]);

            /* delta_chroma_offset_l0[ i ][j] */
            PUT_BITS_SEV(ps_bitstrm, delta_chroma_offset_l0[0], return_status);
            ENTROPY_TRACE("delta_chroma_offset_l0[ i ]", delta_chroma_offset_l0[0]);

            /* delta_chroma_weight_l0[ i ][j] */
            PUT_BITS_SEV(ps_bitstrm, delta_chroma_weight_l0[1], return_status);
            ENTROPY_TRACE("delta_chroma_weight_l0[ i ]", delta_chroma_weight_l0[1]);

            /* delta_chroma_offset_l0[ i ][j] */
            PUT_BITS_SEV(ps_bitstrm, delta_chroma_offset_l0[1], return_status);
            ENTROPY_TRACE("delta_chroma_offset_l0[ i ]", delta_chroma_offset_l0[1]);
        }
    }

    if(BSLICE == ps_slice_hdr->i1_slice_type)
    {
        for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l1_active; i++)
        {
            /* luma_weight_l1_flag[ i ] */
            PUT_BITS(ps_bitstrm, ps_wt_ofst->i1_luma_weight_l1_flag[i], 1, return_status);
            i4_wght_count += ps_wt_ofst->i1_luma_weight_l1_flag[i];
            assert(i4_wght_count <= 24);
            ENTROPY_TRACE("luma_weight_l1_flag[ i ]", ps_wt_ofst->i1_luma_weight_l1_flag[i]);
        }

        if(ps_sps->i1_chroma_format_idc != 0)
        {
            for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l1_active; i++)
            {
                /* chroma_weight_l1_flag[ i ] */
                PUT_BITS(ps_bitstrm, ps_wt_ofst->i1_chroma_weight_l1_flag[i], 1, return_status);
                i4_wght_count += ps_wt_ofst->i1_chroma_weight_l1_flag[i];
                assert(i4_wght_count <= 24);
                ENTROPY_TRACE(
                    "chroma_weight_l1_flag[ i ]", ps_wt_ofst->i1_chroma_weight_l1_flag[i]);
            }
        }

        for(i = 0; i < ps_slice_hdr->i1_num_ref_idx_l1_active; i++)
        {
            if(ps_wt_ofst->i1_luma_weight_l1_flag[i])
            {
                /* delta_luma_weight_l1[ i ] */
                PUT_BITS_SEV(
                    ps_bitstrm,
                    ps_wt_ofst->i2_luma_weight_l1[i] - delta_luma_weight,
                    return_status);
                ENTROPY_TRACE(
                    "delta_luma_weight_l1[ i ]",
                    ps_wt_ofst->i2_luma_weight_l1[i] - delta_luma_weight);

                /* luma_offset_l1[ i ] */
                PUT_BITS_SEV(ps_bitstrm, ps_wt_ofst->i2_luma_offset_l1[i], return_status);
                ENTROPY_TRACE("luma_offset_l1[ i ]", ps_wt_ofst->i2_luma_offset_l1[i]);
            }

            if(ps_wt_ofst->i1_chroma_weight_l1_flag[i])
            {
                WORD32 shift = (1 << (BIT_DEPTH_CHROMA - 1));
                WORD32 delta_chroma_weight_l1[2];
                WORD32 delta_chroma_offset_l1[2];

                delta_chroma_weight_l1[0] =
                    ps_wt_ofst->i2_chroma_weight_l1_cb[i] - delta_chroma_weight;
                delta_chroma_weight_l1[1] =
                    ps_wt_ofst->i2_chroma_weight_l1_cr[i] - delta_chroma_weight;

                delta_chroma_offset_l1[0] =
                    ps_wt_ofst->i2_chroma_offset_l1_cb[i] +
                    ((shift * ps_wt_ofst->i2_chroma_weight_l1_cb[i]) >> chroma_log2_weight_denom) -
                    shift;
                delta_chroma_offset_l1[1] =
                    ps_wt_ofst->i2_chroma_offset_l1_cr[i] +
                    ((shift * ps_wt_ofst->i2_chroma_weight_l1_cr[i]) >> chroma_log2_weight_denom) -
                    shift;

                /* delta_chroma_weight_l1[ i ][j] */
                PUT_BITS_SEV(ps_bitstrm, delta_chroma_weight_l1[0], return_status);
                ENTROPY_TRACE("delta_chroma_weight_l1[ i ]", delta_chroma_weight_l1[0]);

                /* delta_chroma_offset_l1[ i ][j] */
                PUT_BITS_SEV(ps_bitstrm, delta_chroma_offset_l1[0], return_status);
                ENTROPY_TRACE("delta_chroma_offset_l1[ i ]", delta_chroma_offset_l1[0]);

                /* delta_chroma_weight_l1[ i ][j] */
                PUT_BITS_SEV(ps_bitstrm, delta_chroma_weight_l1[1], return_status);
                ENTROPY_TRACE("delta_chroma_weight_l1[ i ]", delta_chroma_weight_l1[1]);

                /* delta_chroma_offset_l1[ i ][j] */
                PUT_BITS_SEV(ps_bitstrm, delta_chroma_offset_l1[1], return_status);
                ENTROPY_TRACE("delta_chroma_offset_l1[ i ]", delta_chroma_offset_l1[1]);
            }
        }
    }

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates AUD (Access Unit Delimiter)
*
*  @par   Description
*  Generate Access Unit Delimiter as per Section 7.3.2.5
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   pic_type
*  picture type
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_aud(bitstrm_t *ps_bitstrm, WORD32 pic_type)
{
    WORD32 return_status = IHEVCE_SUCCESS;

    /* Insert the NAL start code */
    return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, NAL_AUD, 0);

    /* pic_type */
    PUT_BITS(ps_bitstrm, pic_type, 3, return_status);
    ENTROPY_TRACE("pic type", pic_type);

    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates EOS (End of Sequence)
*
*  @par   Description
*  Generate End of sequence as per Section 7.3.2.6
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_eos(bitstrm_t *ps_bitstrm)
{
    WORD32 return_status = IHEVCE_SUCCESS;

    /* Insert the NAL start code */
    return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, NAL_EOS, 0);

    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates VPS (Video Parameter Set)
*
*  @par   Description
*  Generate Video Parameter Set as per Section 7.3.2.1
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_vps
*  pointer to structure containing VPS data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_vps(bitstrm_t *ps_bitstrm, vps_t *ps_vps)
{
    WORD32 i;
    WORD8 i1_vps_max_sub_layers_minus1 = ps_vps->i1_vps_max_sub_layers - 1;
    WORD32 return_status = IHEVCE_SUCCESS;

    /* Insert Start Code */
    ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    ihevce_generate_nal_unit_header(ps_bitstrm, NAL_VPS, 0);

    /* video_parameter_set_id */
    PUT_BITS(ps_bitstrm, ps_vps->i1_vps_id, 4, return_status);
    ENTROPY_TRACE("video_parameter_set_id", ps_vps->i1_vps_id);

    /* vps_reserved_three_2bits */
    PUT_BITS(ps_bitstrm, 3, 2, return_status);
    ENTROPY_TRACE("vps_reserved_three_2bits", 3);

    /* vps_max_layers_minus1  */
    PUT_BITS(ps_bitstrm, 0, 6, return_status);
    ENTROPY_TRACE("vps_max_layers_minus1 ", 3);

    /* vps_max_sub_layers_minus1 */
    PUT_BITS(ps_bitstrm, i1_vps_max_sub_layers_minus1, 3, return_status);
    ENTROPY_TRACE("vps_max_sub_layers_minus1", i1_vps_max_sub_layers_minus1);

    /* vps_temporal_id_nesting_flag */
    PUT_BITS(ps_bitstrm, ps_vps->i1_vps_temporal_id_nesting_flag, 1, return_status);
    ENTROPY_TRACE("vps_temporal_id_nesting_flag", ps_vps->i1_vps_temporal_id_nesting_flag);

    /* vps_reserved_0xffff_16bits */
    PUT_BITS(ps_bitstrm, 0xffff, 16, return_status);
    ENTROPY_TRACE("vps_reserved_0xffff_16bits", 0xffff);

    /* profile-tier and level info */
    ihevce_generate_profile_tier_level(ps_bitstrm, &ps_vps->s_ptl, 1, i1_vps_max_sub_layers_minus1);

    /* vps_sub_layer_ordering_info_present_flag */
    PUT_BITS(ps_bitstrm, ps_vps->i1_sub_layer_ordering_info_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "vps_sub_layer_ordering_info_present_flag",
        ps_vps->i1_sub_layer_ordering_info_present_flag);

    i = ps_vps->i1_sub_layer_ordering_info_present_flag ? 0 : i1_vps_max_sub_layers_minus1;

    for(; i <= i1_vps_max_sub_layers_minus1; i++)
    {
        /* vps_max_dec_pic_buffering[i] */
        PUT_BITS_UEV(ps_bitstrm, ps_vps->ai1_vps_max_dec_pic_buffering[i], return_status);
        ENTROPY_TRACE(
            "vps_max_dec_pic_buffering_minus1[i]", ps_vps->ai1_vps_max_dec_pic_buffering[i]);

        /* vps_num_reorder_pics[i] */
        PUT_BITS_UEV(ps_bitstrm, ps_vps->ai1_vps_max_num_reorder_pics[i], return_status);
        ENTROPY_TRACE("ai1_vps_max_num_reorder_pics[i]", ps_vps->ai1_vps_max_num_reorder_pics[i]);

        /* vps_max_latency_increase[i] */
        PUT_BITS_UEV(ps_bitstrm, ps_vps->ai1_vps_max_latency_increase[i], return_status);
        ENTROPY_TRACE("ai1_vps_max_latency_increase[i]", ps_vps->ai1_vps_max_latency_increase[i]);
    }

    /* vps_max_layer_id */
    PUT_BITS(ps_bitstrm, ps_vps->i1_vps_max_nuh_reserved_zero_layer_id, 6, return_status);
    ENTROPY_TRACE("vps_max_layer_id", ps_vps->i1_vps_max_nuh_reserved_zero_layer_id);

    /* vps_num_layer_sets_minus1 */
    PUT_BITS_UEV(ps_bitstrm, 0, return_status);
    ENTROPY_TRACE("vps_num_layer_sets_minus1", 0);

    /* vps_timing_info_present_flag */
    PUT_BITS(ps_bitstrm, 0, 1, return_status);
    ENTROPY_TRACE("vps_timing_info_present_flag", 0);

    /* vps_extension_flag */
    PUT_BITS(ps_bitstrm, 0, 1, return_status);
    ENTROPY_TRACE("vps_extension_flag", 0);

    /* rbsp trailing bits */
    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates SPS (Video Parameter Set)
*
*  @par   Description
*  Parse Video Parameter Set as per Section 7.3.2.2
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_sps
*  pointer to structure containing SPS data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_sps(bitstrm_t *ps_bitstrm, sps_t *ps_sps)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    WORD8 i1_max_sub_layers_minus1 = ps_sps->i1_sps_max_sub_layers - 1;

    UWORD32 u4_log2_max_pic_order_cnt_lsb = (UWORD32)(ps_sps->i1_log2_max_pic_order_cnt_lsb);

    UWORD32 u4_log2_min_coding_block_size_minus3 =
        (UWORD32)(ps_sps->i1_log2_min_coding_block_size) - 3;

    UWORD32 u4_log2_diff_max_min_coding_block_size =
        (UWORD32)(ps_sps->i1_log2_diff_max_min_coding_block_size);

    UWORD32 u4_log2_min_transform_block_size_minus2 =
        (UWORD32)(ps_sps->i1_log2_min_transform_block_size) - 2;

    UWORD32 u4_log2_diff_max_min_transform_block_size =
        (UWORD32)(ps_sps->i1_log2_diff_max_min_transform_block_size);

    /* Insert Start Code */
    return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, NAL_SPS, 0);

    /* video_parameter_set_id */
    PUT_BITS(ps_bitstrm, ps_sps->i1_vps_id, 4, return_status);
    ENTROPY_TRACE("video_parameter_set_id", ps_sps->i1_vps_id);

    /* sps_max_sub_layers_minus1 */
    PUT_BITS(ps_bitstrm, i1_max_sub_layers_minus1, 3, return_status);
    ENTROPY_TRACE("sps_max_sub_layers_minus1", i1_max_sub_layers_minus1);

    /* sps_temporal_id_nesting_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_sps_temporal_id_nesting_flag, 1, return_status);
    ENTROPY_TRACE("sps_temporal_id_nesting_flag", ps_sps->i1_sps_temporal_id_nesting_flag);

    /* profile-tier and level info */
    ihevce_generate_profile_tier_level(ps_bitstrm, &ps_sps->s_ptl, 1, i1_max_sub_layers_minus1);

    /* seq_parameter_set_id */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_sps_id, return_status);
    ENTROPY_TRACE("seq_parameter_set_id", ps_sps->i1_sps_id);

    /* chroma_format_idc */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_chroma_format_idc, return_status);
    ENTROPY_TRACE("chroma_format_idc", ps_sps->i1_chroma_format_idc);

    if(CHROMA_FMT_IDC_YUV444 == ps_sps->i1_chroma_format_idc)
    {
        /* separate_colour_plane_flag */
        PUT_BITS(ps_bitstrm, 1, 1, return_status);
        ENTROPY_TRACE("separate_colour_plane_flag", 1);
    }

    /* pic_width_in_luma_samples */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_width_in_luma_samples, return_status);
    ENTROPY_TRACE("pic_width_in_luma_samples", ps_sps->i2_pic_width_in_luma_samples);

    /* pic_height_in_luma_samples */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_height_in_luma_samples, return_status);
    ENTROPY_TRACE("pic_height_in_luma_samples", ps_sps->i2_pic_height_in_luma_samples);

    /* pic_cropping_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_pic_cropping_flag, 1, return_status);
    ENTROPY_TRACE("pic_cropping_flag", ps_sps->i1_pic_cropping_flag);

    if(ps_sps->i1_pic_cropping_flag)
    {
        /* pic_crop_left_offset */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_crop_left_offset, return_status);
        ENTROPY_TRACE("pic_crop_left_offset", ps_sps->i2_pic_crop_left_offset);

        /* pic_crop_right_offset */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_crop_right_offset, return_status);
        ENTROPY_TRACE("pic_crop_right_offset", ps_sps->i2_pic_crop_right_offset);

        /* pic_crop_top_offset */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_crop_top_offset, return_status);
        ENTROPY_TRACE("pic_crop_top_offset", ps_sps->i2_pic_crop_top_offset);

        /* pic_crop_bottom_offset */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->i2_pic_crop_bottom_offset, return_status);
        ENTROPY_TRACE("pic_crop_bottom_offset", ps_sps->i2_pic_crop_bottom_offset);
    }

    /* bit_depth_luma_minus8 */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_bit_depth_luma_minus8, return_status);
    ENTROPY_TRACE("bit_depth_luma_minus8", ps_sps->i1_bit_depth_luma_minus8);

    /* bit_depth_chroma_minus8 */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_bit_depth_chroma_minus8, return_status);
    ENTROPY_TRACE("i1_bit_depth_chroma_minus8", ps_sps->i1_bit_depth_chroma_minus8);

    /* log2_max_pic_order_cnt_lsb_minus4 */
    PUT_BITS_UEV(ps_bitstrm, u4_log2_max_pic_order_cnt_lsb - 4, return_status);
    ENTROPY_TRACE("log2_max_pic_order_cnt_lsb_minus4", u4_log2_max_pic_order_cnt_lsb - 4);

    /* sps_sub_layer_ordering_info_present_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_sps_sub_layer_ordering_info_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "sps_sub_layer_ordering_info_present_flag",
        ps_sps->i1_sps_sub_layer_ordering_info_present_flag);

    i = ps_sps->i1_sps_sub_layer_ordering_info_present_flag ? 0 : i1_max_sub_layers_minus1;

    for(; i <= i1_max_sub_layers_minus1; i++)
    {
        /* max_dec_pic_buffering */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->ai1_sps_max_dec_pic_buffering[i], return_status);
        ENTROPY_TRACE("max_dec_pic_buffering_minus1", ps_sps->ai1_sps_max_dec_pic_buffering[i]);

        /* num_reorder_pics */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->ai1_sps_max_num_reorder_pics[i], return_status);
        ENTROPY_TRACE("num_reorder_pics", ps_sps->ai1_sps_max_num_reorder_pics[i]);

        /* max_latency_increase */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->ai1_sps_max_latency_increase[i], return_status);
        ENTROPY_TRACE("max_latency_increase", ps_sps->ai1_sps_max_latency_increase[i]);
    }

    /* log2_min_coding_block_size_minus3 */
    PUT_BITS_UEV(ps_bitstrm, u4_log2_min_coding_block_size_minus3, return_status);
    ENTROPY_TRACE("log2_min_coding_block_size_minus3", u4_log2_min_coding_block_size_minus3);

    /* log2_diff_max_min_coding_block_size */
    PUT_BITS_UEV(ps_bitstrm, u4_log2_diff_max_min_coding_block_size, return_status);
    ENTROPY_TRACE("log2_diff_max_min_coding_block_size", u4_log2_diff_max_min_coding_block_size);

    /* log2_min_transform_block_size_minus2 */
    PUT_BITS_UEV(ps_bitstrm, u4_log2_min_transform_block_size_minus2, return_status);
    ENTROPY_TRACE("log2_min_transform_block_size_minus2", u4_log2_min_transform_block_size_minus2);

    /* log2_diff_max_min_transform_block_size */
    PUT_BITS_UEV(ps_bitstrm, u4_log2_diff_max_min_transform_block_size, return_status);
    ENTROPY_TRACE(
        "log2_diff_max_min_transform_block_size", u4_log2_diff_max_min_transform_block_size);

    /* max_transform_hierarchy_depth_inter */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_max_transform_hierarchy_depth_inter, return_status);
    ENTROPY_TRACE(
        "max_transform_hierarchy_depth_inter", ps_sps->i1_max_transform_hierarchy_depth_inter);

    /* max_transform_hierarchy_depth_intra */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_max_transform_hierarchy_depth_intra, return_status);
    ENTROPY_TRACE(
        "max_transform_hierarchy_depth_intra", ps_sps->i1_max_transform_hierarchy_depth_intra);

    /* scaling_list_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_scaling_list_enable_flag, 1, return_status);
    ENTROPY_TRACE("scaling_list_enabled_flag", ps_sps->i1_scaling_list_enable_flag);

    if(ps_sps->i1_scaling_list_enable_flag)
    {
        /* sps_scaling_list_data_present_flag */
        PUT_BITS(ps_bitstrm, ps_sps->i1_sps_scaling_list_data_present_flag, 1, return_status);
        ENTROPY_TRACE(
            "sps_scaling_list_data_present_flag", ps_sps->i1_sps_scaling_list_data_present_flag);

#if 0 /* TODO: Will be enabled once scaling list support is added */
        if(ps_sps->i1_sps_scaling_list_data_present_flag)
        {
            //TODO
            ihevce_generate_scaling_list_data(ps_bitstrm);
        }
#endif
    }

    /* asymmetric_motion_partitions_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_amp_enabled_flag, 1, return_status);
    ENTROPY_TRACE("asymmetric_motion_partitions_enabled_flag", ps_sps->i1_amp_enabled_flag);

    /* sample_adaptive_offset_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_sample_adaptive_offset_enabled_flag, 1, return_status);
    ENTROPY_TRACE(
        "sample_adaptive_offset_enabled_flag", ps_sps->i1_sample_adaptive_offset_enabled_flag);

    /* pcm_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_pcm_enabled_flag, 1, return_status);
    ENTROPY_TRACE("pcm_enabled_flag", ps_sps->i1_pcm_enabled_flag);
    if(ps_sps->i1_pcm_enabled_flag)
    {
        UWORD32 u4_log2_min_pcm_coding_block_size = (ps_sps->i1_log2_min_pcm_coding_block_size);
        UWORD32 u4_log2_diff_max_min_pcm_coding_block_size =
            (ps_sps->i1_log2_diff_max_min_pcm_coding_block_size);

        /* pcm_sample_bit_depth_luma_minus1 */
        PUT_BITS(ps_bitstrm, ps_sps->i1_pcm_sample_bit_depth_luma - 1, 4, return_status);
        ENTROPY_TRACE("pcm_sample_bit_depth_luma", ps_sps->i1_pcm_sample_bit_depth_luma - 1);

        /* pcm_sample_bit_depth_chroma_minus1 */
        PUT_BITS(ps_bitstrm, ps_sps->i1_pcm_sample_bit_depth_chroma - 1, 4, return_status);
        ENTROPY_TRACE("pcm_sample_bit_depth_chroma", ps_sps->i1_pcm_sample_bit_depth_chroma - 1);

        /* log2_min_pcm_coding_block_size_minus3 */
        PUT_BITS_UEV(ps_bitstrm, u4_log2_min_pcm_coding_block_size - 3, return_status);
        ENTROPY_TRACE(
            "log2_min_pcm_coding_block_size_minus3", u4_log2_min_pcm_coding_block_size - 3);

        /* log2_diff_max_min_pcm_coding_block_size */
        PUT_BITS_UEV(ps_bitstrm, u4_log2_diff_max_min_pcm_coding_block_size, return_status);
        ENTROPY_TRACE(
            "log2_diff_max_min_pcm_coding_block_size", u4_log2_diff_max_min_pcm_coding_block_size);

        /* pcm_loop_filter_disable_flag */
        PUT_BITS(ps_bitstrm, ps_sps->i1_pcm_loop_filter_disable_flag, 1, return_status);
        ENTROPY_TRACE("pcm_loop_filter_disable_flag", ps_sps->i1_pcm_loop_filter_disable_flag);
    }

    /* num_short_term_ref_pic_sets */
    PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_num_short_term_ref_pic_sets, return_status);
    ENTROPY_TRACE("num_short_term_ref_pic_sets", ps_sps->i1_num_short_term_ref_pic_sets);

    for(i = 0; i < ps_sps->i1_num_short_term_ref_pic_sets; i++)
    {
        WORD32 i4_NumPocTotalCurr = 0;
        ihevce_short_term_ref_pic_set(
            ps_bitstrm,
            &ps_sps->as_stref_picset[0],
            ps_sps->i1_num_short_term_ref_pic_sets,
            i,
            &i4_NumPocTotalCurr);
    }

    /* long_term_ref_pics_present_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_long_term_ref_pics_present_flag, 1, return_status);
    ENTROPY_TRACE("long_term_ref_pics_present_flag", ps_sps->i1_long_term_ref_pics_present_flag);

    if(ps_sps->i1_long_term_ref_pics_present_flag)
    {
        /* num_long_term_ref_pics_sps */
        PUT_BITS_UEV(ps_bitstrm, ps_sps->i1_num_long_term_ref_pics_sps, return_status);
        ENTROPY_TRACE("num_long_term_ref_pics_sps", ps_sps->i1_num_long_term_ref_pics_sps);

        for(i = 0; i < ps_sps->i1_num_long_term_ref_pics_sps; i++)
        {
            /* lt_ref_pic_poc_lsb_sps[i] */
            PUT_BITS(
                ps_bitstrm,
                ps_sps->au2_lt_ref_pic_poc_lsb_sps[i],
                u4_log2_max_pic_order_cnt_lsb,
                return_status);
            ENTROPY_TRACE("lt_ref_pic_poc_lsb_sps[i]", ps_sps->au2_lt_ref_pic_poc_lsb_sps[i]);

            /* used_by_curr_pic_lt_sps_flag[i] */
            PUT_BITS(ps_bitstrm, ps_sps->ai1_used_by_curr_pic_lt_sps_flag[i], 1, return_status);
            ENTROPY_TRACE(
                "used_by_curr_pic_lt_sps_flag[i]", ps_sps->ai1_used_by_curr_pic_lt_sps_flag[i]);
        }
    }

    /* sps_temporal_mvp_enable_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_sps_temporal_mvp_enable_flag, 1, return_status);
    ENTROPY_TRACE("sps_temporal_mvp_enable_flag", ps_sps->i1_sps_temporal_mvp_enable_flag);

#if !HM_8DOT1_SYNTAX
    /* strong_intra_smoothing_enable_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_strong_intra_smoothing_enable_flag, 1, return_status);
    ENTROPY_TRACE(
        "sps_strong_intra_smoothing_enable_flag", ps_sps->i1_strong_intra_smoothing_enable_flag);
#endif

    /* vui_parameters_present_flag */
    PUT_BITS(ps_bitstrm, ps_sps->i1_vui_parameters_present_flag, 1, return_status);
    ENTROPY_TRACE("vui_parameters_present_flag", ps_sps->i1_vui_parameters_present_flag);

    ENTROPY_TRACE("----------- vui_parameters -----------", 0);

    if(ps_sps->i1_vui_parameters_present_flag)
    {
        /* Add vui parameters to the bitstream */
        ihevce_generate_vui(ps_bitstrm, ps_sps, ps_sps->s_vui_parameters);
    }

    /* sps_extension_flag */
    PUT_BITS(ps_bitstrm, 0, 1, return_status);
    ENTROPY_TRACE("sps_extension_flag", 0);

    /* rbsp trailing bits */
    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates PPS (Picture Parameter Set)
*
*  @par   Description
*  Generate Picture Parameter Set as per Section 7.3.2.3
*
*  @param[in]   ps_bitstrm
*  pointer to bitstream context (handle)
*
*  @param[in]   ps_pps
*  pointer to structure containing PPS data
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_pps(bitstrm_t *ps_bitstrm, pps_t *ps_pps)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;

    /* Insert the NAL start code */
    return_status = ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(ps_bitstrm, NAL_PPS, 0);

    /* pic_parameter_set_id */
    PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_pps_id, return_status);
    ENTROPY_TRACE("pic_parameter_set_id", ps_pps->i1_pps_id);

    /* seq_parameter_set_id */
    PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_sps_id, return_status);
    ENTROPY_TRACE("seq_parameter_set_id", ps_pps->i1_sps_id);

    /* dependent_slices_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_dependent_slice_enabled_flag, 1, return_status);
    ENTROPY_TRACE("dependent_slices_enabled_flag", ps_pps->i1_dependent_slice_enabled_flag);

    /* output_flag_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_output_flag_present_flag, 1, return_status);
    ENTROPY_TRACE("output_flag_present_flag", ps_pps->i1_output_flag_present_flag);

    /* num_extra_slice_header_bits */
    PUT_BITS(ps_bitstrm, ps_pps->i1_num_extra_slice_header_bits, 3, return_status);
    ENTROPY_TRACE("num_extra_slice_header_bits", ps_pps->i1_num_extra_slice_header_bits);

    /* sign_data_hiding_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_sign_data_hiding_flag, 1, return_status);
    ENTROPY_TRACE("sign_data_hiding_flag", ps_pps->i1_sign_data_hiding_flag);

    /* cabac_init_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_cabac_init_present_flag, 1, return_status);
    ENTROPY_TRACE("cabac_init_present_flag", ps_pps->i1_cabac_init_present_flag);

    /* num_ref_idx_l0_default_active_minus1 */
    PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_num_ref_idx_l0_default_active - 1, return_status);
    ENTROPY_TRACE(
        "num_ref_idx_l0_default_active_minus1", ps_pps->i1_num_ref_idx_l0_default_active - 1);

    /* num_ref_idx_l1_default_active_minus1 */
    PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_num_ref_idx_l1_default_active - 1, return_status);
    ENTROPY_TRACE(
        "num_ref_idx_l1_default_active_minus1", ps_pps->i1_num_ref_idx_l1_default_active - 1);

    /* pic_init_qp_minus26 */
    PUT_BITS_SEV(ps_bitstrm, ps_pps->i1_pic_init_qp - 26, return_status);
    ENTROPY_TRACE("pic_init_qp_minus26", ps_pps->i1_pic_init_qp - 26);

    /* constrained_intra_pred_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_constrained_intra_pred_flag, 1, return_status);
    ENTROPY_TRACE("constrained_intra_pred_flag", ps_pps->i1_constrained_intra_pred_flag);

    /* transform_skip_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_transform_skip_enabled_flag, 1, return_status);
    ENTROPY_TRACE("transform_skip_enabled_flag", ps_pps->i1_transform_skip_enabled_flag);

    /* cu_qp_delta_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_cu_qp_delta_enabled_flag, 1, return_status);
    ENTROPY_TRACE("cu_qp_delta_enabled_flag", ps_pps->i1_cu_qp_delta_enabled_flag);

    if(ps_pps->i1_cu_qp_delta_enabled_flag)
    {
        /* diff_cu_qp_delta_depth */
        PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_diff_cu_qp_delta_depth, return_status);
        ENTROPY_TRACE("diff_cu_qp_delta_depth", ps_pps->i1_diff_cu_qp_delta_depth);
    }

    /* cb_qp_offset */
    PUT_BITS_SEV(ps_bitstrm, ps_pps->i1_pic_cb_qp_offset, return_status);
    ENTROPY_TRACE("cb_qp_offset", ps_pps->i1_pic_cb_qp_offset);

    /* cr_qp_offset */
    PUT_BITS_SEV(ps_bitstrm, ps_pps->i1_pic_cr_qp_offset, return_status);
    ENTROPY_TRACE("cr_qp_offset", ps_pps->i1_pic_cr_qp_offset);

    /* slicelevel_chroma_qp_flag */
    PUT_BITS(
        ps_bitstrm, ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "slicelevel_chroma_qp_flag", ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag);

    /* weighted_pred_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_weighted_pred_flag, 1, return_status);
    ENTROPY_TRACE("weighted_pred_flag", ps_pps->i1_weighted_pred_flag);

    /* weighted_bipred_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_weighted_bipred_flag, 1, return_status);
    ENTROPY_TRACE("weighted_bipred_flag", ps_pps->i1_weighted_bipred_flag);

    /* transquant_bypass_enable_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_transquant_bypass_enable_flag, 1, return_status);
    ENTROPY_TRACE("transquant_bypass_enable_flag", ps_pps->i1_transquant_bypass_enable_flag);

    /* tiles_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_tiles_enabled_flag, 1, return_status);
    ENTROPY_TRACE("tiles_enabled_flag", ps_pps->i1_tiles_enabled_flag);

    /* entropy_coding_sync_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_entropy_coding_sync_enabled_flag, 1, return_status);
    ENTROPY_TRACE("entropy_coding_sync_enabled_flag", ps_pps->i1_entropy_coding_sync_enabled_flag);

    if(ps_pps->i1_tiles_enabled_flag)
    {
        /* num_tile_columns_minus1 */
        PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_num_tile_columns - 1, return_status);
        ENTROPY_TRACE("num_tile_columns_minus1", ps_pps->i1_num_tile_columns - 1);

        /* num_tile_rows_minus1 */
        PUT_BITS_UEV(ps_bitstrm, ps_pps->i1_num_tile_rows - 1, return_status);
        ENTROPY_TRACE("num_tile_rows_minus1", ps_pps->i1_num_tile_rows - 1);

        /* uniform_spacing_flag */
        PUT_BITS(ps_bitstrm, ps_pps->i1_uniform_spacing_flag, 1, return_status);
        ENTROPY_TRACE("uniform_spacing_flag", ps_pps->i1_uniform_spacing_flag);

        if(!ps_pps->i1_uniform_spacing_flag)
        {
            for(i = 0; i < ps_pps->i1_num_tile_columns - 1; i++)
            {
                /* column_width_minus1[i] */
                PUT_BITS_UEV(ps_bitstrm, ps_pps->ps_tile[i].u2_wd - 1, return_status);
                ENTROPY_TRACE("column_width_minus1[i]", ps_pps->ps_tile[i].u2_wd - 1);
            }
            for(i = 0; i < ps_pps->i1_num_tile_rows - 1; i++)
            {
                /* row_height_minus1[i] */
                PUT_BITS_UEV(ps_bitstrm, ps_pps->ps_tile[i].u2_ht - 1, return_status);
                ENTROPY_TRACE("row_height_minus1[i]", ps_pps->ps_tile[i].u2_ht - 1);
            }
        }

        /* loop_filter_across_tiles_enabled_flag */
        PUT_BITS(ps_bitstrm, ps_pps->i1_loop_filter_across_tiles_enabled_flag, 1, return_status);
        ENTROPY_TRACE(
            "loop_filter_across_tiles_enabled_flag",
            ps_pps->i1_loop_filter_across_tiles_enabled_flag);
    }

    /* loop_filter_across_slices_enabled_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_loop_filter_across_slices_enabled_flag, 1, return_status);
    ENTROPY_TRACE(
        "loop_filter_across_slices_enabled_flag",
        ps_pps->i1_loop_filter_across_slices_enabled_flag);

    /* deblocking_filter_control_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_deblocking_filter_control_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "deblocking_filter_control_present_flag",
        ps_pps->i1_deblocking_filter_control_present_flag);

    if(ps_pps->i1_deblocking_filter_control_present_flag)
    {
        /* deblocking_filter_override_enabled_flag */
        PUT_BITS(ps_bitstrm, ps_pps->i1_deblocking_filter_override_enabled_flag, 1, return_status);
        ENTROPY_TRACE(
            "deblocking_filter_override_enabled_flag",
            ps_pps->i1_deblocking_filter_override_enabled_flag);

        /* pic_disable_deblocking_filter_flag */
        PUT_BITS(ps_bitstrm, ps_pps->i1_pic_disable_deblocking_filter_flag, 1, return_status);
        ENTROPY_TRACE(
            "pic_disable_deblocking_filter_flag", ps_pps->i1_pic_disable_deblocking_filter_flag);

        if(!ps_pps->i1_pic_disable_deblocking_filter_flag)
        {
            /* beta_offset_div2 */
            PUT_BITS_SEV(ps_bitstrm, ps_pps->i1_beta_offset_div2 >> 1, return_status);
            ENTROPY_TRACE("beta_offset_div2", ps_pps->i1_beta_offset_div2 >> 1);

            /* tc_offset_div2 */
            PUT_BITS_SEV(ps_bitstrm, ps_pps->i1_tc_offset_div2 >> 1, return_status);
            ENTROPY_TRACE("tc_offset_div2", ps_pps->i1_tc_offset_div2 >> 1);
        }
    }

    /* pps_scaling_list_data_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_pps_scaling_list_data_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "pps_scaling_list_data_present_flag", ps_pps->i1_pps_scaling_list_data_present_flag);

#if 0 /* TODO: Will be enabled once scaling list support is added */
    if(ps_pps->i1_pps_scaling_list_data_present_flag )
    {
        //TODO
        ihevce_scaling_list_data();
    }
#endif

    /* lists_modification_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_lists_modification_present_flag, 1, return_status);
    ENTROPY_TRACE("lists_modification_present_flag", ps_pps->i1_lists_modification_present_flag);

    {
        UWORD32 u4_log2_parallel_merge_level_minus2 = ps_pps->i1_log2_parallel_merge_level;

        u4_log2_parallel_merge_level_minus2 -= 2;

        /* log2_parallel_merge_level_minus2 */
        PUT_BITS_UEV(ps_bitstrm, u4_log2_parallel_merge_level_minus2, return_status);
        ENTROPY_TRACE("log2_parallel_merge_level_minus2", u4_log2_parallel_merge_level_minus2);
    }

    /* slice_header_extension_present_flag */
    PUT_BITS(ps_bitstrm, ps_pps->i1_slice_header_extension_present_flag, 1, return_status);
    ENTROPY_TRACE(
        "slice_header_extension_present_flag", ps_pps->i1_slice_header_extension_present_flag);

    /* pps_extension_flag */
    PUT_BITS(ps_bitstrm, 0, 1, return_status);
    ENTROPY_TRACE("pps_extension_flag", 0);

    ihevce_put_rbsp_trailing_bits(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Generates Slice Header
*
*  @par   Description
*  Generate Slice Header as per Section 7.3.5.1
*
*  @param[inout]   ps_bitstrm
*  pointer to bitstream context for generating slice header
*
*  @param[in]   i1_nal_unit_type
*  nal unit type
*
*  @param[in]   ps_slice_hdr
*  pointer to slice header params
*
*  @param[in]   ps_pps
*  pointer to pps params referred by slice
*
*  @param[in]   ps_sps
*  pointer to sps params referred by slice
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_generate_slice_header(
    bitstrm_t *ps_bitstrm,
    WORD8 i1_nal_unit_type,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    bitstrm_t *ps_dup_bit_strm_ent_offset,
    UWORD32 *pu4_first_slice_start_offset,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 i4_next_slice_seg_x,
    WORD32 i4_next_slice_seg_y)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;

    WORD32 RapPicFlag = (i1_nal_unit_type >= NAL_BLA_W_LP) &&
                        (i1_nal_unit_type <= NAL_RSV_RAP_VCL23);
    WORD32 idr_pic_flag = (NAL_IDR_W_LP == i1_nal_unit_type) || (NAL_IDR_N_LP == i1_nal_unit_type);

    WORD32 disable_deblocking_filter_flag;

    WORD32 i4_NumPocTotalCurr = 0;
    /* Initialize the pic width and pic height from sps parameters */
    WORD32 pic_width = ps_sps->i2_pic_width_in_luma_samples;
    WORD32 pic_height = ps_sps->i2_pic_height_in_luma_samples;

    /* Initialize the CTB size from sps parameters */
    WORD32 log2_ctb_size =
        ps_sps->i1_log2_min_coding_block_size + ps_sps->i1_log2_diff_max_min_coding_block_size;
    WORD32 ctb_size = (1 << log2_ctb_size);

    /* Update ps_slice_hdr->i2_slice_address based on tile position in frame */
    WORD32 num_ctb_in_row = (pic_width + ctb_size - 1) >> log2_ctb_size;

    /* Overwrite i2_slice_address here as pre-enc didn't had tile structure
    available in it's scope. Otherwise i2_slice_address would be set in
    populate_slice_header() itself */
    if(1 == ps_tile_params->i4_tiles_enabled_flag)
    {
        ps_slice_hdr->i2_slice_address =
            ps_tile_params->i4_first_ctb_y * num_ctb_in_row + ps_tile_params->i4_first_ctb_x;
    }
    else
    {
        ps_slice_hdr->i2_slice_address = i4_next_slice_seg_x + i4_next_slice_seg_y * num_ctb_in_row;
    }

    /* Overwrite i1_first_slice_in_pic_flag here as pre-enc didn't had tile structure
    available in it's scope. Otherwise i1_first_slice_in_pic_flag would be set in
    populate_slice_header() itself */
    ps_slice_hdr->i1_first_slice_in_pic_flag = (ps_slice_hdr->i2_slice_address == 0);

    /* Currently if dependent slices are enabled, then all slices
    after first slice of picture, are made dependent slices */
    if((1 == ps_pps->i1_dependent_slice_enabled_flag) &&
       (0 == ps_slice_hdr->i1_first_slice_in_pic_flag))
    {
        ps_slice_hdr->i1_dependent_slice_flag = 1;
    }
    else
    {
        ps_slice_hdr->i1_dependent_slice_flag = 0;
    }

    /* Insert start code */
    return_status |= ihevce_put_nal_start_code_prefix(ps_bitstrm, 1);

    /* Insert Nal Unit Header */
    return_status |= ihevce_generate_nal_unit_header(
        ps_bitstrm,
        i1_nal_unit_type,
        ps_slice_hdr->u4_nuh_temporal_id);  //TEMPORALA_SCALABILITY CHANGES

    /* first_slice_in_pic_flag */
    PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_first_slice_in_pic_flag, 1, return_status);
    ENTROPY_TRACE("first_slice_in_pic_flag", ps_slice_hdr->i1_first_slice_in_pic_flag);

    if(RapPicFlag)
    {
        /* no_output_of_prior_pics_flag */
        PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_no_output_of_prior_pics_flag, 1, return_status);
        ENTROPY_TRACE(
            "no_output_of_prior_pics_flag", ps_slice_hdr->i1_no_output_of_prior_pics_flag);
    }

    /* pic_parameter_set_id */
    PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_pps_id, return_status);
    ENTROPY_TRACE("pic_parameter_set_id", ps_slice_hdr->i1_pps_id);

    /* If ps_pps->i1_dependent_slice_enabled_flag is enabled and
    curent slice is not the first slice of picture then put
    i1_dependent_slice_flag into the bitstream */
    if((ps_pps->i1_dependent_slice_enabled_flag) && (!ps_slice_hdr->i1_first_slice_in_pic_flag))
    {
        /* dependent_slice_flag */
        PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_dependent_slice_flag, 1, return_status);
        ENTROPY_TRACE("dependent_slice_flag", ps_slice_hdr->i1_dependent_slice_flag);
    }

    if(!ps_slice_hdr->i1_first_slice_in_pic_flag)
    {
        WORD32 num_bits;
        WORD32 num_ctb_in_pic;

        /* ctbs in frame ceiled for width / height not multiple of ctb size */
        num_ctb_in_pic = ((pic_width + (ctb_size - 1)) >> log2_ctb_size) *
                         ((pic_height + (ctb_size - 1)) >> log2_ctb_size);

        /* Use CLZ to compute Ceil( Log2( PicSizeInCtbsY ) ) */
        num_bits = 32 - CLZ(num_ctb_in_pic - 1);

        /* slice_address */
        PUT_BITS(ps_bitstrm, ps_slice_hdr->i2_slice_address, num_bits, return_status);
        ENTROPY_TRACE("slice_address", ps_slice_hdr->i2_slice_address);
    }

    if(!ps_slice_hdr->i1_dependent_slice_flag)
    {
        for(i = 0; i < ps_pps->i1_num_extra_slice_header_bits; i++)
        {
            /* slice_reserved_undetermined_flag */
            PUT_BITS(ps_bitstrm, 0, 1, return_status);
            ENTROPY_TRACE("slice_reserved_undetermined_flag", 0);
        }
        /* slice_type */
        PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_slice_type, return_status);
        ENTROPY_TRACE("slice_type", ps_slice_hdr->i1_slice_type);

        if(ps_pps->i1_output_flag_present_flag)
        {
            /* pic_output_flag */
            PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_pic_output_flag, 1, return_status);
            ENTROPY_TRACE("pic_output_flag", ps_slice_hdr->i1_pic_output_flag);
        }

        if(!idr_pic_flag)
        {
            /* pic_order_cnt_lsb */
            PUT_BITS(
                ps_bitstrm,
                ps_slice_hdr->i4_pic_order_cnt_lsb,
                ps_sps->i1_log2_max_pic_order_cnt_lsb,
                return_status);
            ENTROPY_TRACE("pic_order_cnt_lsb", ps_slice_hdr->i4_pic_order_cnt_lsb);

            /* short_term_ref_pic_set_sps_flag */
            PUT_BITS(
                ps_bitstrm, ps_slice_hdr->i1_short_term_ref_pic_set_sps_flag, 1, return_status);
            ENTROPY_TRACE(
                "short_term_ref_pic_set_sps_flag",
                ps_slice_hdr->i1_short_term_ref_pic_set_sps_flag);

            if(!ps_slice_hdr->i1_short_term_ref_pic_set_sps_flag)
            {
                ihevce_short_term_ref_pic_set(
                    ps_bitstrm, &ps_slice_hdr->s_stref_picset, 1, 0, &i4_NumPocTotalCurr);
            }
            else
            {
                WORD32 num_bits = 32 - CLZ(ps_sps->i1_num_short_term_ref_pic_sets);

                /* short_term_ref_pic_set_idx */
                PUT_BITS(
                    ps_bitstrm,
                    ps_slice_hdr->i1_short_term_ref_pic_set_idx,
                    num_bits,
                    return_status);
                ENTROPY_TRACE(
                    "short_term_ref_pic_set_idx", ps_slice_hdr->i1_short_term_ref_pic_set_idx);
            }

            if(ps_sps->i1_long_term_ref_pics_present_flag)
            {
                if(ps_sps->i1_num_long_term_ref_pics_sps > 0)
                {
                    /* num_long_term_sps */
                    PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_num_long_term_sps, return_status);
                    ENTROPY_TRACE("num_long_term_sps", ps_slice_hdr->i1_num_long_term_sps);
                }

                /* num_long_term_pics */
                PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_num_long_term_pics, return_status);
                ENTROPY_TRACE("num_long_term_pics", ps_slice_hdr->i1_num_long_term_pics);

                for(i = 0;
                    i < (ps_slice_hdr->i1_num_long_term_sps + ps_slice_hdr->i1_num_long_term_pics);
                    i++)
                {
                    if(i < ps_slice_hdr->i1_num_long_term_sps)
                    {
                        /* Use CLZ to compute Ceil( Log2
                        ( num_long_term_ref_pics_sps ) ) */
                        WORD32 num_bits = 32 - CLZ(ps_sps->i1_num_long_term_ref_pics_sps);

                        /* lt_idx_sps[i] */
                        PUT_BITS(
                            ps_bitstrm, ps_slice_hdr->ai1_lt_idx_sps[i], num_bits, return_status);
                        ENTROPY_TRACE("lt_idx_sps[i]", ps_slice_hdr->ai1_lt_idx_sps[i]);
                    }
                    else
                    {
                        /* poc_lsb_lt[i] */
                        PUT_BITS(
                            ps_bitstrm,
                            ps_slice_hdr->ai4_poc_lsb_lt[i],
                            ps_sps->i1_log2_max_pic_order_cnt_lsb,
                            return_status);
                        ENTROPY_TRACE("poc_lsb_lt[i]", ps_slice_hdr->ai4_poc_lsb_lt[i]);

                        /* used_by_curr_pic_lt_flag[i] */
                        PUT_BITS(
                            ps_bitstrm,
                            ps_slice_hdr->ai1_used_by_curr_pic_lt_flag[i],
                            1,
                            return_status);
                        ENTROPY_TRACE(
                            "used_by_curr_pic_lt_flag[i]",
                            ps_slice_hdr->ai1_used_by_curr_pic_lt_flag[i]);
                    }

                    /* delta_poc_msb_present_flag[i] */
                    PUT_BITS(
                        ps_bitstrm,
                        ps_slice_hdr->ai1_delta_poc_msb_present_flag[i],
                        1,
                        return_status);
                    ENTROPY_TRACE(
                        "delta_poc_msb_present_flag[i]",
                        ps_slice_hdr->ai1_delta_poc_msb_present_flag[i]);

                    if(ps_slice_hdr->ai1_delta_poc_msb_present_flag[i])
                    {
                        /* delata_poc_msb_cycle_lt[i] */
                        PUT_BITS_UEV(
                            ps_bitstrm, ps_slice_hdr->ai1_delta_poc_msb_cycle_lt[i], return_status);
                        ENTROPY_TRACE(
                            "delata_poc_msb_cycle_lt", ps_slice_hdr->ai1_delta_poc_msb_cycle_lt[i]);
                    }
                }
            }

            if(ps_sps->i1_sps_temporal_mvp_enable_flag)
            {
                /* slice_temporal_mvp_enable_flag */
                PUT_BITS(
                    ps_bitstrm, ps_slice_hdr->i1_slice_temporal_mvp_enable_flag, 1, return_status);
                ENTROPY_TRACE(
                    "slice_temporal_mvp_enable_flag",
                    ps_slice_hdr->i1_slice_temporal_mvp_enable_flag);
            }
        }

        if(ps_sps->i1_sample_adaptive_offset_enabled_flag)
        {
            /* slice_sao_luma_flag */
            PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_slice_sao_luma_flag, 1, return_status);
            ENTROPY_TRACE("slice_sao_luma_flag", ps_slice_hdr->i1_slice_sao_luma_flag);

            /* slice_sao_chroma_flag */
            PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_slice_sao_chroma_flag, 1, return_status);
            ENTROPY_TRACE("slice_sao_chroma_flag", ps_slice_hdr->i1_slice_sao_chroma_flag);
        }
        if((PSLICE == ps_slice_hdr->i1_slice_type) || (BSLICE == ps_slice_hdr->i1_slice_type))
        {
            /* num_ref_idx_active_override_flag */
            PUT_BITS(
                ps_bitstrm, ps_slice_hdr->i1_num_ref_idx_active_override_flag, 1, return_status);
            ENTROPY_TRACE(
                "num_ref_idx_active_override_flag",
                ps_slice_hdr->i1_num_ref_idx_active_override_flag);

            if(ps_slice_hdr->i1_num_ref_idx_active_override_flag)
            {
                /* i1_num_ref_idx_l0_active_minus1 */
                PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_num_ref_idx_l0_active - 1, return_status);
                ENTROPY_TRACE(
                    "i1_num_ref_idx_l0_active_minus1", ps_slice_hdr->i1_num_ref_idx_l0_active - 1);

                if(BSLICE == ps_slice_hdr->i1_slice_type)
                {
                    /* i1_num_ref_idx_l1_active */
                    PUT_BITS_UEV(
                        ps_bitstrm, ps_slice_hdr->i1_num_ref_idx_l1_active - 1, return_status);
                    ENTROPY_TRACE(
                        "i1_num_ref_idx_l1_active", ps_slice_hdr->i1_num_ref_idx_l1_active - 1);
                }
            }

            if(ps_pps->i1_lists_modification_present_flag && i4_NumPocTotalCurr > 1)
            {
                ref_pic_list_modification(ps_bitstrm, ps_slice_hdr, i4_NumPocTotalCurr);
            }

            if(BSLICE == ps_slice_hdr->i1_slice_type)
            {
                /* mvd_l1_zero_flag */
                PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_mvd_l1_zero_flag, 1, return_status);
                ENTROPY_TRACE("mvd_l1_zero_flag", ps_slice_hdr->i1_mvd_l1_zero_flag);
            }

            if(ps_pps->i1_cabac_init_present_flag)
            {
                /* cabac_init_flag */
                PUT_BITS(ps_bitstrm, ps_slice_hdr->i1_cabac_init_flag, 1, return_status);
                ENTROPY_TRACE("cabac_init_flag", ps_slice_hdr->i1_cabac_init_flag);
            }

            if(ps_slice_hdr->i1_slice_temporal_mvp_enable_flag)
            {
                if(BSLICE == ps_slice_hdr->i1_slice_type)
                {
                    /* collocated_from_l0_flag */
                    PUT_BITS(
                        ps_bitstrm, ps_slice_hdr->i1_collocated_from_l0_flag, 1, return_status);
                    ENTROPY_TRACE(
                        "collocated_from_l0_flag", ps_slice_hdr->i1_collocated_from_l0_flag);
                }
                if((ps_slice_hdr->i1_collocated_from_l0_flag &&
                    (ps_slice_hdr->i1_num_ref_idx_l0_active > 1)) ||
                   (!ps_slice_hdr->i1_collocated_from_l0_flag &&
                    (ps_slice_hdr->i1_num_ref_idx_l1_active > 1)))
                {
                    /* collocated_ref_idx */
                    PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_collocated_ref_idx, return_status);
                    ENTROPY_TRACE("collocated_ref_idx", ps_slice_hdr->i1_collocated_ref_idx);
                }
            }

            if((ps_pps->i1_weighted_pred_flag && (PSLICE == ps_slice_hdr->i1_slice_type)) ||
               (ps_pps->i1_weighted_bipred_flag && (BSLICE == ps_slice_hdr->i1_slice_type)))
            {
                ihevce_generate_pred_weight_table(ps_bitstrm, ps_sps, ps_pps, ps_slice_hdr);
            }

#if !HM_8DOT1_SYNTAX
            /* five_minus_max_num_merge_cand */
            PUT_BITS_UEV(ps_bitstrm, 5 - ps_slice_hdr->i1_max_num_merge_cand, return_status);
            ENTROPY_TRACE("five_minus_max_num_merge_cand", 5 - ps_slice_hdr->i1_max_num_merge_cand);
#endif
        }
#if HM_8DOT1_SYNTAX
        /* five_minus_max_num_merge_cand */
        PUT_BITS_UEV(ps_bitstrm, 5 - ps_slice_hdr->i1_max_num_merge_cand, return_status);
        ENTROPY_TRACE("five_minus_max_num_merge_cand", 5 - ps_slice_hdr->i1_max_num_merge_cand);
#endif

        /* slice_qp_delta */
        PUT_BITS_SEV(ps_bitstrm, ps_slice_hdr->i1_slice_qp_delta, return_status);
        ENTROPY_TRACE("slice_qp_delta", ps_slice_hdr->i1_slice_qp_delta);

        if(ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag)
        {
            /* slice_cb_qp_offset */
            PUT_BITS_SEV(ps_bitstrm, ps_slice_hdr->i1_slice_cb_qp_offset, return_status);
            ENTROPY_TRACE("slice_cb_qp_offset", ps_slice_hdr->i1_slice_cb_qp_offset);

            /* slice_cr_qp_offset */
            PUT_BITS_SEV(ps_bitstrm, ps_slice_hdr->i1_slice_cr_qp_offset, return_status);
            ENTROPY_TRACE("slice_cr_qp_offset", ps_slice_hdr->i1_slice_cr_qp_offset);
        }

        if(ps_pps->i1_deblocking_filter_control_present_flag)
        {
            if(ps_pps->i1_deblocking_filter_override_enabled_flag)
            {
                /* deblocking_filter_override_flag */
                PUT_BITS(
                    ps_bitstrm, ps_slice_hdr->i1_deblocking_filter_override_flag, 1, return_status);
                ENTROPY_TRACE(
                    "deblocking_filter_override_flag",
                    ps_slice_hdr->i1_deblocking_filter_override_flag);
            }

            if(ps_slice_hdr->i1_deblocking_filter_override_flag)
            {
                /* slice_disable_deblocking_filter_flag */
                PUT_BITS(
                    ps_bitstrm,
                    ps_slice_hdr->i1_slice_disable_deblocking_filter_flag,
                    1,
                    return_status);
                ENTROPY_TRACE(
                    "slice_disable_deblocking_filter_flag",
                    ps_slice_hdr->i1_slice_disable_deblocking_filter_flag);

                if(!ps_slice_hdr->i1_slice_disable_deblocking_filter_flag)
                {
                    /* beta_offset_div2 */
                    PUT_BITS_SEV(ps_bitstrm, ps_slice_hdr->i1_beta_offset_div2 >> 1, return_status);
                    ENTROPY_TRACE("beta_offset_div2", ps_slice_hdr->i1_beta_offset_div2 >> 1);

                    /* tc_offset_div2 */
                    PUT_BITS_SEV(ps_bitstrm, ps_slice_hdr->i1_tc_offset_div2 >> 1, return_status);
                    ENTROPY_TRACE("tc_offset_div2", ps_slice_hdr->i1_tc_offset_div2 >> 1);
                }
            }
        }

        disable_deblocking_filter_flag = ps_slice_hdr->i1_slice_disable_deblocking_filter_flag |
                                         ps_pps->i1_pic_disable_deblocking_filter_flag;

        if(ps_pps->i1_loop_filter_across_slices_enabled_flag &&
           (ps_slice_hdr->i1_slice_sao_luma_flag || ps_slice_hdr->i1_slice_sao_chroma_flag ||
            !disable_deblocking_filter_flag))
        {
            /* slice_loop_filter_across_slices_enabled_flag */
            PUT_BITS(
                ps_bitstrm,
                ps_slice_hdr->i1_slice_loop_filter_across_slices_enabled_flag,
                1,
                return_status);
            ENTROPY_TRACE(
                "slice_loop_filter_across_slices_enabled_flag",
                ps_slice_hdr->i1_slice_loop_filter_across_slices_enabled_flag);
        }
    }

    if((ps_pps->i1_tiles_enabled_flag) || (ps_pps->i1_entropy_coding_sync_enabled_flag))
    {
        /* num_entry_point_offsets */
        PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i4_num_entry_point_offsets, return_status);
        ENTROPY_TRACE("num_entry_point_offsets", ps_slice_hdr->i4_num_entry_point_offsets);

        /*copy the bitstream state at this stage, later once all the offset are known the duplicated state is used to write offset in bitstream*/
        memcpy(ps_dup_bit_strm_ent_offset, ps_bitstrm, sizeof(bitstrm_t));

        if(ps_slice_hdr->i4_num_entry_point_offsets > 0)
        {
            /* offset_len_minus1 */
            PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_offset_len - 1, return_status);
            ENTROPY_TRACE("offset_len_minus1", ps_slice_hdr->i1_offset_len - 1);

            /*check the bitstream offset here, the first offset will be fixed here based on num_entry_offset and maximum possible emulaiton prevention bytes*/
            /*This offset is used to generate bitstream, In the end of frame processing actual offset are updated and if there was no emulation bits the extra bytes
            shall be filled with 0xFF so that decoder discards it as part of slice header extension*/

            /*assume one byte of emulation preention for every offset we signal*/
            /*considering emulation prevention bytes and assuming incomplete word(4 bytes) that is yet to filled and offset length(4 bytes) that will be calc
            based on max offset length after frame is encoded*/
            pu4_first_slice_start_offset[0] =
                ps_bitstrm->u4_strm_buf_offset +
                ((ps_slice_hdr->i4_num_entry_point_offsets * ps_slice_hdr->i1_offset_len) >> 3) +
                ps_slice_hdr->i4_num_entry_point_offsets + 4 + 4;

            ps_slice_hdr->pu4_entry_point_offset[0] = (*pu4_first_slice_start_offset);

            for(i = 0; i < ps_slice_hdr->i4_num_entry_point_offsets; i++)
            {
                /* entry_point_offset[i] */
                PUT_BITS(
                    ps_bitstrm,
                    ps_slice_hdr->pu4_entry_point_offset[i],
                    ps_slice_hdr->i1_offset_len,
                    return_status);
                ENTROPY_TRACE("entry_point_offset[i]", ps_slice_hdr->pu4_entry_point_offset[i]);
            }
        }
    }

    if(ps_pps->i1_slice_header_extension_present_flag)
    {
        /* slice_header_extension_length */
        PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i2_slice_header_extension_length, return_status);
        ENTROPY_TRACE(
            "slice_header_extension_length", ps_slice_hdr->i2_slice_header_extension_length);

        for(i = 0; i < ps_slice_hdr->i2_slice_header_extension_length; i++)
        {
            /* slice_header_extension_data_byte[i] */
            PUT_BITS(ps_bitstrm, 0, 8, return_status);
            ENTROPY_TRACE("slice_header_extension_data_byte[i]", 0);
        }
    }

    BYTE_ALIGNMENT(ps_bitstrm);

    return return_status;
}

/**
******************************************************************************
*
*  @brief Populates vps structure
*
*  @par   Description
*  All the parameters in vps are currently hard coded
*
*  @param[out]  ps_vps
*  pointer to vps params that needs to be populated
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[in]   ps_out_strm_params
*  pointer to output stream config params
*
*  @param[in]   ps_coding_params
*  pointer to coding params; to enable/disable various toolsets in pps
*
*  @param[in]   ps_config_prms
*  pointer to configuration params like bitrate, HRD buffer sizes, cu, tu sizes
*
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_vps(
    enc_ctxt_t *ps_enc_ctxt,
    vps_t *ps_vps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_resolution_id)
{
    WORD8 *pi1_profile_compatiblity_flags;
    WORD32 i;
    WORD32 i4_field_pic = ps_src_params->i4_field_pic;
    WORD32 i4_codec_level_index;
    ps_vps->i1_vps_id = DEFAULT_VPS_ID;

    (void)ps_config_prms;
    /* default sub layers is 1 */
    ps_vps->i1_vps_max_sub_layers = 1;
    if(1 == ps_stat_cfg_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability)
    {
        ps_vps->i1_vps_max_sub_layers = 2;
    }

    for(i = 0; i < ps_vps->i1_vps_max_sub_layers; i++)
    {
        /* currently bit rate and pic rate signalling is disabled */
        ps_vps->ai1_bit_rate_info_present_flag[i] = 0;
        ps_vps->ai1_pic_rate_info_present_flag[i] = 0;

        if(ps_vps->ai1_bit_rate_info_present_flag[i])
        {
            /* TODO: Add support for bitrate and max bitrate */
            ps_vps->au2_avg_bit_rate[i] = 0;
            ps_vps->au2_max_bit_rate[i] = 0;
        }

        if(ps_vps->ai1_pic_rate_info_present_flag[i])
        {
            /* TODO: Add support for pic rate idc and avg pic rate */
        }
    }

    /* default sub layer ordering info present flag */
    ps_vps->i1_sub_layer_ordering_info_present_flag = VPS_SUB_LAYER_ORDERING_INFO_ABSENT;

    /* hrd and temporal id nesting not supported for now */
    ps_vps->i1_vps_num_hrd_parameters = 0;

    if(ps_vps->i1_vps_max_sub_layers == 1)
    {
        ps_vps->i1_vps_temporal_id_nesting_flag = 1;
    }
    else
    {
        ps_vps->i1_vps_temporal_id_nesting_flag = 0;
    }

    /* populate the general profile, tier and level information */
    ps_vps->s_ptl.s_ptl_gen.i1_profile_space = 0;  // BLU_RAY specific change is default

    /* set the profile according to user input */
    ps_vps->s_ptl.s_ptl_gen.i1_profile_idc = ps_out_strm_params->i4_codec_profile;

    /***************************************************************/
    /* set the profile compatibility flag for current profile to 1 */
    /* the rest of the flags are set to 0                          */
    /***************************************************************/
    pi1_profile_compatiblity_flags = &ps_vps->s_ptl.s_ptl_gen.ai1_profile_compatibility_flag[0];

    for(i = 0; i < ps_vps->i1_vps_max_sub_layers; i++)  //TEMPORALA_SCALABILITY CHANGES
    {
        ps_vps->ai1_vps_max_dec_pic_buffering[i] =
            ps_coding_params->i4_max_reference_frames + (2 << i4_field_pic) - 1;

        ps_vps->ai1_vps_max_num_reorder_pics[i] = ps_coding_params->i4_max_temporal_layers
                                                  << i4_field_pic;

        ps_vps->ai1_vps_max_latency_increase[i] = 0;

        ps_vps->s_ptl.ai1_sub_layer_level_present_flag[i] = 1;  //TEMPORALA_SCALABILITY CHANGES

        ps_vps->s_ptl.ai1_sub_layer_profile_present_flag[i] = 0;  //TEMPORALA_SCALABILITY CHANGES

        ps_vps->s_ptl.as_ptl_sub[i].i1_profile_space = 0;  // BLU_RAY specific change is default

        ps_vps->s_ptl.as_ptl_sub[i].i1_profile_idc = ps_out_strm_params->i4_codec_profile;

        memset(
            ps_vps->s_ptl.as_ptl_sub[i].ai1_profile_compatibility_flag,
            0,
            MAX_PROFILE_COMPATBLTY * sizeof(WORD8));

        ps_vps->s_ptl.as_ptl_sub[i]
            .ai1_profile_compatibility_flag[ps_out_strm_params->i4_codec_profile] = 1;

        ps_vps->s_ptl.as_ptl_sub[i].u1_level_idc =
            ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level;

        if(0 == i)  // Only one level temporal scalability suport has been added.
        {
            i4_codec_level_index = ihevce_get_level_index(
                ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level);

            if(i4_codec_level_index)
                i4_codec_level_index -= 1;

            ps_vps->s_ptl.as_ptl_sub[i].u1_level_idc =
                (WORD32)g_as_level_data[i4_codec_level_index].e_level;
        }

        ps_vps->s_ptl.as_ptl_sub[i].i1_tier_flag = ps_out_strm_params->i4_codec_tier;

        if(ps_src_params->i4_field_pic == IV_PROGRESSIVE)
        {
            ps_vps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 1;

            ps_vps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 0;
        }
        else if(ps_src_params->i4_field_pic == IV_INTERLACED)
        {
            ps_vps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 0;

            ps_vps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 1;
        }
        else if(ps_src_params->i4_field_pic == IV_CONTENTTYPE_NA)
        {
            ps_vps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 0;

            ps_vps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 0;
        }

        ps_vps->s_ptl.as_ptl_sub[i].i1_general_non_packed_constraint_flag =
            DEFAULT_NON_PACKED_CONSTRAINT_FLAG;

        if(ps_enc_ctxt->i4_blu_ray_spec == 1)
        {
            ps_vps->s_ptl.as_ptl_sub[i].i1_frame_only_constraint_flag = 1;
        }
        else
        {
            ps_vps->s_ptl.as_ptl_sub[i].i1_frame_only_constraint_flag =
                DEFAULT_FRAME_ONLY_CONSTRAINT_FLAG;
        }
    }

    memset(pi1_profile_compatiblity_flags, 0, MAX_PROFILE_COMPATBLTY);
    pi1_profile_compatiblity_flags[ps_out_strm_params->i4_codec_profile] = 1;

    /* set the level idc according to user input */
    ps_vps->s_ptl.s_ptl_gen.u1_level_idc =
        ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level;

    ps_vps->s_ptl.s_ptl_gen.i1_tier_flag = ps_out_strm_params->i4_codec_tier;

    if(ps_src_params->i4_field_pic == IV_PROGRESSIVE)
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 0;
    }
    else if(ps_src_params->i4_field_pic == IV_INTERLACED)
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 1;
    }
    else if(ps_src_params->i4_field_pic == IV_CONTENTTYPE_NA)
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 0;
    }

    ps_vps->s_ptl.s_ptl_gen.i1_general_non_packed_constraint_flag =
        DEFAULT_NON_PACKED_CONSTRAINT_FLAG;

    if(ps_enc_ctxt->i4_blu_ray_spec == 1)
    {
        ps_vps->s_ptl.s_ptl_gen.i1_frame_only_constraint_flag = 1;
    }
    else
    {
        ps_vps->s_ptl.s_ptl_gen.i1_frame_only_constraint_flag = DEFAULT_FRAME_ONLY_CONSTRAINT_FLAG;
    }
    if((ps_out_strm_params->i4_codec_profile == 4) &&
       (ps_src_params->i4_chr_format == IV_YUV_420SP_UV))
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 1;
    }
    else if(
        (ps_out_strm_params->i4_codec_profile == 4) &&
        (ps_src_params->i4_chr_format == IV_YUV_422SP_UV))
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 1;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 1;
    }
    else
    {
        ps_vps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_vps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 0;
    }

    ps_vps->i1_vps_max_nuh_reserved_zero_layer_id = 0;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates sps structure
*
*  @par   Description
*  Populates sps structure for its use in header generation
*
*  @param[out]  ps_sps
*  pointer to sps params that needs to be populated
*
*  @param[in]   ps_vps
*  pointer to vps params referred by the sps
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[in]   ps_out_strm_params
*  pointer to output stream config params
*
*  @param[in]   ps_coding_params
*  pointer to coding params; to enable/disable various toolsets in pps
*
*  @param[in]   ps_config_prms
*  pointer to configuration params like bitrate, HRD buffer sizes, cu, tu sizes
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_sps(
    enc_ctxt_t *ps_enc_ctxt,
    sps_t *ps_sps,
    vps_t *ps_vps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_resolution_id)
{
    WORD32 i;
    WORD32 i4_field_pic = ps_src_params->i4_field_pic;
    WORD32 i4_quality_preset =
        ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset;
    WORD32 i4_codec_level_index;

    if(i4_quality_preset == IHEVCE_QUALITY_P7)
    {
        i4_quality_preset = IHEVCE_QUALITY_P6;
    }

    ps_sps->i1_sps_id = DEFAULT_SPS_ID;

    if(1 == ps_stat_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out)
    {
        ps_sps->i1_sps_id = i4_resolution_id;
    }

    ps_sps->i1_vps_id = ps_vps->i1_vps_id;

    ps_sps->i2_pic_height_in_luma_samples = ps_frm_ctb_prms->i4_cu_aligned_pic_ht;

    ps_sps->i2_pic_width_in_luma_samples = ps_frm_ctb_prms->i4_cu_aligned_pic_wd;

    ps_sps->i1_amp_enabled_flag = AMP_ENABLED;

    ps_sps->i1_chroma_format_idc = (ps_src_params->i4_chr_format == IV_YUV_422SP_UV) ? 2 : 1;

    ps_sps->i1_separate_colour_plane_flag = 0;

    ps_sps->i1_bit_depth_luma_minus8 = ps_stat_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8;

    ps_sps->i1_bit_depth_chroma_minus8 = ps_stat_cfg_prms->s_tgt_lyr_prms.i4_internal_bit_depth - 8;

    ps_sps->i1_log2_min_coding_block_size = ps_config_prms->i4_min_log2_cu_size;

    ps_sps->i1_log2_diff_max_min_coding_block_size =
        ps_config_prms->i4_max_log2_cu_size - ps_config_prms->i4_min_log2_cu_size;

    ps_sps->i1_log2_ctb_size =
        ps_sps->i1_log2_min_coding_block_size + ps_sps->i1_log2_diff_max_min_coding_block_size;

    ps_sps->i1_log2_diff_max_min_transform_block_size =
        ps_config_prms->i4_max_log2_tu_size - ps_config_prms->i4_min_log2_tu_size;

    ps_sps->i1_log2_min_transform_block_size = ps_config_prms->i4_min_log2_tu_size;

    ps_sps->i1_long_term_ref_pics_present_flag = LONG_TERM_REF_PICS_ABSENT;

    ps_sps->i1_max_transform_hierarchy_depth_inter = ps_config_prms->i4_max_tr_tree_depth_nI;

    ps_sps->i1_max_transform_hierarchy_depth_intra = ps_config_prms->i4_max_tr_tree_depth_I;

    ps_sps->i1_pcm_enabled_flag = PCM_DISABLED;

    ps_sps->i1_pcm_loop_filter_disable_flag = PCM_LOOP_FILTER_DISABLED;

    ps_sps->i1_pic_cropping_flag = !!ps_coding_params->i4_cropping_mode;

    if(i4_quality_preset < IHEVCE_QUALITY_P4)
    {
        /*** Enable SAO for PQ,HQ,MS presets **/
        ps_sps->i1_sample_adaptive_offset_enabled_flag = SAO_ENABLED;
    }
    else
    {
        ps_sps->i1_sample_adaptive_offset_enabled_flag = SAO_DISABLED;
    }
#if DISABLE_SAO
    ps_sps->i1_sample_adaptive_offset_enabled_flag = SAO_DISABLED;
#endif

    if(ps_coding_params->i4_use_default_sc_mtx == 1)
    {
        ps_sps->i1_scaling_list_enable_flag = SCALING_LIST_ENABLED;
    }
    else
    {
        ps_sps->i1_scaling_list_enable_flag = SCALING_LIST_DISABLED;
    }

    ps_sps->i1_sps_max_sub_layers = DEFAULT_SPS_MAX_SUB_LAYERS;

    if(1 == ps_stat_cfg_prms->s_tgt_lyr_prms.i4_enable_temporal_scalability)
    {
        ps_sps->i1_sps_max_sub_layers = DEFAULT_SPS_MAX_SUB_LAYERS + 1;
    }

    ps_sps->i1_sps_sub_layer_ordering_info_present_flag = SPS_SUB_LAYER_ORDERING_INFO_ABSENT;

    ps_sps->i1_sps_scaling_list_data_present_flag = SCALING_LIST_DATA_ABSENT;

    if(ps_sps->i1_sps_max_sub_layers == 1)
    {
        ps_sps->i1_sps_temporal_id_nesting_flag = 1;  //NO_SPS_TEMPORAL_ID_NESTING_DONE;
    }
    else
    {
        ps_sps->i1_sps_temporal_id_nesting_flag = 0;  //NO_SPS_TEMPORAL_ID_NESTING_DONE;
    }

    /* short term and long term ref pic set not signalled in sps */
    ps_sps->i1_num_short_term_ref_pic_sets = 0;
    ps_sps->i1_long_term_ref_pics_present_flag = 0;

    ps_sps->i1_num_long_term_ref_pics_sps = 0;
    ps_sps->i1_sps_temporal_mvp_enable_flag = !DISABLE_TMVP;

    ps_sps->i1_strong_intra_smoothing_enable_flag = STRONG_INTRA_SMOOTHING_FLAG_ENABLE;

    ps_sps->i1_vui_parameters_present_flag = ps_out_strm_params->i4_vui_enable;

    /*required in generation of slice header*/
    ps_sps->i2_pic_ht_in_ctb = ps_frm_ctb_prms->i4_num_ctbs_vert;

    ps_sps->i2_pic_wd_in_ctb = ps_frm_ctb_prms->i4_num_ctbs_horz;

    ps_sps->i1_log2_max_pic_order_cnt_lsb = DEFAULT_LOG2_MAX_POC_LSB;

    if(ps_sps->i1_pic_cropping_flag)
    {
        WORD32 num_rows_to_pad_bottom =
            ps_sps->i2_pic_height_in_luma_samples - ps_stat_cfg_prms->s_src_prms.i4_orig_height;
        WORD32 num_rows_to_pad_right =
            ps_sps->i2_pic_width_in_luma_samples - ps_stat_cfg_prms->s_src_prms.i4_orig_width;

        ps_sps->i2_pic_crop_top_offset = DEFAULT_PIC_CROP_TOP_OFFSET;

        ps_sps->i2_pic_crop_left_offset = DEFAULT_PIC_CROP_LEFT_OFFSET;

        /* picture offsets should be signalled in terms of chroma unit */
        ps_sps->i2_pic_crop_bottom_offset = num_rows_to_pad_bottom >> 1;

        /* picture offsets should be signalled in terms of chroma unit */
        ps_sps->i2_pic_crop_right_offset = num_rows_to_pad_right >> 1;
    }

    for(i = 0; i < (ps_sps->i1_sps_max_sub_layers); i++)
    {
        ps_sps->ai1_sps_max_dec_pic_buffering[i] =
            ps_coding_params->i4_max_reference_frames + (2 << i4_field_pic) - 1;

        ps_sps->ai1_sps_max_num_reorder_pics[i] = ps_coding_params->i4_max_temporal_layers
                                                  << i4_field_pic;

        ps_sps->ai1_sps_max_latency_increase[i] = 0;

        ps_sps->s_ptl.ai1_sub_layer_level_present_flag[i] = 1;  //TEMPORALA_SCALABILITY CHANGES

        ps_sps->s_ptl.ai1_sub_layer_profile_present_flag[i] = 0;  //TEMPORALA_SCALABILITY CHANGES

        ps_sps->s_ptl.as_ptl_sub[i].i1_profile_space = 0;  // BLU_RAY specific change is default

        ps_sps->s_ptl.as_ptl_sub[i].i1_profile_idc = ps_out_strm_params->i4_codec_profile;

        memset(
            ps_sps->s_ptl.as_ptl_sub[i].ai1_profile_compatibility_flag,
            0,
            MAX_PROFILE_COMPATBLTY * sizeof(WORD8));

        ps_sps->s_ptl.as_ptl_sub[i]
            .ai1_profile_compatibility_flag[ps_out_strm_params->i4_codec_profile] = 1;

        ps_sps->s_ptl.as_ptl_sub[i].u1_level_idc =
            ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level;

        if(0 == i)  // Only one level temporal scalability suport has been added.
        {
            i4_codec_level_index = ihevce_get_level_index(
                ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level);

            if(i4_codec_level_index)
                i4_codec_level_index -= 1;

            ps_sps->s_ptl.as_ptl_sub[i].u1_level_idc =
                (WORD32)g_as_level_data[i4_codec_level_index].e_level;
        }
        ps_sps->s_ptl.as_ptl_sub[i].i1_tier_flag = ps_out_strm_params->i4_codec_tier;

        if(ps_src_params->i4_field_pic == IV_PROGRESSIVE)
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 0;
        }
        else if(ps_src_params->i4_field_pic == IV_INTERLACED)
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 1;
        }
        else if(ps_src_params->i4_field_pic == IV_CONTENTTYPE_NA)
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_progressive_source_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_interlaced_source_flag = 0;
        }

        ps_sps->s_ptl.as_ptl_sub[i].i1_general_non_packed_constraint_flag =
            DEFAULT_NON_PACKED_CONSTRAINT_FLAG;

        if(ps_enc_ctxt->i4_blu_ray_spec == 1)
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_frame_only_constraint_flag = 1;
        }
        else
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_frame_only_constraint_flag =
                DEFAULT_FRAME_ONLY_CONSTRAINT_FLAG;
        }
        if((ps_out_strm_params->i4_codec_profile == 4) && (ps_sps->i1_chroma_format_idc == 1))
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_12bit_constraint_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_10bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_8bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_422chroma_constraint_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_420chroma_constraint_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_monochrome_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_intra_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_one_picture_only_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_lower_bit_rate_constraint_flag = 1;
        }
        else if((ps_out_strm_params->i4_codec_profile == 4) && (ps_sps->i1_chroma_format_idc == 2))
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_12bit_constraint_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_10bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_8bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_422chroma_constraint_flag = 1;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_420chroma_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_monochrome_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_intra_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_one_picture_only_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_lower_bit_rate_constraint_flag = 1;
        }
        else
        {
            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_12bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_10bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_8bit_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_422chroma_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_420chroma_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_max_monochrome_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_intra_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_one_picture_only_constraint_flag = 0;

            ps_sps->s_ptl.as_ptl_sub[i].i1_general_lower_bit_rate_constraint_flag = 0;
        }
    }

    memset(
        ps_sps->s_ptl.s_ptl_gen.ai1_profile_compatibility_flag,
        0,
        MAX_PROFILE_COMPATBLTY * sizeof(WORD8));

    /* populate the general profile, tier and level information */
    ps_sps->s_ptl.s_ptl_gen.i1_profile_space = 0;  // BLU_RAY specific change is default

    ps_sps->s_ptl.s_ptl_gen.i1_profile_idc = ps_out_strm_params->i4_codec_profile;

    ps_sps->s_ptl.s_ptl_gen.ai1_profile_compatibility_flag[ps_out_strm_params->i4_codec_profile] =
        1;

    ps_sps->s_ptl.s_ptl_gen.u1_level_idc =
        ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_codec_level;

    ps_sps->s_ptl.s_ptl_gen.i1_tier_flag = ps_out_strm_params->i4_codec_tier;

    if(ps_src_params->i4_field_pic == IV_PROGRESSIVE)
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 0;
    }
    else if(ps_src_params->i4_field_pic == IV_INTERLACED)
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 1;
    }
    else if(ps_src_params->i4_field_pic == IV_CONTENTTYPE_NA)
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_progressive_source_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_interlaced_source_flag = 0;
    }

    ps_sps->s_ptl.s_ptl_gen.i1_general_non_packed_constraint_flag =
        DEFAULT_NON_PACKED_CONSTRAINT_FLAG;

    if(ps_enc_ctxt->i4_blu_ray_spec == 1)
    {
        ps_sps->s_ptl.s_ptl_gen.i1_frame_only_constraint_flag = 1;
    }
    else
    {
        ps_sps->s_ptl.s_ptl_gen.i1_frame_only_constraint_flag = DEFAULT_FRAME_ONLY_CONSTRAINT_FLAG;
    }
    if((ps_out_strm_params->i4_codec_profile == 4) && (ps_sps->i1_chroma_format_idc == 1))
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 1;
    }
    else if((ps_out_strm_params->i4_codec_profile == 4) && (ps_sps->i1_chroma_format_idc == 2))
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 1;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 1;
    }
    else
    {
        ps_sps->s_ptl.s_ptl_gen.i1_general_max_12bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_10bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_8bit_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_422chroma_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_420chroma_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_max_monochrome_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_intra_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_one_picture_only_constraint_flag = 0;

        ps_sps->s_ptl.s_ptl_gen.i1_general_lower_bit_rate_constraint_flag = 0;
    }

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates pps structure based on input cofiguration params
*
*  @par   Description
*  Populates pps structure for its use in header generation
*
*  @param[out]  ps_pps
*  pointer to pps params structure which needs to be populated
*
*  @param[in]   ps_sps
*  pointer to sps params refered by the pps
*
*  @param[in]   ps_src_params
*  pointer to source config params; resolution, frame rate etc
*
*  @param[in]   ps_out_strm_params
*  pointer to output stream config params
*
*  @param[in]   ps_coding_params
*  pointer to coding params; to enable/disable various toolsets in pps
*
*  @param[in]   ps_config_prms
*  pointer to configuration params like bitrate, HRD buffer sizes, cu, tu sizes
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_pps(
    pps_t *ps_pps,
    sps_t *ps_sps,
    ihevce_src_params_t *ps_src_params,
    ihevce_out_strm_params_t *ps_out_strm_params,
    ihevce_coding_params_t *ps_coding_params,
    ihevce_config_prms_t *ps_config_prms,
    ihevce_static_cfg_params_t *ps_stat_cfg_prms,
    WORD32 i4_bitrate_instance_id,
    WORD32 i4_resolution_id,
    ihevce_tile_params_t *ps_tile_params_base,
    WORD32 *pi4_column_width_array,
    WORD32 *pi4_row_height_array)
{
    (void)ps_src_params;
    (void)ps_out_strm_params;

    ps_pps->i1_beta_offset_div2 = DEFAULT_BETA_OFFSET;

    ps_pps->i1_cabac_init_present_flag = CABAC_INIT_ABSENT;

    ps_pps->i1_constrained_intra_pred_flag = CONSTR_IPRED_DISABLED;
    /*delta qp can be disabled for constant qp mode to save on qp signalling bits*/
    ps_pps->i1_cu_qp_delta_enabled_flag = ps_config_prms->i4_cu_level_rc;

    ps_pps->i1_deblocking_filter_control_present_flag = DEBLOCKING_FILTER_CONTROL_PRESENT;

    ps_pps->i1_deblocking_filter_override_enabled_flag = DEBLOCKING_FILTER_OVERRIDE_DISABLED;

    ps_pps->i1_pic_disable_deblocking_filter_flag = ps_coding_params->i4_deblocking_type;

    if(0 != ps_stat_cfg_prms->s_slice_params.i4_slice_segment_mode)
    {
        ps_pps->i1_dependent_slice_enabled_flag = DEPENDENT_SLICE_ENABLED;
    }
    else
    {
        ps_pps->i1_dependent_slice_enabled_flag = DEPENDENT_SLICE_DISABLED;
    }

    /* Assign the diff_cu_qp_delta_depth with 3,2,1 for making
    CU_LEVEL_QP_MODULATION limited to 8x8, 16x16, 32x32 respectively : Lokesh */
    ps_pps->i1_diff_cu_qp_delta_depth = CU_LEVEL_QP_LIMIT_8x8;

    if(1 == ps_coding_params->i4_enable_entropy_sync)
    {
        ps_pps->i1_entropy_coding_sync_enabled_flag = ENTROPY_CODING_SYNC_ENABLED;
    }
    else
    {
        ps_pps->i1_entropy_coding_sync_enabled_flag = ENTROPY_CODING_SYNC_DISABLED;
    }

    ps_pps->i1_entropy_slice_enabled_flag = ENTROPY_SLICE_DISABLED;

    ps_pps->i1_lists_modification_present_flag = ps_coding_params->i4_weighted_pred_enable;

    ps_pps->i1_log2_parallel_merge_level = DEFAULT_PARALLEL_MERGE_LEVEL;

    ps_pps->i1_num_extra_slice_header_bits = 0;

    /* SAO_note_01: Currently SAO is implemented is such a way that the
    loop-filter has to be enabled across syntatical-tiles and slices.
    Search for <SAO_note_01> in workspace to know more */
    ps_pps->i1_loop_filter_across_slices_enabled_flag = LF_ACROSS_SLICES_ENABLED;

    ps_pps->i1_num_ref_idx_l0_default_active = DEFAULT_NUM_REF_IDX_L0_DEFAULT_ACTIVE;

    ps_pps->i1_num_ref_idx_l1_default_active = DEFAULT_NUM_REF_IDX_L1_DEFAULT_ACTIVE;

    if(0 == ps_tile_params_base->i4_tiles_enabled_flag)
    {
        ps_pps->i1_num_tile_columns = NUM_TILES_COLS;

        ps_pps->i1_num_tile_rows = NUM_TILES_ROWS;

        ps_pps->i1_tiles_enabled_flag = TILES_DISABLED;

        ps_pps->i1_uniform_spacing_flag = SPACING_IS_UNIFORM;
    }
    else
    {
        ps_pps->i1_num_tile_columns = ps_tile_params_base->i4_num_tile_cols;

        ps_pps->i1_num_tile_rows = ps_tile_params_base->i4_num_tile_rows;

        ps_pps->i1_tiles_enabled_flag = TILES_ENABLED;

        ps_pps->i1_uniform_spacing_flag = ps_tile_params_base->i4_uniform_spacing_flag;

        if(SPACING_IS_NONUNIFORM == ps_pps->i1_uniform_spacing_flag)
        {
            WORD32 i4_i;
            for(i4_i = 0; i4_i < ps_tile_params_base->i4_num_tile_cols; i4_i++)
            {
                ps_pps->ps_tile[i4_i].u2_wd = pi4_column_width_array[i4_i] >>
                                              ps_config_prms->i4_max_log2_cu_size;
            }
            for(i4_i = 0; i4_i < ps_tile_params_base->i4_num_tile_rows; i4_i++)
            {
                ps_pps->ps_tile[i4_i].u2_ht = pi4_row_height_array[i4_i] >>
                                              ps_config_prms->i4_max_log2_cu_size;
            }
        }
    }

    /* SAO_note_01: Currently SAO is implemented is such a way that the
    loop-filter has to be enabled across syntatical-tiles and slices.
    Search for <SAO_note_01> in workspace to know more */
    if(0 == ps_tile_params_base->i4_tiles_enabled_flag)
    {
        ps_pps->i1_loop_filter_across_tiles_enabled_flag = 1;
    }
    else
    {
        ps_pps->i1_loop_filter_across_tiles_enabled_flag = 0;
    }

    ps_pps->i1_output_flag_present_flag = OUTPUT_FLAG_ABSENT;

    ps_pps->i1_pic_cb_qp_offset = DEFAULT_PIC_CB_QP_OFFSET;

    ps_pps->i1_pic_cr_qp_offset = DEFAULT_PIC_CR_QP_OFFSET;

    /*init qp is different for each bit-rate instance */
    ps_pps->i1_pic_init_qp = CLIP3(
        ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id]
            .ai4_frame_qp[i4_bitrate_instance_id],
        ps_config_prms->i4_min_frame_qp,
        ps_config_prms->i4_max_frame_qp);

    /* enable chroma QP offset only if stasino or psy rd is present */
    if(((ps_coding_params->i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_CONTROL_TOGGLER)) &&
        ((ps_coding_params->i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_NOISE_PRESERVATION)) ||
         (ps_coding_params->i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_1)) ||
         (ps_coding_params->i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_2)) ||
         (ps_coding_params->i4_vqet & (1 << BITPOS_IN_VQ_TOGGLE_FOR_ENABLING_PSYRDOPT_3)))))
    {
        ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag =
            SLICE_LEVEL_CHROMA_QP_OFFSETS_PRESENT;
    }
    else
    {
        ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag =
            SLICE_LEVEL_CHROMA_QP_OFFSETS_ABSENT;
    }

    ps_pps->i1_pps_id = DEFAULT_PPS_ID;

    if(1 == ps_stat_cfg_prms->s_tgt_lyr_prms.i4_mres_single_out)
    {
        ps_pps->i1_pps_id = i4_resolution_id;
    }

    ps_pps->i1_pps_scaling_list_data_present_flag = SCALING_LIST_DATA_ABSENT;

    if(ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset <
       IHEVCE_QUALITY_P3)
    {
        ps_pps->i1_sign_data_hiding_flag = SIGN_DATA_HIDDEN;
    }
    else if(
        ps_stat_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_quality_preset ==
        IHEVCE_QUALITY_P3)
    {
        ps_pps->i1_sign_data_hiding_flag = SIGN_DATA_UNHIDDEN;
    }
    else
    {
        ps_pps->i1_sign_data_hiding_flag = SIGN_DATA_UNHIDDEN;
    }

#if DISABLE_SBH
    ps_pps->i1_sign_data_hiding_flag = SIGN_DATA_UNHIDDEN;
#endif

    ps_pps->i1_slice_extension_present_flag = SLICE_EXTENSION_ABSENT;

    ps_pps->i1_slice_header_extension_present_flag = SLICE_HEADER_EXTENSION_ABSENT;

    ps_pps->i1_sps_id = ps_sps->i1_sps_id;

    ps_pps->i1_tc_offset_div2 = DEFAULT_TC_OFFSET;

    ps_pps->i1_transform_skip_enabled_flag = TRANSFORM_SKIP_DISABLED;

    ps_pps->i1_transquant_bypass_enable_flag = TRANSFORM_BYPASS_DISABLED;

    ps_pps->i1_weighted_bipred_flag = ps_coding_params->i4_weighted_pred_enable;

    ps_pps->i1_weighted_pred_flag = ps_coding_params->i4_weighted_pred_enable;

    return IHEVCE_SUCCESS;
}

/**
******************************************************************************
*
*  @brief Populates slice header structure
*
*  @par   Description
*  Populates slice header structure for its use in header generation
*
*  @param[out]  ps_slice_hdr
*  pointer to slice header structure that needs to be populated
*
*  @param[in]  ps_pps
*  pointer to pps params structure refered by the slice
*
*  @param[in]   ps_sps
*  pointer to sps params refered by the pps
*
*  @param[in]   nal_unit_type
*  nal unit type for current slice
*
*  @param[in]   slice_type
*  current slice type
*
*  @param[in]   ctb_x
*  x offset of first ctb in current slice (ctb units)
*
*  @param[in]   ctb_y
*  y offset of first ctb in current slice (ctb units)
*
*  @param[in]   poc
*  pic order count for current slice (shall be 0 for IDR pics)
*
*  @param[in]   cur_slice_qp
*  qp for the current slice
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_populate_slice_header(
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    WORD32 nal_unit_type,
    WORD32 slice_type,
    WORD32 ctb_x,
    WORD32 ctb_y,
    WORD32 poc,
    WORD32 cur_slice_qp,
    WORD32 max_merge_candidates,
    WORD32 i4_rc_pass_num,
    WORD32 i4_quality_preset,
    WORD32 stasino_enabled)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    WORD32 RapPicFlag = (nal_unit_type >= NAL_BLA_W_LP) && (nal_unit_type <= NAL_RSV_RAP_VCL23);

    WORD32 idr_pic_flag = (NAL_IDR_W_LP == nal_unit_type) || (NAL_IDR_N_LP == nal_unit_type);

    WORD32 disable_deblocking_filter_flag;

    (void)ctb_x;
    (void)ctb_y;
    /* first_slice_in_pic_flag  */
    if(i4_quality_preset == IHEVCE_QUALITY_P7)
    {
        i4_quality_preset = IHEVCE_QUALITY_P6;
    }

    if(RapPicFlag)
    {
        /* no_output_of_prior_pics_flag */ /* TODO:revisit this */
        ps_slice_hdr->i1_no_output_of_prior_pics_flag = 0;  //BLU_RAY specific already done
    }

    /* pic_parameter_set_id */
    ps_slice_hdr->i1_pps_id = ps_pps->i1_pps_id;

    {
        /* This i1_dependent_slice_flag will further be updated in generate_slice_header() function */
        ps_slice_hdr->i1_dependent_slice_flag = 0;
    }

    if(!ps_slice_hdr->i1_dependent_slice_flag)
    {
        /* slice_type */
        ps_slice_hdr->i1_slice_type = (WORD8)slice_type;

        if(ps_pps->i1_output_flag_present_flag)
        {
            /* pic_output_flag */ /* TODO:revisit this */
            ps_slice_hdr->i1_pic_output_flag = 0;
        }

        /* separate colour plane flag not supported in this encoder */
        ASSERT(0 == ps_sps->i1_separate_colour_plane_flag);

        if(!idr_pic_flag)
        {
            WORD32 log2_max_poc_lsb = ps_sps->i1_log2_max_pic_order_cnt_lsb;

            /* pic_order_cnt_lsb */
            ps_slice_hdr->i4_pic_order_cnt_lsb = poc & ((1 << log2_max_poc_lsb) - 1);

            /* short_term_ref_pic_set_sps_flag */
            /* TODO : revisit this */
            ps_slice_hdr->i1_short_term_ref_pic_set_sps_flag = 0;

            if(!ps_slice_hdr->i1_short_term_ref_pic_set_sps_flag)
            {
                /* TODO: To populate short term ref pic set for this slice   */
            }

            /* long term ref pic flag not supported */
            ASSERT(0 == ps_sps->i1_long_term_ref_pics_present_flag);
            if(ps_sps->i1_long_term_ref_pics_present_flag)
            {
                /* TODO : not supported */
            }
        }

        //ASSERT(0 == ps_sps->i1_sample_adaptive_offset_enabled_flag);
        if(ps_sps->i1_sample_adaptive_offset_enabled_flag)
        {
            /* slice_sao_luma_flag */
            ps_slice_hdr->i1_slice_sao_luma_flag = 1;
            ps_slice_hdr->i1_slice_sao_chroma_flag = 1;
        }

#if DISABLE_LUMA_SAO
        ps_slice_hdr->i1_slice_sao_luma_flag = 0;
#endif

#if DISABLE_CHROMA_SAO
        ps_slice_hdr->i1_slice_sao_chroma_flag = 0;
#endif
        if((PSLICE == ps_slice_hdr->i1_slice_type) || (BSLICE == ps_slice_hdr->i1_slice_type))
        {
            /* TODO: currently temporal mvp disabled, need to enable later */
            if(1 == ps_sps->i1_sps_temporal_mvp_enable_flag)
            {
                ps_slice_hdr->i1_slice_temporal_mvp_enable_flag = 1;
            }
            else
            {
                ps_slice_hdr->i1_slice_temporal_mvp_enable_flag = 0;
            }

            /* num_ref_idx_active_override_flag */
            ps_slice_hdr->i1_num_ref_idx_active_override_flag = 0;

            if(ps_slice_hdr->i1_num_ref_idx_active_override_flag)
            {
                /* TODO revisit this*/
                /* i1_num_ref_idx_l0_active_minus1 */
                ps_slice_hdr->i1_num_ref_idx_l0_active = 1;

                if(BSLICE == ps_slice_hdr->i1_slice_type)
                {
                    /* i1_num_ref_idx_l1_active */
                    /* TODO revisit this*/
                    ps_slice_hdr->i1_num_ref_idx_l1_active = 1;
                }
            }

            if(BSLICE == ps_slice_hdr->i1_slice_type)
            {
                /* mvd_l1_zero_flag */
                ps_slice_hdr->i1_mvd_l1_zero_flag = 0;
            }

            {
                /* cabac_init_flag curently set to 0 */
                ps_slice_hdr->i1_cabac_init_flag = ps_pps->i1_cabac_init_present_flag ? 1 : 0;
            }

            if(ps_slice_hdr->i1_slice_temporal_mvp_enable_flag)
            {
                if(BSLICE == ps_slice_hdr->i1_slice_type)
                {
                    /* collocated_from_l0_flag */
                    ps_slice_hdr->i1_collocated_from_l0_flag = 0;
                }
                else if(PSLICE == ps_slice_hdr->i1_slice_type)
                {
                    ps_slice_hdr->i1_collocated_from_l0_flag = 1;
                }

                if((ps_slice_hdr->i1_collocated_from_l0_flag &&
                    (ps_slice_hdr->i1_num_ref_idx_l0_active > 1)) ||
                   (!ps_slice_hdr->i1_collocated_from_l0_flag &&
                    (ps_slice_hdr->i1_num_ref_idx_l1_active > 1)))
                {
                    /* collocated_ref_idx */
                    /* TODO revisit this*/
                    ps_slice_hdr->i1_collocated_ref_idx = 0;
                    //ps_slice_hdr->i1_num_ref_idx_l1_active - 1;
                }
            }
        }
        ps_slice_hdr->i1_max_num_merge_cand = max_merge_candidates;

        /* TODO : revisit this */
        ps_slice_hdr->i1_slice_qp_delta = (WORD8)cur_slice_qp - ps_pps->i1_pic_init_qp;

        if(!ps_pps->i1_pic_slice_level_chroma_qp_offsets_present_flag || !stasino_enabled)
        {
            /* slice_cb_qp_offset */
            ps_slice_hdr->i1_slice_cb_qp_offset = 0;

            /* slice_cr_qp_offset */
            ps_slice_hdr->i1_slice_cr_qp_offset = 0;
        }
        else /* only noisy regions have lower Chroma QP rating */
        {
            ps_slice_hdr->i1_slice_cb_qp_offset = -2;
            ps_slice_hdr->i1_slice_cr_qp_offset = -2;
        }

        if(ps_pps->i1_deblocking_filter_control_present_flag)
        {
            ps_slice_hdr->i1_deblocking_filter_override_flag = 0;

            if(ps_pps->i1_deblocking_filter_override_enabled_flag)
            {
                /* deblocking_filter_override_flag */
                ps_slice_hdr->i1_deblocking_filter_override_flag = 0;
            }

            if(ps_slice_hdr->i1_deblocking_filter_override_flag)
            {
                /* slice_disable_deblocking_filter_flag */
                ps_slice_hdr->i1_slice_disable_deblocking_filter_flag = DISABLE_DEBLOCKING_FLAG;

                if(!ps_slice_hdr->i1_slice_disable_deblocking_filter_flag)
                {
                    /* beta_offset_div2 */
                    ps_slice_hdr->i1_beta_offset_div2 = 0;

                    /* tc_offset_div2 */
                    ps_slice_hdr->i1_tc_offset_div2 = 0;
                }
            }
        }

        disable_deblocking_filter_flag = ps_slice_hdr->i1_slice_disable_deblocking_filter_flag |
                                         ps_pps->i1_pic_disable_deblocking_filter_flag;

        if(ps_pps->i1_loop_filter_across_slices_enabled_flag &&
           (ps_slice_hdr->i1_slice_sao_luma_flag || ps_slice_hdr->i1_slice_sao_chroma_flag ||
            !disable_deblocking_filter_flag))
        {
            /* slice_loop_filter_across_slices_enabled_flag */
            ps_slice_hdr->i1_slice_loop_filter_across_slices_enabled_flag = 1;
        }
    }

    if(1 == ps_pps->i1_entropy_coding_sync_enabled_flag)
    {
        /* num_entry_point_offsets, same as NUM of ctb rows to enable entropy sync at start of every CTB */
        ps_slice_hdr->i4_num_entry_point_offsets = ps_sps->i2_pic_ht_in_ctb - 1;

        if(ps_slice_hdr->i4_num_entry_point_offsets > 0)
        {
            /* generate offset_len here */
            /* fixing the offset lenght assuming 4kx2k is log2(w * h / num_ctb_row) = 20*/
            ps_slice_hdr->i1_offset_len = 24;
        }
    }
    else
    {
        ps_slice_hdr->i4_num_entry_point_offsets = 0;
        ps_slice_hdr->i1_offset_len = 0;
    }

    /* slice_header_extension_present_flag not supported */
    //if(ps_pps->i1_slice_header_extension_present_flag)
    {
        /* slice_header_extension_length */
        ps_slice_hdr->i2_slice_header_extension_length = 0;

        for(i = 0; i < ps_slice_hdr->i2_slice_header_extension_length; i++)
        {
            /* slice_header_extension_data_byte[i] */
        }
    }

    /* TODO : hard coding ref pix set for now                           */
    /* Need to update this once the ref pics are known from lap output  */

    /*  NOTE                                                            */
    /* inter ref pic prediction is too much of logic for few bit savings*/
    /* at slice header level this is not supported by the encoder       */
    ps_slice_hdr->s_stref_picset.i1_inter_ref_pic_set_prediction_flag = 0;

    /* hardcoding 1 ref pic for now ..... will be updated base on lap output */
    ps_slice_hdr->s_stref_picset.i1_num_delta_pocs = 1;
    ps_slice_hdr->s_stref_picset.i1_num_neg_pics = 1;
    ps_slice_hdr->s_stref_picset.i1_num_pos_pics = 0;

    memset(
        ps_slice_hdr->s_stref_picset.ai2_delta_poc,
        0,
        MAX_DPB_SIZE * sizeof(*ps_slice_hdr->s_stref_picset.ai2_delta_poc));
    ps_slice_hdr->s_stref_picset.ai2_delta_poc[0] = 1;

    return return_status;
}

/**
******************************************************************************
*
*  @brief Insert entry point offset
*
*  @par   Description
*  Insert entry point offset in slice header after frame processing is done 7.3.5.1
*
*  @param[inout]   ps_bitstrm
*  pointer to bitstream context for generating slice header
*
*  @param[in]   i1_nal_unit_type
*  nal unit type
*
*  @param[in]   ps_slice_hdr
*  pointer to slice header params
*
*  @param[in]   ps_pps
*  pointer to pps params referred by slice
*
*  @param[in]   ps_sps
*  pointer to sps params referred by slice
*
*  @return      success or failure error code
*
******************************************************************************
*/
WORD32 ihevce_insert_entry_offset_slice_header(
    bitstrm_t *ps_bitstrm,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    UWORD32 u4_first_slice_start_offset)
{
    WORD32 i;
    WORD32 return_status = IHEVCE_SUCCESS;
    UWORD32 max_offset = 0, offset_len = 0, num_bytes_shift = 0;
    /*entire slice data has to be shifted*/
    num_bytes_shift =
        ps_slice_hdr->pu4_entry_point_offset[ps_slice_hdr->i4_num_entry_point_offsets + 1] -
        ps_slice_hdr->pu4_entry_point_offset[0];
    /*generate relative offset*/
    for(i = 0; i < ps_slice_hdr->i4_num_entry_point_offsets; i++)
    {
        ps_slice_hdr->pu4_entry_point_offset[i] =
            ps_slice_hdr->pu4_entry_point_offset[i + 1] - ps_slice_hdr->pu4_entry_point_offset[i];
        if(ps_slice_hdr->pu4_entry_point_offset[i] > (WORD32)max_offset)
        {
            max_offset = ps_slice_hdr->pu4_entry_point_offset[i];
        }
    }
    while(1)
    {
        if(max_offset & 0x80000000)
        {
            break;
        }
        max_offset <<= 1;
        offset_len++;
    }
    offset_len = 32 - offset_len;
    ps_slice_hdr->i1_offset_len = offset_len;

    if(ps_slice_hdr->i4_num_entry_point_offsets > 0)
    {
        /* offset_len_minus1 */
        PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i1_offset_len - 1, return_status);
        ENTROPY_TRACE("offset_len_minus1", ps_slice_hdr->i1_offset_len - 1);
    }

    for(i = 0; i < ps_slice_hdr->i4_num_entry_point_offsets; i++)
    {
        /* entry_point_offset[i] */
        /* entry point offset minus1 is indicated in 10.0 */
        PUT_BITS(
            ps_bitstrm,
            ps_slice_hdr->pu4_entry_point_offset[i] - 1,
            ps_slice_hdr->i1_offset_len,
            return_status);
        ENTROPY_TRACE("entry_point_offset[i]", ps_slice_hdr->pu4_entry_point_offset[i]);
    }

    if(ps_pps->i1_slice_header_extension_present_flag)
    {
        /* slice_header_extension_length */
        PUT_BITS_UEV(ps_bitstrm, ps_slice_hdr->i2_slice_header_extension_length, return_status);
        ENTROPY_TRACE(
            "slice_header_extension_length", ps_slice_hdr->i2_slice_header_extension_length);
        /*calculate slice header extension length to fill in the gap*/

        for(i = 0; i < ps_slice_hdr->i2_slice_header_extension_length; i++)
        {
            /* slice_header_extension_data_byte[i] */
            PUT_BITS(ps_bitstrm, 0xFF, 8, return_status);
            ENTROPY_TRACE("slice_header_extension_data_byte[i]", 0);
        }
    }

    BYTE_ALIGNMENT(ps_bitstrm);

    ASSERT(num_bytes_shift > 0);
    /* copy the bitstream to point where header data has ended*/
    memmove(
        (UWORD8 *)(ps_bitstrm->pu1_strm_buffer + ps_bitstrm->u4_strm_buf_offset),
        (UWORD8 *)(ps_bitstrm->pu1_strm_buffer + u4_first_slice_start_offset),
        num_bytes_shift);

    /*send feedback of actual bytes generated*/
    ps_bitstrm->u4_strm_buf_offset += num_bytes_shift;

    //ASSERT(ps_bitstrm->u4_strm_buf_offset == u4_first_slice_start_offset);
    return return_status;
}
