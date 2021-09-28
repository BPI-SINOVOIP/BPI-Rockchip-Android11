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
* \file ihevce_tu_tree_selector.c
*
* \brief
*    Functions that facilitate selection of optimal TU tree
*
* \date
*    20/04/2016
*
* \author
*    Ittiam
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
#include "ihevce_enc_loop_utils.h"
#include "ihevce_tu_tree_selector.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_tu_tree_coverage_in_cu \endif
*
* \brief
*    Determination of the area within the CU that is swept by the TU tree.
*    Input : Pointer to a node of the TU tree
*    Output : Area covered by the current TU or its children
*
*****************************************************************************
*/
WORD32 ihevce_tu_tree_coverage_in_cu(tu_tree_node_t *ps_node)
{
    WORD32 i4_tu_tree_area = 0;

    if(ps_node->u1_is_valid_node)
    {
        i4_tu_tree_area += ps_node->s_luma_data.u1_size * ps_node->s_luma_data.u1_size;
    }
    else
    {
        if(NULL != ps_node->ps_child_node_tl)
        {
            i4_tu_tree_area += ihevce_tu_tree_coverage_in_cu(ps_node->ps_child_node_tl);
        }

        if(NULL != ps_node->ps_child_node_tr)
        {
            i4_tu_tree_area += ihevce_tu_tree_coverage_in_cu(ps_node->ps_child_node_tr);
        }

        if(NULL != ps_node->ps_child_node_bl)
        {
            i4_tu_tree_area += ihevce_tu_tree_coverage_in_cu(ps_node->ps_child_node_bl);
        }

        if(NULL != ps_node->ps_child_node_br)
        {
            i4_tu_tree_area += ihevce_tu_tree_coverage_in_cu(ps_node->ps_child_node_br);
        }
    }

    return i4_tu_tree_area;
}

static void ihevce_tu_node_data_init(
    tu_node_data_t *ps_tu_data, UWORD8 u1_size, UWORD8 u1_posx, UWORD8 u1_posy)
{
    ps_tu_data->u1_size = u1_size;
    ps_tu_data->i8_ssd = 0;
    ps_tu_data->i8_cost = 0;
#if ENABLE_INTER_ZCU_COST
    ps_tu_data->i8_not_coded_cost = 0;
#endif
    ps_tu_data->u4_sad = 0;
    ps_tu_data->i4_bits = 0;
    ps_tu_data->i4_num_bytes_used_for_ecd = 0;
    ps_tu_data->u1_cbf = 0;
    ps_tu_data->u1_reconBufId = UCHAR_MAX;
    ps_tu_data->u1_posx = u1_posx;
    ps_tu_data->u1_posy = u1_posy;
}

/*!
******************************************************************************
* \if Function name : ihevce_tu_node_init \endif
*
* \brief
*    This function initialises all nodes of the TU tree from the root upto and
*    including the nodes at the max tree depth. Only those nodes that lie
*    within the (max + 1) and (min - 1) depths are set as valid. Everything
*    else is invalid. The pointers to the children nodes of the leaf-most
*    nodes in the tree are assigned NULL.
*    Input : Pointer to root of the tree containing TU info.
*    Output : The memory of this node and all its progeny shall be modified
*    returns Number of nodes of the TU tree that have been modified
*
*****************************************************************************
*/
static UWORD16 ihevce_tu_node_init(
    tu_tree_node_t *ps_root,
    UWORD8 u1_size,
    UWORD8 u1_parent_posx,
    UWORD8 u1_parent_posy,
    UWORD8 u1_cur_depth,
    UWORD8 u1_min_tree_depth,
    UWORD8 u1_max_tree_depth,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422,
    TU_POS_T e_tu_pos)
{
    tu_tree_node_t *ps_node;
    tu_tree_node_t *ps_childNodeTL;
    tu_tree_node_t *ps_childNodeTR;
    tu_tree_node_t *ps_childNodeBL;
    tu_tree_node_t *ps_childNodeBR;

    UWORD8 u1_start_index_for_parent = 0;
    UWORD8 u1_start_index_for_child = 0;
    UWORD16 u2_parent_offset = 0;
    UWORD16 u2_child_offset = 0;
    UWORD8 u1_posx = 0;
    UWORD8 u1_posy = 0;

    const UWORD8 u1_nxn_tu_node_start_index = 0;
    const UWORD8 u1_nBye2xnBye2_tu_node_start_index = 1;
    const UWORD8 u1_nBye4xnBye4_tu_node_start_index = 1 + 4;
    const UWORD8 u1_nBye8xnBye8_tu_node_start_index = 1 + 4 + 16;
    const UWORD8 u1_nBye16xnBye16_tu_node_start_index = 1 + 4 + 16 + 64;
    UWORD16 u2_num_nodes_initialised = 0;

    ASSERT(u1_cur_depth <= u1_max_tree_depth);
    ASSERT(u1_max_tree_depth >= u1_min_tree_depth);

    switch(e_tu_pos)
    {
    case POS_TL:
    {
        u1_posx = u1_parent_posx;
        u1_posy = u1_parent_posy;

        break;
    }
    case POS_TR:
    {
        u1_posx = u1_parent_posx + u1_size;
        u1_posy = u1_parent_posy;

        break;
    }
    case POS_BL:
    {
        u1_posx = u1_parent_posx;
        u1_posy = u1_parent_posy + u1_size;

        break;
    }
    case POS_BR:
    {
        u1_posx = u1_parent_posx + u1_size;
        u1_posy = u1_parent_posy + u1_size;

        break;
    }
    default:
    {
        /* Here be dragons */
        ASSERT(0);
    }
    }

    switch(u1_cur_depth)
    {
    case 0:
    {
        u1_start_index_for_parent = u1_nxn_tu_node_start_index;
        u1_start_index_for_child = u1_nBye2xnBye2_tu_node_start_index;

        u2_parent_offset = 0;
        u2_child_offset = 0;

        break;
    }
    case 1:
    {
        u1_start_index_for_parent = u1_nBye2xnBye2_tu_node_start_index;
        u1_start_index_for_child = u1_nBye4xnBye4_tu_node_start_index;

        u2_parent_offset = e_tu_pos;
        u2_child_offset = 4 * u1_posx / u1_size + 8 * u1_posy / u1_size;

        break;
    }
    case 2:
    {
        u1_start_index_for_parent = u1_nBye4xnBye4_tu_node_start_index;
        u1_start_index_for_child = u1_nBye8xnBye8_tu_node_start_index;

        u2_parent_offset = 2 * u1_parent_posx / u1_size + 4 * u1_parent_posy / u1_size + e_tu_pos;
        u2_child_offset = 4 * u1_posx / u1_size + 16 * u1_posy / u1_size;

        break;
    }
    case 3:
    {
        u1_start_index_for_parent = u1_nBye8xnBye8_tu_node_start_index;
        u1_start_index_for_child = u1_nBye16xnBye16_tu_node_start_index;

        u2_parent_offset = 2 * u1_parent_posx / u1_size + 8 * u1_parent_posy / u1_size + e_tu_pos;
        u2_child_offset = 4 * u1_posx / u1_size + 32 * u1_posy / u1_size;

        break;
    }
    case 4:
    {
        u1_start_index_for_parent = u1_nBye16xnBye16_tu_node_start_index;
        u1_start_index_for_child = 0;

        u2_parent_offset = 2 * u1_parent_posx / u1_size + 16 * u1_parent_posy / u1_size + e_tu_pos;
        u2_child_offset = 0;

        break;
    }
    default:
    {
        /* Here be dragons */
        ASSERT(0);
    }
    }

    ASSERT((u1_start_index_for_parent + u2_parent_offset) < (256 + 64 + 16 + 4 + 1));
    ASSERT((u1_start_index_for_child + u2_child_offset + POS_BR) < (256 + 64 + 16 + 4 + 1));

    ps_node = ps_root + u1_start_index_for_parent + u2_parent_offset;
    ps_childNodeTL = ps_root + u1_start_index_for_child + u2_child_offset + POS_TL;
    ps_childNodeTR = ps_root + u1_start_index_for_child + u2_child_offset + POS_TR;
    ps_childNodeBL = ps_root + u1_start_index_for_child + u2_child_offset + POS_BL;
    ps_childNodeBR = ps_root + u1_start_index_for_child + u2_child_offset + POS_BR;

    ihevce_tu_node_data_init(&ps_node->s_luma_data, u1_size, u1_posx, u1_posy);

    if(u1_chroma_processing_enabled)
    {
        UWORD8 i;

        if(u1_size > 4)
        {
            for(i = 0; i < (u1_is_422 + 1); i++)
            {
                ihevce_tu_node_data_init(
                    &ps_node->as_cb_data[i],
                    u1_size / 2,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size / 2);

                ihevce_tu_node_data_init(
                    &ps_node->as_cr_data[i],
                    u1_size / 2,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size / 2);
            }
        }
        else if(POS_TL == e_tu_pos)
        {
            for(i = 0; i < (u1_is_422 + 1); i++)
            {
                ihevce_tu_node_data_init(
                    &ps_node->as_cb_data[i],
                    u1_size,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size);

                ihevce_tu_node_data_init(
                    &ps_node->as_cr_data[i],
                    u1_size,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size);
            }
        }
        else
        {
            for(i = 0; i < (u1_is_422 + 1); i++)
            {
                ihevce_tu_node_data_init(
                    &ps_node->as_cb_data[i],
                    u1_size / 2,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size);

                ihevce_tu_node_data_init(
                    &ps_node->as_cr_data[i],
                    u1_size / 2,
                    u1_posx / 2,
                    !u1_is_422 ? u1_posy / 2 : u1_posy + i * u1_size);
            }
        }
    }

    if((u1_cur_depth >= u1_min_tree_depth) && (u1_cur_depth <= u1_max_tree_depth))
    {
        ps_node->u1_is_valid_node = 1;
    }
    else
    {
        ps_node->u1_is_valid_node = 0;
    }

    u2_num_nodes_initialised++;

    if((u1_cur_depth < u1_max_tree_depth) && (u1_size > MIN_TU_SIZE))
    {
        ps_node->ps_child_node_tl = ps_childNodeTL;
        ps_node->ps_child_node_tr = ps_childNodeTR;
        ps_node->ps_child_node_bl = ps_childNodeBL;
        ps_node->ps_child_node_br = ps_childNodeBR;

        u2_num_nodes_initialised += ihevce_tu_node_init(
            ps_root,
            u1_size / 2,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            u1_cur_depth + 1,
            u1_min_tree_depth,
            u1_max_tree_depth,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_TL);

        u2_num_nodes_initialised += ihevce_tu_node_init(
            ps_root,
            u1_size / 2,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            u1_cur_depth + 1,
            u1_min_tree_depth,
            u1_max_tree_depth,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_TR);

        u2_num_nodes_initialised += ihevce_tu_node_init(
            ps_root,
            u1_size / 2,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            u1_cur_depth + 1,
            u1_min_tree_depth,
            u1_max_tree_depth,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_BL);

        u2_num_nodes_initialised += ihevce_tu_node_init(
            ps_root,
            u1_size / 2,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            u1_cur_depth + 1,
            u1_min_tree_depth,
            u1_max_tree_depth,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_BR);
    }
    else
    {
        ps_node->ps_child_node_tl = NULL;
        ps_node->ps_child_node_tr = NULL;
        ps_node->ps_child_node_bl = NULL;
        ps_node->ps_child_node_br = NULL;
    }

    return u2_num_nodes_initialised;
}

/*!
******************************************************************************
* \if Function name : ihevce_tu_tree_init \endif
*
* \brief
*    Initialises all relevant data within all nodes for a specified TU tree
*    Input : Pointer to root of the tree containing TU info.
*    Output : Returns the number of nodes initialised
*
*****************************************************************************
*/
UWORD16 ihevce_tu_tree_init(
    tu_tree_node_t *ps_root,
    UWORD8 u1_cu_size,
    UWORD8 u1_min_tree_depth,
    UWORD8 u1_max_tree_depth,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422)
{
    UWORD16 u2_num_nodes = 0;

    ASSERT(u1_max_tree_depth >= u1_min_tree_depth);

    u2_num_nodes += ihevce_tu_node_init(
        ps_root,
        u1_cu_size,
        0,
        0,
        0,
        u1_min_tree_depth,
        u1_max_tree_depth,
        u1_chroma_processing_enabled,
        u1_is_422,
        POS_TL);

    return u2_num_nodes;
}

/*!
******************************************************************************
* \if Function name : ihevce_cabac_bins2Bits_converter_and_state_updater \endif
*
* \brief
*    cabac bin to bits converter
*    Input : 1. Pointer to buffer which stores the current CABAC state. This
*    buffer shall be modified by this function. 2. Index to the cabac state
*    that corresponds to the bin. 3. bin value
*    Output : Number of bits required to encode the bin
*
*****************************************************************************
*/
static INLINE UWORD32 ihevce_cabac_bins2Bits_converter_and_state_updater(
    UWORD8 *pu1_cabac_ctxt, UWORD8 u1_cabac_state_idx, UWORD8 u1_bin_value)
{
    UWORD32 u4_bits = 0;

    u4_bits += gau2_ihevce_cabac_bin_to_bits[pu1_cabac_ctxt[u1_cabac_state_idx] ^ u1_bin_value];
    pu1_cabac_ctxt[u1_cabac_state_idx] =
        gau1_ihevc_next_state[(pu1_cabac_ctxt[u1_cabac_state_idx] << 1) | u1_bin_value];

    return u4_bits;
}

static tu_tree_node_t *
    ihevce_tu_node_parent_finder(tu_tree_node_t *ps_root, tu_tree_node_t *ps_leaf)
{
    UWORD8 u1_depth_of_leaf;

    GETRANGE(u1_depth_of_leaf, ps_root->s_luma_data.u1_size / ps_leaf->s_luma_data.u1_size);
    u1_depth_of_leaf--;

    if(0 == u1_depth_of_leaf)
    {
        return NULL;
    }
    else if(1 == u1_depth_of_leaf)
    {
        return ps_root;
    }
    else
    {
        UWORD8 u1_switch_conditional =
            (ps_leaf->s_luma_data.u1_posx >= ps_root->ps_child_node_tl->s_luma_data.u1_size) +
            (ps_leaf->s_luma_data.u1_posy >= ps_root->ps_child_node_tl->s_luma_data.u1_size) * 2;

        ASSERT(NULL != ps_root->ps_child_node_tl);
        ASSERT(NULL != ps_root->ps_child_node_tr);
        ASSERT(NULL != ps_root->ps_child_node_bl);
        ASSERT(NULL != ps_root->ps_child_node_br);

        switch(u1_switch_conditional)
        {
        case 0:
        {
            return ihevce_tu_node_parent_finder(ps_root->ps_child_node_tl, ps_leaf);
        }
        case 1:
        {
            return ihevce_tu_node_parent_finder(ps_root->ps_child_node_tr, ps_leaf);
        }
        case 2:
        {
            return ihevce_tu_node_parent_finder(ps_root->ps_child_node_bl, ps_leaf);
        }
        case 3:
        {
            return ihevce_tu_node_parent_finder(ps_root->ps_child_node_br, ps_leaf);
        }
        }
    }

    return NULL;
}

/*!
******************************************************************************
* \if Function name : ihevce_compute_bits_for_TUSplit_and_cbf \endif
*
* \notes
*    1. This function ought to be called before the call to 'ihevce_tu_tree_selector'
*    of children TU's in order to determine bits to encode splitFlag as 1.
*    This should also be called at the end of 'ihevce_tu_processor' in order
*    to determine bits required to encode cbf and splitFlag.
*    2. When 'ENABLE_TOP_DOWN_TU_RECURSION' = 0 and 'INCLUDE_CHROMA_DURING_TU_RECURSION' = 1,
*    it shall be assumed that parent chroma cbf is 1.
*    3. When 'INCLUDE_CHROMA_DURING_TU_RECURSION' = 0, this function works as
*    though no chroma related syntax was included in the HEVC syntax for coding
*    the transform tree
*    Input : 1. ps_root: Pointer to root of the tree containing TU info
*            2. ps_leaf: Pointer to current node of the TU tree
*            3. pu1_cabac_ctxt: Pointer to buffer which stores the current CABAC
*            state. This buffer shall be modified by this function
*    Output : Number of bits required to encode cbf and splitFlags
*
*****************************************************************************
*/
static WORD32 ihevce_compute_bits_for_TUSplit_and_cbf(
    tu_tree_node_t *ps_root,
    tu_tree_node_t *ps_leaf,
    UWORD8 *pu1_cabac_ctxt,
    UWORD8 u1_max_tu_size,
    UWORD8 u1_min_tu_size,
    UWORD8 u1_cur_depth,
    UWORD8 u1_max_depth,
    UWORD8 u1_is_intra,
    UWORD8 u1_is_intra_nxn_pu,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422)
{
    UWORD8 u1_cabac_state_idx;
    UWORD8 u1_log2_tu_size;

    UWORD32 u4_num_bits = 0;
    UWORD8 u1_tu_size = ps_leaf->s_luma_data.u1_size;

    ASSERT(u1_min_tu_size >= MIN_TU_SIZE);
    ASSERT(u1_min_tu_size <= u1_max_tu_size);
    ASSERT(u1_max_tu_size <= MAX_TU_SIZE);
    ASSERT(u1_tu_size >= MIN_TU_SIZE);
    ASSERT(u1_tu_size <= MAX_TU_SIZE);
    ASSERT(u1_cur_depth <= u1_max_depth);

    GETRANGE(u1_log2_tu_size, u1_tu_size);

    if((ps_root->s_luma_data.u1_size >> u1_cur_depth) == u1_tu_size)
    {
        if((u1_tu_size <= u1_max_tu_size) && (u1_tu_size > u1_min_tu_size) &&
           (u1_cur_depth < u1_max_depth) && !(u1_is_intra_nxn_pu && !u1_cur_depth))
        {
            u1_cabac_state_idx = IHEVC_CAB_SPLIT_TFM + (5 - u1_log2_tu_size);
            u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                pu1_cabac_ctxt, u1_cabac_state_idx, 0);
        }

        if(u1_chroma_processing_enabled && (u1_tu_size > 4))
        {
            tu_tree_node_t *ps_parent = ihevce_tu_node_parent_finder(ps_root, ps_leaf);

            u1_cabac_state_idx = IHEVC_CAB_CBCR_IDX + u1_cur_depth;

            if(!u1_cur_depth || ps_parent->as_cb_data[0].u1_cbf || ps_parent->as_cb_data[1].u1_cbf)
            {
                if(u1_is_422)
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cb_data[0].u1_cbf);

                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cb_data[1].u1_cbf);
                }
                else
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cb_data[0].u1_cbf);
                }
            }

            if(!u1_cur_depth || ps_parent->as_cr_data[0].u1_cbf || ps_parent->as_cr_data[1].u1_cbf)
            {
                if(u1_is_422)
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cr_data[0].u1_cbf);

                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cr_data[1].u1_cbf);
                }
                else
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cr_data[0].u1_cbf);
                }
            }
        }

        if(u1_is_intra || u1_cur_depth)
        {
            u1_cabac_state_idx = IHEVC_CAB_CBF_LUMA_IDX + !u1_cur_depth;
            u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->s_luma_data.u1_cbf);
        }
    }
    else
    {
        if((u1_tu_size <= u1_max_tu_size) && (u1_tu_size > u1_min_tu_size) &&
           (u1_cur_depth < u1_max_depth) && !(u1_is_intra_nxn_pu && !u1_cur_depth))
        {
            u1_cabac_state_idx = IHEVC_CAB_SPLIT_TFM + (5 - u1_log2_tu_size);
            u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                pu1_cabac_ctxt, u1_cabac_state_idx, 1);
        }

        if(u1_chroma_processing_enabled && (u1_tu_size > 4))
        {
            tu_tree_node_t *ps_parent = ihevce_tu_node_parent_finder(ps_root, ps_leaf);

            u1_cabac_state_idx = IHEVC_CAB_CBCR_IDX + u1_cur_depth;

            if(!u1_cur_depth || ps_parent->as_cb_data[0].u1_cbf || ps_parent->as_cb_data[1].u1_cbf)
            {
                if(u1_is_422 && (8 == u1_tu_size))
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cb_data[0].u1_cbf);

                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cb_data[1].u1_cbf);
                }
                else
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt,
                        u1_cabac_state_idx,
                        ps_leaf->as_cb_data[0].u1_cbf || ps_leaf->as_cb_data[1].u1_cbf);
                }
            }

            if(!u1_cur_depth || ps_parent->as_cr_data[0].u1_cbf || ps_parent->as_cr_data[1].u1_cbf)
            {
                if(u1_is_422 && (8 == u1_tu_size))
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cr_data[0].u1_cbf);

                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt, u1_cabac_state_idx, ps_leaf->as_cr_data[1].u1_cbf);
                }
                else
                {
                    u4_num_bits += ihevce_cabac_bins2Bits_converter_and_state_updater(
                        pu1_cabac_ctxt,
                        u1_cabac_state_idx,
                        ps_leaf->as_cr_data[0].u1_cbf || ps_leaf->as_cr_data[1].u1_cbf);
                }
            }
        }
    }

    return u4_num_bits;
}

/*!
******************************************************************************
* \if Function name : ihevce_tu_processor \endif
*
* \notes
*    Input : 1. ps_ctxt: Pointer to enc-loop's context. Parts of this structure
*            shall be modified by this function. They include, au1_cu_csbf,
*            i8_cu_not_coded_cost, ai2_scratch and s_rdoq_sbh_ctxt
*            2. ps_node: Pointer to current node of the TU tree. This struct
*            shall be modified by this function
*            3. pv_src: Pointer to buffer which stores the source
*            4. pv_pred: Pointer to buffer which stores the pred
*            5. pv_recon: Pointer to buffer which stores the recon
*            This buffer shall be modified by this function
*            6. pi2_deq_data: Pointer to buffer which stores the output of IQ.
*            This buffer shall be modified by this function
*            7. pu1_ecd: Pointer to buffer which stores the data output by
*            entropy coding. This buffer shall be modified by this function
*            8. pu1_cabac_ctxt: Pointer to buffer which stores the current CABAC
*            state. This buffer shall be modified by this function
*    Output : NA
*
*****************************************************************************
*/
static void ihevce_tu_processor(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    tu_tree_node_t *ps_node,
    buffer_data_for_tu_t *ps_buffer_data,
    UWORD8 *pu1_cabac_ctxt,
    WORD32 i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_compute_spatial_ssd)
{
    UWORD8 u1_is_recon_available;

    void *pv_src = ps_buffer_data->s_src_pred_rec_buf_luma.pv_src;
    void *pv_pred = ps_buffer_data->s_src_pred_rec_buf_luma.pv_pred;
    void *pv_recon = ps_buffer_data->s_src_pred_rec_buf_luma.pv_recon;
    WORD16 *pi2_deq_data = ps_buffer_data->pi2_deq_data;
    UWORD8 *pu1_ecd = ps_buffer_data->ppu1_ecd[0];
    WORD32 i4_src_stride = ps_buffer_data->s_src_pred_rec_buf_luma.i4_src_stride;
    WORD32 i4_pred_stride = ps_buffer_data->s_src_pred_rec_buf_luma.i4_pred_stride;
    WORD32 i4_recon_stride = ps_buffer_data->s_src_pred_rec_buf_luma.i4_recon_stride;
    WORD32 i4_deq_data_stride = ps_buffer_data->i4_deq_data_stride;
    UWORD8 u1_size = ps_node->s_luma_data.u1_size;
    UWORD8 u1_posx = ps_node->s_luma_data.u1_posx;
    UWORD8 u1_posy = ps_node->s_luma_data.u1_posy;
    WORD32 trans_size = (64 == u1_size) ? 32 : u1_size;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);

    (void)pu1_cabac_ctxt;
    {
        pv_src = ((UWORD8 *)pv_src) + u1_posx + u1_posy * i4_src_stride;
        pv_pred = ((UWORD8 *)pv_pred) + u1_posx + u1_posy * i4_pred_stride;
        pv_recon = ((UWORD8 *)pv_recon) + u1_posx + u1_posy * i4_recon_stride;
    }

    pi2_deq_data += u1_posx + u1_posy * i4_deq_data_stride;

    /*2 Multi- dimensinal array based on trans size  of rounding factor to be added here */
    /* arrays are for rounding factor corr. to 0-1 decision and 1-2 decision */
    /* Currently the complete array will contain only single value*/
    /*The rounding factor is calculated with the formula
    Deadzone val = (((R1 - R0) * (2^(-8/3)) * lamMod) + 1)/2
    rounding factor = (1 - DeadZone Val)

    Assumption: Cabac states of All the sub-blocks in the TU are considered independent
    */
    if((ps_ctxt->i4_quant_rounding_level == TU_LEVEL_QUANT_ROUNDING) &&
       (ps_node->s_luma_data.u1_posx || ps_node->s_luma_data.u1_posy))
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
        ps_ctxt->pi4_quant_round_factor_tu_0_1[trans_size >> 3] = &ps_ctxt->i4_quant_round_tu[0][0];
        ps_ctxt->pi4_quant_round_factor_tu_1_2[trans_size >> 3] = &ps_ctxt->i4_quant_round_tu[1][0];

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

#if ENABLE_INTER_ZCU_COST
    ps_ctxt->i8_cu_not_coded_cost = 0;
#endif

    {
        ps_node->s_luma_data.u1_cbf = ihevce_t_q_iq_ssd_scan_fxn(
            ps_ctxt,
            (UWORD8 *)pv_pred,
            i4_pred_stride,
            (UWORD8 *)pv_src,
            i4_src_stride,
            pi2_deq_data,
            i4_deq_data_stride,
            (UWORD8 *)pv_recon,
            i4_recon_stride,
            pu1_ecd,
            ps_ctxt->au1_cu_csbf,
            ps_ctxt->i4_cu_csbf_strd,
            u1_size,
            i4_pred_mode,
            &ps_node->s_luma_data.i8_ssd,
            &ps_node->s_luma_data.i4_num_bytes_used_for_ecd,
            &ps_node->s_luma_data.i4_bits,
            &ps_node->s_luma_data.u4_sad,
            &ps_node->s_luma_data.i4_zero_col,
            &ps_node->s_luma_data.i4_zero_row,
            &u1_is_recon_available,
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq,
            ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
            1);
    }

#if ENABLE_INTER_ZCU_COST
    ps_node->s_luma_data.i8_not_coded_cost = ps_ctxt->i8_cu_not_coded_cost;
#endif

    if(u1_compute_spatial_ssd && u1_is_recon_available)
    {
        ps_node->s_luma_data.u1_reconBufId = 0;
    }
    else
    {
        ps_node->s_luma_data.u1_reconBufId = UCHAR_MAX;
    }

    ps_node->s_luma_data.i8_cost =
        ps_node->s_luma_data.i8_ssd +
        COMPUTE_RATE_COST_CLIP30(
            ps_node->s_luma_data.i4_bits, ps_ctxt->i8_cl_ssd_lambda_qf, LAMBDA_Q_SHIFT);

    pu1_ecd += ps_node->s_luma_data.i4_num_bytes_used_for_ecd;

    if(u1_chroma_processing_enabled &&
       ((!(u1_posx % 8) && !(u1_posy % 8) && (4 == u1_size)) || (u1_size > 4)))
    {
        UWORD8 i;
        void *pv_chroma_src;
        void *pv_chroma_pred;
        void *pv_chroma_recon;
        WORD16 *pi2_deq_data_chroma;

        WORD32 i4_chroma_src_stride = ps_buffer_data->s_src_pred_rec_buf_chroma.i4_src_stride;
        WORD32 i4_chroma_pred_stride = ps_buffer_data->s_src_pred_rec_buf_chroma.i4_pred_stride;
        WORD32 i4_chroma_recon_stride = ps_buffer_data->s_src_pred_rec_buf_chroma.i4_recon_stride;
        WORD32 i4_deq_data_stride_chroma = ps_buffer_data->i4_deq_data_stride_chroma;

        /* SubTU loop */
        for(i = 0; i < u1_is_422 + 1; i++)
        {
            UWORD8 u1_chroma_size = ps_node->as_cb_data[i].u1_size;
            UWORD8 u1_chroma_posx = ps_node->as_cb_data[i].u1_posx;
            UWORD8 u1_chroma_posy = ps_node->as_cb_data[i].u1_posy;

#if ENABLE_INTER_ZCU_COST
            ps_ctxt->i8_cu_not_coded_cost = 0;
#endif

            pi2_deq_data_chroma = ps_buffer_data->pi2_deq_data_chroma + (u1_chroma_posx * 2) +
                                  u1_chroma_posy * i4_deq_data_stride_chroma;

            {
                pv_chroma_src = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_src) +
                                (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_src_stride;
                pv_chroma_pred = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_pred) +
                                 (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_pred_stride;
                pv_chroma_recon = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_recon) +
                                  (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_recon_stride;

                ps_node->as_cb_data[i].u1_cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                    ps_ctxt,
                    (UWORD8 *)pv_chroma_pred,
                    i4_chroma_pred_stride,
                    (UWORD8 *)pv_chroma_src,
                    i4_chroma_src_stride,
                    pi2_deq_data_chroma,
                    i4_deq_data_stride_chroma,
                    (UWORD8 *)pv_chroma_recon,
                    i4_chroma_recon_stride,
                    pu1_ecd,
                    ps_ctxt->au1_cu_csbf,
                    ps_ctxt->i4_cu_csbf_strd,
                    u1_chroma_size,
                    SCAN_DIAG_UPRIGHT,
                    0,
                    &ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd,
                    &ps_node->as_cb_data[i].i4_bits,
                    &ps_node->as_cb_data[i].i4_zero_col,
                    &ps_node->as_cb_data[i].i4_zero_row,
                    &u1_is_recon_available,
                    ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq,
                    ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh,
                    &ps_node->as_cb_data[i].i8_ssd,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    i4_pred_mode == PRED_MODE_SKIP,
                    u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                    U_PLANE);
            }

#if ENABLE_INTER_ZCU_COST
            ps_node->as_cb_data[i].i8_not_coded_cost = ps_ctxt->i8_cu_not_coded_cost;
#endif

            if(u1_compute_spatial_ssd && u1_is_recon_available)
            {
                ps_node->as_cb_data[i].u1_reconBufId = 0;
            }
            else
            {
                ps_node->as_cb_data[i].u1_reconBufId = UCHAR_MAX;
            }

            ps_node->as_cb_data[i].i8_cost =
                ps_node->as_cb_data[i].i8_ssd + COMPUTE_RATE_COST_CLIP30(
                                                    ps_node->as_cb_data[i].i4_bits,
                                                    ps_ctxt->i8_cl_ssd_lambda_chroma_qf,
                                                    LAMBDA_Q_SHIFT);

#if WEIGH_CHROMA_COST
            ps_node->as_cb_data[i].i8_cost =
                (ps_node->as_cb_data[i].i8_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                 (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT;
#endif

            pu1_ecd += ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd;
        }

        for(i = 0; i < u1_is_422 + 1; i++)
        {
            UWORD8 u1_chroma_size = ps_node->as_cr_data[i].u1_size;
            UWORD8 u1_chroma_posx = ps_node->as_cr_data[i].u1_posx;
            UWORD8 u1_chroma_posy = ps_node->as_cr_data[i].u1_posy;

#if ENABLE_INTER_ZCU_COST
            ps_ctxt->i8_cu_not_coded_cost = 0;
#endif

            pi2_deq_data_chroma = ps_buffer_data->pi2_deq_data_chroma + u1_chroma_size +
                                  (u1_chroma_posx * 2) + u1_chroma_posy * i4_deq_data_stride_chroma;

            {
                pv_chroma_src = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_src) +
                                (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_src_stride;
                pv_chroma_pred = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_pred) +
                                 (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_pred_stride;
                pv_chroma_recon = ((UWORD8 *)ps_buffer_data->s_src_pred_rec_buf_chroma.pv_recon) +
                                  (u1_chroma_posx * 2) + u1_chroma_posy * i4_chroma_recon_stride;

                ps_node->as_cr_data[i].u1_cbf = ihevce_chroma_t_q_iq_ssd_scan_fxn(
                    ps_ctxt,
                    (UWORD8 *)pv_chroma_pred,
                    i4_chroma_pred_stride,
                    (UWORD8 *)pv_chroma_src,
                    i4_chroma_src_stride,
                    pi2_deq_data_chroma,
                    i4_deq_data_stride_chroma,
                    (UWORD8 *)pv_chroma_recon,
                    i4_chroma_recon_stride,
                    pu1_ecd,
                    ps_ctxt->au1_cu_csbf,
                    ps_ctxt->i4_cu_csbf_strd,
                    u1_chroma_size,
                    SCAN_DIAG_UPRIGHT,
                    0,
                    &ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd,
                    &ps_node->as_cr_data[i].i4_bits,
                    &ps_node->as_cr_data[i].i4_zero_col,
                    &ps_node->as_cr_data[i].i4_zero_row,
                    &u1_is_recon_available,
                    ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_rdoq,
                    ps_ctxt->s_rdoq_sbh_ctxt.i4_perform_all_cand_sbh,
                    &ps_node->as_cr_data[i].i8_ssd,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    i4_pred_mode == PRED_MODE_SKIP,
                    u1_compute_spatial_ssd ? SPATIAL_DOMAIN_SSD : FREQUENCY_DOMAIN_SSD,
                    V_PLANE);
            }

#if ENABLE_INTER_ZCU_COST
            ps_node->as_cr_data[i].i8_not_coded_cost = ps_ctxt->i8_cu_not_coded_cost;
#endif

            if(u1_compute_spatial_ssd && u1_is_recon_available)
            {
                ps_node->as_cr_data[i].u1_reconBufId = 0;
            }
            else
            {
                ps_node->as_cr_data[i].u1_reconBufId = UCHAR_MAX;
            }

            ps_node->as_cr_data[i].i8_cost =
                ps_node->as_cr_data[i].i8_ssd + COMPUTE_RATE_COST_CLIP30(
                                                    ps_node->as_cr_data[i].i4_bits,
                                                    ps_ctxt->i8_cl_ssd_lambda_chroma_qf,
                                                    LAMBDA_Q_SHIFT);

#if WEIGH_CHROMA_COST
            ps_node->as_cr_data[i].i8_cost =
                (ps_node->as_cr_data[i].i8_cost * ps_ctxt->u4_chroma_cost_weighing_factor +
                 (1 << (CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT - 1))) >>
                CHROMA_COST_WEIGHING_FACTOR_Q_SHIFT;
#endif

            pu1_ecd += ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd;
        }
    }
}

static INLINE void ihevce_nbr_data_copier(
    nbr_4x4_t *ps_nbr_data_buf,
    WORD32 i4_nbr_data_buf_stride,
    WORD32 i4_cu_qp,
    UWORD8 u1_cbf,
    WORD32 u1_posx,
    UWORD8 u1_posy,
    UWORD8 u1_size)
{
    WORD32 i, j;

    UWORD8 u1_num_4x4_in_tu = u1_size / 4;

    ps_nbr_data_buf += ((u1_posx) / 4) + (u1_posy / 4) * i4_nbr_data_buf_stride;

    for(i = 0; i < u1_num_4x4_in_tu; i++)
    {
        for(j = 0; j < u1_num_4x4_in_tu; j++)
        {
            ps_nbr_data_buf[j].b8_qp = i4_cu_qp;
            ps_nbr_data_buf[j].b1_y_cbf = u1_cbf;
        }

        ps_nbr_data_buf += i4_nbr_data_buf_stride;
    }
}

static INLINE void ihevce_debriefer_when_parent_wins(
    tu_tree_node_t *ps_node,
    FT_COPY_2D *pf_copy_2d,
    FT_CHROMA_INTERLEAVE_2D_COPY *pf_chroma_interleave_2d_copy,
    nbr_4x4_t *ps_nbr_data_buf,
    WORD16 *pi2_deq_data_src,
    WORD16 *pi2_deq_data_dst,
    WORD16 *pi2_deq_data_src_chroma,
    WORD16 *pi2_deq_data_dst_chroma,
    void *pv_recon_src,
    void *pv_recon_dst,
    void *pv_recon_src_chroma,
    void *pv_recon_dst_chroma,
    UWORD8 *pu1_cabac_ctxt_src,
    UWORD8 *pu1_cabac_ctxt_dst,
    UWORD8 *pu1_ecd_src,
    UWORD8 *pu1_ecd_dst,
    WORD32 i4_nbr_data_buf_stride,
    WORD32 i4_deq_data_src_stride,
    WORD32 i4_deq_data_dst_stride,
    WORD32 i4_deq_data_src_stride_chroma,
    WORD32 i4_deq_data_dst_stride_chroma,
    WORD32 i4_recon_src_stride,
    WORD32 i4_recon_dst_stride,
    WORD32 i4_recon_src_stride_chroma,
    WORD32 i4_recon_dst_stride_chroma,
    WORD32 i4_cabac_state_table_size,
    WORD32 i4_cu_qp,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422,
    UWORD8 u1_is_hbd)
{
    UWORD8 i;

    UWORD32 u4_num_ecd_bytes = 0;

    /* Y */
    {
        UWORD8 u1_posx = ps_node->s_luma_data.u1_posx;
        UWORD8 u1_posy = ps_node->s_luma_data.u1_posy;
        UWORD8 *pu1_deq_data_dst =
            (UWORD8 *)(pi2_deq_data_dst + u1_posx + u1_posy * i4_deq_data_dst_stride);
        UWORD8 *pu1_deq_data_src =
            (UWORD8 *)(pi2_deq_data_src + u1_posx + u1_posy * i4_deq_data_src_stride);
        UWORD8 *pu1_recon_dst;
        UWORD8 *pu1_recon_src;

        {
            pu1_recon_dst = (((UWORD8 *)pv_recon_dst) + u1_posx + u1_posy * i4_recon_dst_stride);
            pu1_recon_src = (((UWORD8 *)pv_recon_src) + u1_posx + u1_posy * i4_recon_src_stride);
        }
        u4_num_ecd_bytes += ps_node->s_luma_data.i4_num_bytes_used_for_ecd;

        if(ps_node->s_luma_data.u1_reconBufId != UCHAR_MAX)
        {
            pf_copy_2d(
                pu1_recon_dst,
                i4_recon_dst_stride * (u1_is_hbd + 1),
                pu1_recon_src,
                i4_recon_src_stride * (u1_is_hbd + 1),
                ps_node->s_luma_data.u1_size * (u1_is_hbd + 1),
                ps_node->s_luma_data.u1_size);
        }
        else if(ps_node->s_luma_data.u1_cbf)
        {
            pf_copy_2d(
                pu1_deq_data_dst,
                i4_deq_data_dst_stride * 2,
                pu1_deq_data_src,
                i4_deq_data_src_stride * 2,
                ps_node->s_luma_data.u1_size * 2,
                ps_node->s_luma_data.u1_size);
        }
    }

    /* Cb */
    if(u1_chroma_processing_enabled)
    {
        for(i = 0; i < u1_is_422 + 1; i++)
        {
            UWORD8 u1_posx = ps_node->as_cb_data[i].u1_posx;
            UWORD8 u1_posy = ps_node->as_cb_data[i].u1_posy;
            UWORD8 *pu1_deq_data_dst =
                (UWORD8
                     *)(pi2_deq_data_dst_chroma + (u1_posx * 2) + (u1_posy * i4_deq_data_dst_stride_chroma));
            UWORD8 *pu1_deq_data_src =
                (UWORD8
                     *)(pi2_deq_data_src_chroma + (u1_posx * 2) + (u1_posy * i4_deq_data_src_stride_chroma));
            UWORD8 *pu1_recon_dst;
            UWORD8 *pu1_recon_src;

            {
                pu1_recon_dst =
                    (((UWORD8 *)pv_recon_dst_chroma) + (u1_posx * 2) +
                     u1_posy * i4_recon_dst_stride_chroma);
                pu1_recon_src =
                    (((UWORD8 *)pv_recon_src_chroma) + (u1_posx * 2) +
                     u1_posy * i4_recon_src_stride_chroma);
            }
            u4_num_ecd_bytes += ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd;

            if(ps_node->as_cb_data[i].u1_reconBufId != UCHAR_MAX)
            {
                {
                    pf_chroma_interleave_2d_copy(
                        pu1_recon_src,
                        i4_recon_src_stride_chroma * (u1_is_hbd + 1),
                        pu1_recon_dst,
                        i4_recon_dst_stride_chroma * (u1_is_hbd + 1),
                        ps_node->as_cb_data[i].u1_size * (u1_is_hbd + 1),
                        ps_node->as_cb_data[i].u1_size,
                        U_PLANE);
                }
            }
            else if(ps_node->as_cb_data[i].u1_cbf)
            {
                pf_copy_2d(
                    pu1_deq_data_dst,
                    i4_deq_data_dst_stride_chroma * 2,
                    pu1_deq_data_src,
                    i4_deq_data_src_stride_chroma * 2,
                    ps_node->as_cb_data[i].u1_size * 2,
                    ps_node->as_cb_data[i].u1_size);
            }
        }

        /* Cr */
        for(i = 0; i < u1_is_422 + 1; i++)
        {
            UWORD8 u1_posx = ps_node->as_cr_data[i].u1_posx;
            UWORD8 u1_posy = ps_node->as_cr_data[i].u1_posy;
            UWORD8 *pu1_deq_data_dst =
                (UWORD8
                     *)(pi2_deq_data_dst_chroma + ps_node->as_cr_data[i].u1_size + (u1_posx * 2) + (u1_posy * i4_deq_data_dst_stride_chroma));
            UWORD8 *pu1_deq_data_src =
                (UWORD8
                     *)(pi2_deq_data_src_chroma + ps_node->as_cr_data[i].u1_size + (u1_posx * 2) + (u1_posy * i4_deq_data_src_stride_chroma));
            UWORD8 *pu1_recon_dst;
            UWORD8 *pu1_recon_src;

            {
                pu1_recon_dst =
                    (((UWORD8 *)pv_recon_dst_chroma) + (u1_posx * 2) +
                     u1_posy * i4_recon_dst_stride_chroma);
                pu1_recon_src =
                    (((UWORD8 *)pv_recon_src_chroma) + (u1_posx * 2) +
                     u1_posy * i4_recon_src_stride_chroma);
            }
            u4_num_ecd_bytes += ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd;

            if(ps_node->as_cr_data[i].u1_reconBufId != UCHAR_MAX)
            {
                {
                    pf_chroma_interleave_2d_copy(
                        pu1_recon_src,
                        i4_recon_src_stride_chroma * (u1_is_hbd + 1),
                        pu1_recon_dst,
                        i4_recon_dst_stride_chroma * (u1_is_hbd + 1),
                        ps_node->as_cr_data[i].u1_size * (u1_is_hbd + 1),
                        ps_node->as_cr_data[i].u1_size,
                        V_PLANE);
                }
            }
            else if(ps_node->as_cr_data[i].u1_cbf)
            {
                pf_copy_2d(
                    pu1_deq_data_dst,
                    i4_deq_data_dst_stride_chroma * 2,
                    pu1_deq_data_src,
                    i4_deq_data_src_stride_chroma * 2,
                    ps_node->as_cr_data[i].u1_size * 2,
                    ps_node->as_cr_data[i].u1_size);
            }
        }
    }

    if(pu1_ecd_dst != pu1_ecd_src)
    {
        memmove(pu1_ecd_dst, pu1_ecd_src, u4_num_ecd_bytes);
    }

    memcpy(pu1_cabac_ctxt_dst, pu1_cabac_ctxt_src, i4_cabac_state_table_size);

    ihevce_nbr_data_copier(
        ps_nbr_data_buf,
        i4_nbr_data_buf_stride,
        i4_cu_qp,
        ps_node->s_luma_data.u1_cbf,
        ps_node->s_luma_data.u1_posx,
        ps_node->s_luma_data.u1_posy,
        ps_node->s_luma_data.u1_size);

    ps_node->ps_child_node_tl = NULL;
    ps_node->ps_child_node_tr = NULL;
    ps_node->ps_child_node_bl = NULL;
    ps_node->ps_child_node_br = NULL;
}

/*!
******************************************************************************
* \if Function name : ihevce_ecd_buffer_pointer_updater \endif
*
* \brief
*    Updates ppu1_ecd with current pointer
*    Output : Number of byte positions 'pu1_ecd_buf_ptr_at_t0' is incremented by
*
*****************************************************************************
*/
static INLINE UWORD32 ihevce_ecd_buffer_pointer_updater(
    tu_tree_node_t *ps_node,
    UWORD8 **ppu1_ecd,
    UWORD8 *pu1_ecd_buf_ptr_at_t0,
    UWORD8 u1_parent_has_won,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422)
{
    UWORD8 i;

    UWORD32 u4_num_bytes = 0;

    if(u1_parent_has_won)
    {
        u4_num_bytes += ps_node->s_luma_data.i4_num_bytes_used_for_ecd;

        if(u1_chroma_processing_enabled)
        {
            for(i = 0; i < u1_is_422 + 1; i++)
            {
                u4_num_bytes += ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd;
            }
        }
    }
    else
    {
        u4_num_bytes += ps_node->ps_child_node_tl->s_luma_data.i4_num_bytes_used_for_ecd;
        u4_num_bytes += ps_node->ps_child_node_tr->s_luma_data.i4_num_bytes_used_for_ecd;
        u4_num_bytes += ps_node->ps_child_node_bl->s_luma_data.i4_num_bytes_used_for_ecd;
        u4_num_bytes += ps_node->ps_child_node_br->s_luma_data.i4_num_bytes_used_for_ecd;

        if(u1_chroma_processing_enabled)
        {
            for(i = 0; i < u1_is_422 + 1; i++)
            {
                u4_num_bytes += ps_node->ps_child_node_tl->as_cb_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_tl->as_cr_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_tr->as_cb_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_tr->as_cr_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_bl->as_cb_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_bl->as_cr_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_br->as_cb_data[i].i4_num_bytes_used_for_ecd;
                u4_num_bytes += ps_node->ps_child_node_br->as_cr_data[i].i4_num_bytes_used_for_ecd;
            }
        }
    }

    ppu1_ecd[0] = pu1_ecd_buf_ptr_at_t0 + u4_num_bytes;

    return u4_num_bytes;
}

static INLINE LWORD64 ihevce_tu_node_cost_collator(
    tu_tree_node_t *ps_node, UWORD8 u1_chroma_processing_enabled, UWORD8 u1_is_422)
{
    UWORD8 i;

    LWORD64 i8_cost = 0;

    i8_cost += ps_node->s_luma_data.i8_cost;

    if(u1_chroma_processing_enabled)
    {
        for(i = 0; i < u1_is_422 + 1; i++)
        {
            i8_cost += ps_node->as_cb_data[i].i8_cost;
            i8_cost += ps_node->as_cr_data[i].i8_cost;
        }
    }

    return i8_cost;
}

#if !ENABLE_TOP_DOWN_TU_RECURSION
/*!
******************************************************************************
* \if Function name : ihevce_tu_processor \endif
*
* \notes
*    Determines RDO TU Tree using DFS. If the parent is the winner, then all
*    pointers to the children nodes are set to NULL
*    Input : 1. ps_ctxt: Pointer to enc-loop's context. Parts of this structure
*            shall be modified by this function. They include, au1_cu_csbf,
*            i8_cu_not_coded_cost, ai2_scratch, s_rdoq_sbh_ctxt,
*            pi4_quant_round_factor_tu_0_1, pi4_quant_round_factor_tu_1_2,
*            i4_quant_round_tu
*            2. ps_node: Pointer to current node of the TU tree. This struct
*            shall be modified by this function
*            3. pv_recon: Pointer to buffer which stores the recon
*            This buffer shall be modified by this function
*            4. ps_nbr_data_buf: Pointer to struct used by succeeding CU's
*            during RDOPT. This buffer shall be modifie by this function
*            6. pi2_deq_data: Pointer to buffer which stores the output of IQ.
*            This buffer shall be modified by this function
*            7. pu1_ecd: Pointer to buffer which stores the data output by
*            entropy coding. This buffer shall be modified by this function
*            8. pu1_cabac_ctxt: Pointer to buffer which stores the current CABAC
*            state. This buffer shall be modified by this function
*    Output : Cost of coding the current branch of the TU tree
*
*****************************************************************************
*/
LWORD64 ihevce_tu_tree_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    tu_tree_node_t *ps_node,
    buffer_data_for_tu_t *ps_buffer_data,
    UWORD8 *pu1_cabac_ctxt,
    WORD32 i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_cur_depth,
    UWORD8 u1_max_depth,
    UWORD8 u1_part_type,
    UWORD8 u1_compute_spatial_ssd)
{
    UWORD8 au1_cabac_ctxt_backup[IHEVC_CAB_CTXT_END];
    UWORD8 u1_are_children_available;
    UWORD32 u4_tuSplitFlag_and_cbf_coding_bits;

    nbr_4x4_t *ps_nbr_data_buf = ps_buffer_data->ps_nbr_data_buf;
    void *pv_recon_chroma = ps_buffer_data->s_src_pred_rec_buf_chroma.pv_recon;
    WORD16 *pi2_deq_data = ps_buffer_data->pi2_deq_data;
    WORD16 *pi2_deq_data_chroma = ps_buffer_data->pi2_deq_data_chroma;
    UWORD8 **ppu1_ecd = ps_buffer_data->ppu1_ecd;
    WORD32 i4_nbr_data_buf_stride = ps_buffer_data->i4_nbr_data_buf_stride;
    WORD32 i4_recon_stride = ps_buffer_data->s_src_pred_rec_buf_luma.i4_recon_stride;
    WORD32 i4_recon_stride_chroma = ps_buffer_data->s_src_pred_rec_buf_chroma.i4_recon_stride;
    WORD32 i4_deq_data_stride = ps_buffer_data->i4_deq_data_stride;
    WORD32 i4_deq_data_stride_chroma = ps_buffer_data->i4_deq_data_stride_chroma;
    UWORD8 *pu1_ecd_bPtr_backup_t1 = ppu1_ecd[0];
    UWORD8 *pu1_ecd_bPtr_backup_t2 = ppu1_ecd[0];
    LWORD64 i8_winning_cost = 0;

    ASSERT(ps_node != NULL);
    ASSERT(
        !(!ps_node->u1_is_valid_node &&
          ((NULL == ps_node->ps_child_node_tl) || (NULL == ps_node->ps_child_node_tr) ||
           (NULL == ps_node->ps_child_node_bl) || (NULL == ps_node->ps_child_node_br))));

    u1_are_children_available =
        !((NULL == ps_node->ps_child_node_tl) && (NULL == ps_node->ps_child_node_tr) &&
          (NULL == ps_node->ps_child_node_bl) && (NULL == ps_node->ps_child_node_br)) &&
        (ps_node->s_luma_data.u1_size > MIN_TU_SIZE);

    if(u1_are_children_available)
    {
        if(ps_node->u1_is_valid_node)
        {
            memcpy(au1_cabac_ctxt_backup, pu1_cabac_ctxt, sizeof(au1_cabac_ctxt_backup));
        }

        if(i4_pred_mode != PRED_MODE_SKIP)
        {
            u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                ps_node,
                ps_node->ps_child_node_tl,
                pu1_cabac_ctxt,
                MAX_TU_SIZE,
                MIN_TU_SIZE,
                0,
                1,
                i4_pred_mode == PRED_MODE_INTRA,
                (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                0,
                0);

            i8_winning_cost += COMPUTE_RATE_COST_CLIP30(
                u4_tuSplitFlag_and_cbf_coding_bits,
                ps_ctxt->i8_cl_ssd_lambda_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
        }

        i8_winning_cost += ihevce_tu_tree_selector(
            ps_ctxt,
            ps_node->ps_child_node_tl,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_cur_depth,
            u1_max_depth,
            u1_part_type,
            u1_compute_spatial_ssd);

        i8_winning_cost += ihevce_tu_tree_selector(
            ps_ctxt,
            ps_node->ps_child_node_tr,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_cur_depth,
            u1_max_depth,
            u1_part_type,
            u1_compute_spatial_ssd);

        i8_winning_cost += ihevce_tu_tree_selector(
            ps_ctxt,
            ps_node->ps_child_node_bl,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_cur_depth,
            u1_max_depth,
            u1_part_type,
            u1_compute_spatial_ssd);

        i8_winning_cost += ihevce_tu_tree_selector(
            ps_ctxt,
            ps_node->ps_child_node_br,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_cur_depth,
            u1_max_depth,
            u1_part_type,
            u1_compute_spatial_ssd);

        if(ps_node->u1_is_valid_node)
        {
            WORD16 ai2_deq_data_backup[MAX_CU_SIZE * MAX_CU_SIZE];
            UWORD16 au2_recon_backup[MAX_CU_SIZE * MAX_CU_SIZE];

            buffer_data_for_tu_t s_buffer_data = ps_buffer_data[0];

            pu1_ecd_bPtr_backup_t2 = ppu1_ecd[0];
            s_buffer_data.pi2_deq_data = ai2_deq_data_backup;
            s_buffer_data.i4_deq_data_stride = MAX_CU_SIZE;
            s_buffer_data.s_src_pred_rec_buf_luma.pv_recon = au2_recon_backup;
            s_buffer_data.s_src_pred_rec_buf_luma.i4_recon_stride = MAX_CU_SIZE;

            ihevce_tu_processor(
                ps_ctxt,
                ps_node,
                &s_buffer_data,
                au1_cabac_ctxt_backup,
                i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                i4_alpha_stim_multiplier,
                u1_is_cu_noisy,
#endif
                0,
                u1_compute_spatial_ssd);

            if(i4_pred_mode != PRED_MODE_SKIP)
            {
                u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                    ps_node,
                    ps_node,
                    au1_cabac_ctxt_backup,
                    MAX_TU_SIZE,
                    MIN_TU_SIZE,
                    0,
                    (u1_cur_depth == u1_max_depth) ? 0 : 1,
                    i4_pred_mode == PRED_MODE_INTRA,
                    (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                    0,
                    0);

                ps_node->s_luma_data.i8_cost += COMPUTE_RATE_COST_CLIP30(
                    u4_tuSplitFlag_and_cbf_coding_bits,
                    ps_ctxt->i8_cl_ssd_lambda_qf,
                    (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
            }

            if(ps_node->s_luma_data.i8_cost <= i8_winning_cost)
            {
                ihevce_debriefer_when_parent_wins(
                    ps_node,
                    ps_ctxt->s_cmn_opt_func.pf_copy_2d,
                    ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy,
                    ps_nbr_data_buf,
                    ai2_deq_data_backup,
                    pi2_deq_data,
                    ai2_deq_data_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                    pi2_deq_data_chroma,
                    au2_recon_backup,
                    pv_recon_chroma,
                    au2_recon_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                    pv_recon_chroma,
                    au1_cabac_ctxt_backup,
                    pu1_cabac_ctxt,
                    pu1_ecd_bPtr_backup_t2,
                    pu1_ecd_bPtr_backup_t1,
                    i4_nbr_data_buf_stride,
                    MAX_CU_SIZE,
                    i4_deq_data_stride,
                    MAX_CU_SIZE,
                    i4_deq_data_stride_chroma,
                    MAX_CU_SIZE,
                    i4_recon_stride,
                    MAX_CU_SIZE,
                    i4_recon_stride_chroma,
                    sizeof(au1_cabac_ctxt_backup),
                    ps_ctxt->i4_cu_qp,
                    0,
                    ps_ctxt->u1_chroma_array_type == 2,
                    ps_ctxt->u1_bit_depth > 8);

                ppu1_ecd[0] =
                    pu1_ecd_bPtr_backup_t1 + ps_node->s_luma_data.i4_num_bytes_used_for_ecd;
                i8_winning_cost = ps_node->s_luma_data.i8_cost;
            }
            else
            {
                ps_node->u1_is_valid_node = 0;
            }
        }
    }
    else
    {
        ASSERT(ps_node->u1_is_valid_node);

        ihevce_tu_processor(
            ps_ctxt,
            ps_node,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            0,
            u1_compute_spatial_ssd);

        if(i4_pred_mode != PRED_MODE_SKIP)
        {
            u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                ps_node,
                ps_node,
                pu1_cabac_ctxt,
                MAX_TU_SIZE,
                MIN_TU_SIZE,
                0,
                (u1_cur_depth == u1_max_depth) ? 0 : 1,
                i4_pred_mode == PRED_MODE_INTRA,
                (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                0,
                0);

            ps_node->s_luma_data.i8_cost += COMPUTE_RATE_COST_CLIP30(
                u4_tuSplitFlag_and_cbf_coding_bits,
                ps_ctxt->i8_cl_ssd_lambda_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
        }

        ppu1_ecd[0] = pu1_ecd_bPtr_backup_t1 + ps_node->s_luma_data.i4_num_bytes_used_for_ecd;

        ihevce_nbr_data_copier(
            ps_nbr_data_buf,
            i4_nbr_data_buf_stride,
            ps_ctxt->i4_cu_qp,
            ps_node->s_luma_data.u1_cbf,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            ps_node->s_luma_data.u1_size);

        i8_winning_cost = ps_node->s_luma_data.i8_cost;
    }

    return i8_winning_cost;
}
#endif

/*!
******************************************************************************
* \if Function name : ihevce_topDown_tu_tree_selector \endif
*
* \notes
*    Determines RDO TU Tree using DFS. If the parent is the winner, then all
*    pointers to the children nodes are set to NULL
*    Input : 1. ps_ctxt: Pointer to enc-loop's context. Parts of this structure
*            shall be modified by this function. They include, au1_cu_csbf,
*            i8_cu_not_coded_cost, ai2_scratch, s_rdoq_sbh_ctxt,
*            pi4_quant_round_factor_tu_0_1, pi4_quant_round_factor_tu_1_2,
*            i4_quant_round_tu
*            2. ps_node: Pointer to current node of the TU tree. This struct
*            shall be modified by this function
*            3. pv_recon: Pointer to buffer which stores the recon
*            This buffer shall be modified by this function
*            4. ps_nbr_data_buf: Pointer to struct used by succeeding CU's
*            during RDOPT. This buffer shall be modifie by this function
*            6. pi2_deq_data: Pointer to buffer which stores the output of IQ.
*            This buffer shall be modified by this function
*            7. pu1_ecd: Pointer to buffer which stores the data output by
*            entropy coding. This buffer shall be modified by this function
*            8. pu1_cabac_ctxt: Pointer to buffer which stores the current CABAC
*            state. This buffer shall be modified by this function
*    Output : Cost of coding the current branch of the TU tree
*
*****************************************************************************
*/
LWORD64 ihevce_topDown_tu_tree_selector(
    ihevce_enc_loop_ctxt_t *ps_ctxt,
    tu_tree_node_t *ps_node,
    buffer_data_for_tu_t *ps_buffer_data,
    UWORD8 *pu1_cabac_ctxt,
    WORD32 i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_is_cu_noisy,
#endif
    UWORD8 u1_cur_depth,
    UWORD8 u1_max_depth,
    UWORD8 u1_part_type,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_compute_spatial_ssd)
{
    UWORD8 au1_cabac_ctxt_backup[IHEVC_CAB_CTXT_END];
    UWORD8 u1_are_children_available;
    UWORD32 u4_tuSplitFlag_and_cbf_coding_bits;

    nbr_4x4_t *ps_nbr_data_buf = ps_buffer_data->ps_nbr_data_buf;

    void *pv_recon = ps_buffer_data->s_src_pred_rec_buf_luma.pv_recon;
    void *pv_recon_chroma = ps_buffer_data->s_src_pred_rec_buf_chroma.pv_recon;
    WORD16 *pi2_deq_data = ps_buffer_data->pi2_deq_data;
    WORD16 *pi2_deq_data_chroma = ps_buffer_data->pi2_deq_data_chroma;
    UWORD8 **ppu1_ecd = ps_buffer_data->ppu1_ecd;
    WORD32 i4_nbr_data_buf_stride = ps_buffer_data->i4_nbr_data_buf_stride;
    WORD32 i4_recon_stride = ps_buffer_data->s_src_pred_rec_buf_luma.i4_recon_stride;
    WORD32 i4_recon_stride_chroma = ps_buffer_data->s_src_pred_rec_buf_chroma.i4_recon_stride;
    WORD32 i4_deq_data_stride = ps_buffer_data->i4_deq_data_stride;
    WORD32 i4_deq_data_stride_chroma = ps_buffer_data->i4_deq_data_stride_chroma;
    UWORD8 *pu1_ecd_bPtr_backup_t1 = ppu1_ecd[0];
    UWORD8 *pu1_ecd_bPtr_backup_t2 = ppu1_ecd[0];
    LWORD64 i8_parent_cost = 0;
    LWORD64 i8_child_cost = 0;
    LWORD64 i8_winning_cost = 0;
    UWORD8 u1_is_422 = (ps_ctxt->u1_chroma_array_type == 2);

    ASSERT(ps_node != NULL);
    ASSERT(
        !(!ps_node->u1_is_valid_node &&
          ((NULL == ps_node->ps_child_node_tl) || (NULL == ps_node->ps_child_node_tr) ||
           (NULL == ps_node->ps_child_node_bl) || (NULL == ps_node->ps_child_node_br))));

    u1_are_children_available =
        !((NULL == ps_node->ps_child_node_tl) && (NULL == ps_node->ps_child_node_tr) &&
          (NULL == ps_node->ps_child_node_bl) && (NULL == ps_node->ps_child_node_br)) &&
        (ps_node->s_luma_data.u1_size > MIN_TU_SIZE);

    if(u1_are_children_available)
    {
        WORD16 ai2_deq_data_backup[MAX_CU_SIZE * MAX_CU_SIZE * 2];
        UWORD16 au2_recon_backup[MAX_CU_SIZE * MAX_CU_SIZE * 2];

        UWORD8 u1_is_tu_coded = 0;

        if(ps_node->u1_is_valid_node)
        {
            buffer_data_for_tu_t s_buffer_data = ps_buffer_data[0];

            memcpy(au1_cabac_ctxt_backup, pu1_cabac_ctxt, sizeof(au1_cabac_ctxt_backup));

            s_buffer_data.pi2_deq_data = ai2_deq_data_backup;
            s_buffer_data.i4_deq_data_stride = MAX_CU_SIZE;
            s_buffer_data.pi2_deq_data_chroma = ai2_deq_data_backup + MAX_CU_SIZE * MAX_CU_SIZE;
            s_buffer_data.i4_deq_data_stride_chroma = MAX_CU_SIZE;
            s_buffer_data.s_src_pred_rec_buf_luma.pv_recon = au2_recon_backup;
            s_buffer_data.s_src_pred_rec_buf_luma.i4_recon_stride = MAX_CU_SIZE;
            s_buffer_data.s_src_pred_rec_buf_chroma.pv_recon =
                au2_recon_backup + MAX_CU_SIZE * MAX_CU_SIZE;
            s_buffer_data.s_src_pred_rec_buf_chroma.i4_recon_stride = MAX_CU_SIZE;

            ihevce_tu_processor(
                ps_ctxt,
                ps_node,
                &s_buffer_data,
                au1_cabac_ctxt_backup,
                i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                i4_alpha_stim_multiplier,
                u1_is_cu_noisy,
#endif
                u1_chroma_processing_enabled,
                u1_compute_spatial_ssd);

            if(i4_pred_mode != PRED_MODE_SKIP)
            {
                u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                    ps_node,
                    ps_node,
                    au1_cabac_ctxt_backup,
                    MAX_TU_SIZE,
                    MIN_TU_SIZE,
                    0,
                    (u1_cur_depth == u1_max_depth) ? 0 : 1,
                    i4_pred_mode == PRED_MODE_INTRA,
                    (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                    u1_chroma_processing_enabled,
                    u1_is_422);

                ps_node->s_luma_data.i8_cost += COMPUTE_RATE_COST_CLIP30(
                    u4_tuSplitFlag_and_cbf_coding_bits,
                    ps_ctxt->i8_cl_ssd_lambda_qf,
                    (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
            }

            i8_parent_cost +=
                ihevce_tu_node_cost_collator(ps_node, u1_chroma_processing_enabled, u1_is_422);

            ihevce_ecd_buffer_pointer_updater(
                ps_node,
                ppu1_ecd,
                pu1_ecd_bPtr_backup_t1,
                1,
                u1_chroma_processing_enabled,
                u1_is_422);
        }
        else
        {
            ps_node->s_luma_data.i8_cost = i8_parent_cost = LLONG_MAX;
            ps_node->s_luma_data.i4_num_bytes_used_for_ecd = 0;
        }

        u1_is_tu_coded |= ps_node->s_luma_data.u1_cbf;

        if(u1_chroma_processing_enabled)
        {
            UWORD8 i;

            for(i = 0; i < u1_is_422 + 1; i++)
            {
                u1_is_tu_coded |= ps_node->as_cb_data[i].u1_cbf;
                u1_is_tu_coded |= ps_node->as_cr_data[i].u1_cbf;
            }
        }

        if(!ps_node->u1_is_valid_node || u1_is_tu_coded)
        {
            pu1_ecd_bPtr_backup_t2 = ppu1_ecd[0];

            if(i4_pred_mode != PRED_MODE_SKIP)
            {
                u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                    ps_node,
                    ps_node->ps_child_node_tl,
                    pu1_cabac_ctxt,
                    MAX_TU_SIZE,
                    MIN_TU_SIZE,
                    0,
                    1,
                    i4_pred_mode == PRED_MODE_INTRA,
                    (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                    u1_chroma_processing_enabled,
                    u1_is_422);

                i8_child_cost += COMPUTE_RATE_COST_CLIP30(
                    u4_tuSplitFlag_and_cbf_coding_bits,
                    ps_ctxt->i8_cl_ssd_lambda_qf,
                    (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
            }

            if(i8_child_cost < i8_parent_cost)
            {
                i8_child_cost += ihevce_topDown_tu_tree_selector(
                    ps_ctxt,
                    ps_node->ps_child_node_tl,
                    ps_buffer_data,
                    pu1_cabac_ctxt,
                    i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    u1_cur_depth,
                    u1_max_depth,
                    u1_part_type,
                    u1_chroma_processing_enabled,
                    u1_compute_spatial_ssd);

                ps_node->ps_child_node_tl->s_luma_data.i8_cost +=
                    i8_child_cost - ps_node->ps_child_node_tl->s_luma_data.i8_cost;
            }

            if(i8_child_cost < i8_parent_cost)
            {
                i8_child_cost += ihevce_topDown_tu_tree_selector(
                    ps_ctxt,
                    ps_node->ps_child_node_tr,
                    ps_buffer_data,
                    pu1_cabac_ctxt,
                    i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    u1_cur_depth,
                    u1_max_depth,
                    u1_part_type,
                    u1_chroma_processing_enabled,
                    u1_compute_spatial_ssd);
            }

            if(i8_child_cost < i8_parent_cost)
            {
                i8_child_cost += ihevce_topDown_tu_tree_selector(
                    ps_ctxt,
                    ps_node->ps_child_node_bl,
                    ps_buffer_data,
                    pu1_cabac_ctxt,
                    i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    u1_cur_depth,
                    u1_max_depth,
                    u1_part_type,
                    u1_chroma_processing_enabled,
                    u1_compute_spatial_ssd);
            }

            if(i8_child_cost < i8_parent_cost)
            {
                i8_child_cost += ihevce_topDown_tu_tree_selector(
                    ps_ctxt,
                    ps_node->ps_child_node_br,
                    ps_buffer_data,
                    pu1_cabac_ctxt,
                    i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
                    i4_alpha_stim_multiplier,
                    u1_is_cu_noisy,
#endif
                    u1_cur_depth,
                    u1_max_depth,
                    u1_part_type,
                    u1_chroma_processing_enabled,
                    u1_compute_spatial_ssd);
            }

            if(i8_parent_cost > i8_child_cost)
            {
                UWORD32 u4_num_bytes = ihevce_ecd_buffer_pointer_updater(
                    ps_node,
                    ppu1_ecd,
                    pu1_ecd_bPtr_backup_t1,
                    0,
                    u1_chroma_processing_enabled,
                    u1_is_422);

                if(pu1_ecd_bPtr_backup_t2 != pu1_ecd_bPtr_backup_t1)
                {
                    memmove(pu1_ecd_bPtr_backup_t1, pu1_ecd_bPtr_backup_t2, u4_num_bytes);
                }

                ps_node->s_luma_data.i4_num_bytes_used_for_ecd = u4_num_bytes;
                ps_node->as_cb_data[0].i4_num_bytes_used_for_ecd = 0;
                ps_node->as_cb_data[1].i4_num_bytes_used_for_ecd = 0;
                ps_node->as_cr_data[0].i4_num_bytes_used_for_ecd = 0;
                ps_node->as_cr_data[1].i4_num_bytes_used_for_ecd = 0;

                ps_node->u1_is_valid_node = 0;

                i8_winning_cost = i8_child_cost;
            }
            else
            {
                ihevce_debriefer_when_parent_wins(
                    ps_node,
                    ps_ctxt->s_cmn_opt_func.pf_copy_2d,
                    ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy,
                    ps_nbr_data_buf,
                    ai2_deq_data_backup,
                    pi2_deq_data,
                    ai2_deq_data_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                    pi2_deq_data_chroma,
                    au2_recon_backup,
                    pv_recon,
                    au2_recon_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                    pv_recon_chroma,
                    au1_cabac_ctxt_backup,
                    pu1_cabac_ctxt,
                    NULL,
                    NULL,
                    i4_nbr_data_buf_stride,
                    MAX_CU_SIZE,
                    i4_deq_data_stride,
                    MAX_CU_SIZE,
                    i4_deq_data_stride_chroma,
                    MAX_CU_SIZE,
                    i4_recon_stride,
                    MAX_CU_SIZE,
                    i4_recon_stride_chroma,
                    sizeof(au1_cabac_ctxt_backup),
                    ps_ctxt->i4_cu_qp,
                    u1_chroma_processing_enabled,
                    u1_is_422,
                    ps_ctxt->u1_bit_depth > 8);

                ihevce_ecd_buffer_pointer_updater(
                    ps_node,
                    ppu1_ecd,
                    pu1_ecd_bPtr_backup_t1,
                    1,
                    u1_chroma_processing_enabled,
                    u1_is_422);

                i8_winning_cost = i8_parent_cost;
            }
        }
        else
        {
            ihevce_debriefer_when_parent_wins(
                ps_node,
                ps_ctxt->s_cmn_opt_func.pf_copy_2d,
                ps_ctxt->s_cmn_opt_func.pf_chroma_interleave_2d_copy,
                ps_nbr_data_buf,
                ai2_deq_data_backup,
                pi2_deq_data,
                ai2_deq_data_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                pi2_deq_data_chroma,
                au2_recon_backup,
                pv_recon,
                au2_recon_backup + MAX_CU_SIZE * MAX_CU_SIZE,
                pv_recon_chroma,
                au1_cabac_ctxt_backup,
                pu1_cabac_ctxt,
                NULL,
                NULL,
                i4_nbr_data_buf_stride,
                MAX_CU_SIZE,
                i4_deq_data_stride,
                MAX_CU_SIZE,
                i4_deq_data_stride_chroma,
                MAX_CU_SIZE,
                i4_recon_stride,
                MAX_CU_SIZE,
                i4_recon_stride_chroma,
                sizeof(au1_cabac_ctxt_backup),
                ps_ctxt->i4_cu_qp,
                u1_chroma_processing_enabled,
                u1_is_422,
                ps_ctxt->u1_bit_depth > 8);

            ihevce_ecd_buffer_pointer_updater(
                ps_node,
                ppu1_ecd,
                pu1_ecd_bPtr_backup_t1,
                1,
                u1_chroma_processing_enabled,
                u1_is_422);

            i8_winning_cost = i8_parent_cost;
        }
    }
    else
    {
        ASSERT(ps_node->u1_is_valid_node);

        ihevce_tu_processor(
            ps_ctxt,
            ps_node,
            ps_buffer_data,
            pu1_cabac_ctxt,
            i4_pred_mode,
#if USE_NOISE_TERM_IN_ZERO_CODING_DECISION_ALGORITHMS
            i4_alpha_stim_multiplier,
            u1_is_cu_noisy,
#endif
            u1_chroma_processing_enabled,
            u1_compute_spatial_ssd);

        if(i4_pred_mode != PRED_MODE_SKIP)
        {
            u4_tuSplitFlag_and_cbf_coding_bits = ihevce_compute_bits_for_TUSplit_and_cbf(
                ps_node,
                ps_node,
                pu1_cabac_ctxt,
                MAX_TU_SIZE,
                MIN_TU_SIZE,
                0,
                (u1_cur_depth == u1_max_depth) ? 0 : 1,
                i4_pred_mode == PRED_MODE_INTRA,
                (u1_part_type == PART_NxN) && (i4_pred_mode == PRED_MODE_INTRA),
                u1_chroma_processing_enabled,
                u1_is_422);

            ps_node->s_luma_data.i8_cost += COMPUTE_RATE_COST_CLIP30(
                u4_tuSplitFlag_and_cbf_coding_bits,
                ps_ctxt->i8_cl_ssd_lambda_qf,
                (LAMBDA_Q_SHIFT + CABAC_FRAC_BITS_Q));
        }

        i8_winning_cost +=
            ihevce_tu_node_cost_collator(ps_node, u1_chroma_processing_enabled, u1_is_422);

        ihevce_ecd_buffer_pointer_updater(
            ps_node, ppu1_ecd, pu1_ecd_bPtr_backup_t1, 1, u1_chroma_processing_enabled, u1_is_422);

        ihevce_nbr_data_copier(
            ps_nbr_data_buf,
            i4_nbr_data_buf_stride,
            ps_ctxt->i4_cu_qp,
            ps_node->s_luma_data.u1_cbf,
            ps_node->s_luma_data.u1_posx,
            ps_node->s_luma_data.u1_posy,
            ps_node->s_luma_data.u1_size);
    }

    return i8_winning_cost;
}

/*!
******************************************************************************
* \if Function name : ihevce_tu_selector_debriefer \endif
*
* \notes
*    Conversion of TU Tree struct into TU info array. Collection of myriad CU
*    level data
*    Input : 1. ps_node: Pointer to current node of the TU tree. This struct
*            shall be modified by this function
*            2. ps_final_prms: Pointer to struct that stores RDOPT output data.
*            This buffer shall be modified by this function
*    Output : 1. pi8_total_cost: Total CU-level cost
*             2. pi8_total_non_coded_cost: Total CU level cost when no residue
*             is coded
*             3. pi4_num_bytes_used_for_ecd: Number of bytes used for storing
*             entropy coding data
*             4. pi4_num_bits_used_for_encoding: Number of bits used for encoding
*             5. pu2_tu_ctr: Number of TU's in the CU
*
*****************************************************************************
*/
void ihevce_tu_selector_debriefer(
    tu_tree_node_t *ps_node,
    enc_loop_cu_final_prms_t *ps_final_prms,
    LWORD64 *pi8_total_cost,
    LWORD64 *pi8_total_non_coded_cost,
    WORD32 *pi4_num_bytes_used_for_ecd,
    WORD32 *pi4_num_bits_used_for_encoding,
    UWORD16 *pu2_tu_ctr,
    WORD32 i4_cu_qp,
    UWORD8 u1_cu_posx,
    UWORD8 u1_cu_posy,
    UWORD8 u1_chroma_processing_enabled,
    UWORD8 u1_is_422,
    TU_POS_T e_tu_pos)
{
    UWORD8 u1_is_chroma_tu_valid = 1;
    WORD32 i4_log2_size;

    ASSERT(ps_node != NULL);

    if(ps_node->u1_is_valid_node)
    {
        ASSERT(
            (NULL == ps_node->ps_child_node_tl) && (NULL == ps_node->ps_child_node_tr) &&
            (NULL == ps_node->ps_child_node_bl) && (NULL == ps_node->ps_child_node_br));
    }
    else
    {
        ASSERT(
            !((NULL == ps_node->ps_child_node_tl) || (NULL == ps_node->ps_child_node_tr) ||
              (NULL == ps_node->ps_child_node_bl) || (NULL == ps_node->ps_child_node_br)));
    }

    if(ps_node->u1_is_valid_node)
    {
        if((4 == ps_node->s_luma_data.u1_size) && (POS_TL != e_tu_pos))
        {
            u1_is_chroma_tu_valid = INTRA_PRED_CHROMA_IDX_NONE;
        }

        GETRANGE(i4_log2_size, ps_node->s_luma_data.u1_size);

        ps_final_prms->s_recon_datastore.au1_bufId_with_winning_LumaRecon[pu2_tu_ctr[0]] =
            ps_node->s_luma_data.u1_reconBufId;
        ps_final_prms->u4_cu_sad += ps_node->s_luma_data.u4_sad;
        ps_final_prms->u1_is_cu_coded |= ps_node->s_luma_data.u1_cbf;
        ps_final_prms->u4_cu_luma_res_bits += ps_node->s_luma_data.i4_bits;

        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].i4_luma_coeff_offset =
            pi4_num_bytes_used_for_ecd[0];
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_y_cbf = ps_node->s_luma_data.u1_cbf;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cb_cbf = 0;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cr_cbf = 0;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cb_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cr_cbf_subtu1 = 0;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b3_chroma_intra_mode_idx =
            u1_is_chroma_tu_valid;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b7_qp = i4_cu_qp;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_first_tu_in_cu =
            (!ps_node->s_luma_data.u1_posx && !ps_node->s_luma_data.u1_posx);
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_transquant_bypass = 0;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b3_size = i4_log2_size - 3;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b4_pos_x =
            (u1_cu_posx + ps_node->s_luma_data.u1_posx) / 4;
        ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b4_pos_y =
            (u1_cu_posy + ps_node->s_luma_data.u1_posy) / 4;

        ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].i2_luma_bytes_consumed =
            ps_node->s_luma_data.i4_num_bytes_used_for_ecd;
        ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].u4_luma_zero_col =
            ps_node->s_luma_data.i4_zero_col;
        ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].u4_luma_zero_row =
            ps_node->s_luma_data.i4_zero_row;

        pi8_total_cost[0] += ps_node->s_luma_data.i8_cost;
        pi8_total_non_coded_cost[0] += ps_node->s_luma_data.i8_not_coded_cost;
        pi4_num_bytes_used_for_ecd[0] += ps_node->s_luma_data.i4_num_bytes_used_for_ecd;
        pi4_num_bits_used_for_encoding[0] += ps_node->s_luma_data.i4_bits;

        if(u1_chroma_processing_enabled)
        {
            UWORD8 i;

            for(i = 0; i < u1_is_422 + 1; i++)
            {
                ps_final_prms->s_recon_datastore
                    .au1_bufId_with_winning_ChromaRecon[U_PLANE][pu2_tu_ctr[0]][i] =
                    ps_node->as_cb_data[i].u1_reconBufId;
                ps_final_prms->u1_is_cu_coded |= ps_node->as_cb_data[i].u1_cbf;
                ps_final_prms->u4_cu_chroma_res_bits += ps_node->as_cb_data[i].i4_bits;

                ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].ai4_cb_coeff_offset[i] =
                    pi4_num_bytes_used_for_ecd[0];

                if(!i)
                {
                    ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cb_cbf =
                        ps_node->as_cb_data[i].u1_cbf;
                }
                else
                {
                    ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cb_cbf_subtu1 =
                        ps_node->as_cb_data[i].u1_cbf;
                }

                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].ai2_cb_bytes_consumed[i] =
                    ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd;
                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].au4_cb_zero_col[i] =
                    ps_node->as_cb_data[i].i4_zero_col;
                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].au4_cb_zero_row[i] =
                    ps_node->as_cb_data[i].i4_zero_row;

                pi8_total_cost[0] += ps_node->as_cb_data[i].i8_cost;
                pi8_total_non_coded_cost[0] += ps_node->as_cb_data[i].i8_not_coded_cost;
                pi4_num_bytes_used_for_ecd[0] += ps_node->as_cb_data[i].i4_num_bytes_used_for_ecd;
                pi4_num_bits_used_for_encoding[0] += ps_node->as_cb_data[i].i4_bits;
            }

            for(i = 0; i < u1_is_422 + 1; i++)
            {
                ps_final_prms->s_recon_datastore
                    .au1_bufId_with_winning_ChromaRecon[V_PLANE][pu2_tu_ctr[0]][i] =
                    ps_node->as_cr_data[i].u1_reconBufId;
                ps_final_prms->u1_is_cu_coded |= ps_node->as_cr_data[i].u1_cbf;
                ps_final_prms->u4_cu_chroma_res_bits += ps_node->as_cr_data[i].i4_bits;

                ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].ai4_cr_coeff_offset[i] =
                    pi4_num_bytes_used_for_ecd[0];

                if(!i)
                {
                    ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cr_cbf =
                        ps_node->as_cr_data[i].u1_cbf;
                }
                else
                {
                    ps_final_prms->as_tu_enc_loop[pu2_tu_ctr[0]].s_tu.b1_cr_cbf_subtu1 =
                        ps_node->as_cr_data[i].u1_cbf;
                }

                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].ai2_cr_bytes_consumed[i] =
                    ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd;
                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].au4_cr_zero_col[i] =
                    ps_node->as_cr_data[i].i4_zero_col;
                ps_final_prms->as_tu_enc_loop_temp_prms[pu2_tu_ctr[0]].au4_cr_zero_row[i] =
                    ps_node->as_cr_data[i].i4_zero_row;

                pi8_total_cost[0] += ps_node->as_cr_data[i].i8_cost;
                pi8_total_non_coded_cost[0] += ps_node->as_cr_data[i].i8_not_coded_cost;
                pi4_num_bytes_used_for_ecd[0] += ps_node->as_cr_data[i].i4_num_bytes_used_for_ecd;
                pi4_num_bits_used_for_encoding[0] += ps_node->as_cr_data[i].i4_bits;
            }
        }

        pu2_tu_ctr[0]++;
    }
    else
    {
        ihevce_tu_selector_debriefer(
            ps_node->ps_child_node_tl,
            ps_final_prms,
            pi8_total_cost,
            pi8_total_non_coded_cost,
            pi4_num_bytes_used_for_ecd,
            pi4_num_bits_used_for_encoding,
            pu2_tu_ctr,
            i4_cu_qp,
            u1_cu_posx,
            u1_cu_posy,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_TL);

        ihevce_tu_selector_debriefer(
            ps_node->ps_child_node_tr,
            ps_final_prms,
            pi8_total_cost,
            pi8_total_non_coded_cost,
            pi4_num_bytes_used_for_ecd,
            pi4_num_bits_used_for_encoding,
            pu2_tu_ctr,
            i4_cu_qp,
            u1_cu_posx,
            u1_cu_posy,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_TR);

        ihevce_tu_selector_debriefer(
            ps_node->ps_child_node_bl,
            ps_final_prms,
            pi8_total_cost,
            pi8_total_non_coded_cost,
            pi4_num_bytes_used_for_ecd,
            pi4_num_bits_used_for_encoding,
            pu2_tu_ctr,
            i4_cu_qp,
            u1_cu_posx,
            u1_cu_posy,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_BL);

        ihevce_tu_selector_debriefer(
            ps_node->ps_child_node_br,
            ps_final_prms,
            pi8_total_cost,
            pi8_total_non_coded_cost,
            pi4_num_bytes_used_for_ecd,
            pi4_num_bits_used_for_encoding,
            pu2_tu_ctr,
            i4_cu_qp,
            u1_cu_posx,
            u1_cu_posy,
            u1_chroma_processing_enabled,
            u1_is_422,
            POS_BR);
    }
}

static UWORD8 ihevce_get_curTUSplit_from_TUSplitArray(
    WORD32 ai4_tuSplitArray[4], UWORD8 u1_cu_size, UWORD8 u1_tu_size, UWORD8 u1_posx, UWORD8 u1_posy)
{
    UWORD8 u1_is_split = 0;

    UWORD8 u1_tuSplitArrayIndex = 0;
    UWORD8 u1_bit_index = 0;

    switch(u1_cu_size)
    {
    case 8:
    {
        switch(u1_tu_size)
        {
        case 8:
        {
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 4:
        {
            u1_is_split = 0;

            break;
        }
        }

        break;
    }
    case 16:
    {
        switch(u1_tu_size)
        {
        case 16:
        {
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 8:
        {
            u1_bit_index += ((u1_posx / 8) % 2) + 2 * ((u1_posy / 8) % 2) + 1;
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 4:
        {
            u1_is_split = 0;

            break;
        }
        }

        break;
    }
    case 32:
    {
        switch(u1_tu_size)
        {
        case 32:
        {
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 16:
        {
            u1_bit_index += 5 * ((u1_posx / 16) % 2) + 10 * ((u1_posy / 16) % 2) + 1;
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 8:
        {
            u1_bit_index = 5 * ((u1_posx / 16) % 2) + 10 * ((u1_posy / 16) % 2) + 1;
            u1_bit_index += ((u1_posx / 8) % 2) + 2 * ((u1_posy / 8) % 2) + 1;
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 4:
        {
            u1_is_split = 0;

            break;
        }
        }

        break;
    }
    case 64:
    {
        switch(u1_tu_size)
        {
        case 64:
        {
            u1_is_split = 1;

            break;
        }
        case 32:
        {
            u1_tuSplitArrayIndex = ((u1_posx / 32) % 2) + 2 * ((u1_posy / 32) % 2);
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 16:
        {
            u1_tuSplitArrayIndex = ((u1_posx / 32) % 2) + 2 * ((u1_posy / 32) % 2);
            u1_bit_index += 5 * ((u1_posx / 16) % 2) + 10 * ((u1_posy / 16) % 2) + 1;
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 8:
        {
            u1_tuSplitArrayIndex = ((u1_posx / 32) % 2) + 2 * ((u1_posy / 32) % 2);
            u1_bit_index += 5 * ((u1_posx / 16) % 2) + 10 * ((u1_posy / 16) % 2) + 1;
            u1_bit_index += ((u1_posx / 8) % 2) + 2 * ((u1_posy / 8) % 2) + 1;
            u1_is_split = !!(ai4_tuSplitArray[u1_tuSplitArrayIndex] & BIT_EN(u1_bit_index));

            break;
        }
        case 4:
        {
            u1_is_split = 0;

            break;
        }
        }

        break;
    }
    }

    return u1_is_split;
}

/*!
******************************************************************************
* \if Function name : ihevce_tuSplitArray_to_tuTree_mapper \endif
*
* \notes
*    This function assumes that ihevce_tu_tree_init' has been called already.
*    The pointers to the children nodes of the leaf-most nodes in the tree
*    are assigned NULL
*    Input : 1. ps_root: Pointer to root of the tree containing TU info.
*            This struct shall be modified by this function
*            2. ai4_tuSplitArray: Array containing information about TU splits
*    Output : 1. TU tree is modified such that it reflects the information
*             coded in ai4_tuSplitArray
*
*****************************************************************************
*/
void ihevce_tuSplitArray_to_tuTree_mapper(
    tu_tree_node_t *ps_root,
    WORD32 ai4_tuSplitArray[4],
    UWORD8 u1_cu_size,
    UWORD8 u1_tu_size,
    UWORD8 u1_min_tu_size,
    UWORD8 u1_max_tu_size,
    UWORD8 u1_is_skip)
{
    UWORD8 u1_is_split;

    ASSERT(u1_min_tu_size >= MIN_TU_SIZE);
    ASSERT(u1_max_tu_size <= MAX_TU_SIZE);
    ASSERT(u1_min_tu_size <= u1_max_tu_size);

    ASSERT(!u1_is_skip);

    ASSERT(ps_root != NULL);
    ASSERT(ps_root->s_luma_data.u1_size == u1_tu_size);

    if(u1_tu_size <= u1_max_tu_size)
    {
        ASSERT(ps_root->u1_is_valid_node);
    }
    else
    {
        ASSERT(!ps_root->u1_is_valid_node);
    }

    if(u1_tu_size > u1_min_tu_size)
    {
        ASSERT(ps_root->ps_child_node_tl != NULL);
        ASSERT(ps_root->ps_child_node_tr != NULL);
        ASSERT(ps_root->ps_child_node_bl != NULL);
        ASSERT(ps_root->ps_child_node_br != NULL);
        ASSERT(ps_root->ps_child_node_tl->s_luma_data.u1_size == (u1_tu_size / 2));
        ASSERT(ps_root->ps_child_node_tr->s_luma_data.u1_size == (u1_tu_size / 2));
        ASSERT(ps_root->ps_child_node_bl->s_luma_data.u1_size == (u1_tu_size / 2));
        ASSERT(ps_root->ps_child_node_br->s_luma_data.u1_size == (u1_tu_size / 2));
        ASSERT(ps_root->ps_child_node_tl->u1_is_valid_node);
        ASSERT(ps_root->ps_child_node_tr->u1_is_valid_node);
        ASSERT(ps_root->ps_child_node_bl->u1_is_valid_node);
        ASSERT(ps_root->ps_child_node_br->u1_is_valid_node);
    }
    else
    {
        ASSERT(ps_root->ps_child_node_tl == NULL);
        ASSERT(ps_root->ps_child_node_tr == NULL);
        ASSERT(ps_root->ps_child_node_bl == NULL);
        ASSERT(ps_root->ps_child_node_br == NULL);
    }

    u1_is_split = ihevce_get_curTUSplit_from_TUSplitArray(
        ai4_tuSplitArray,
        u1_cu_size,
        u1_tu_size,
        ps_root->s_luma_data.u1_posx,
        ps_root->s_luma_data.u1_posy);

    if(u1_tu_size == u1_min_tu_size)
    {
        ASSERT(!u1_is_split);
    }

    if(u1_is_split)
    {
        ps_root->u1_is_valid_node = 0;

        ihevce_tuSplitArray_to_tuTree_mapper(
            ps_root->ps_child_node_tl,
            ai4_tuSplitArray,
            u1_cu_size,
            ps_root->ps_child_node_tl->s_luma_data.u1_size,
            u1_min_tu_size,
            u1_max_tu_size,
            u1_is_skip);

        ihevce_tuSplitArray_to_tuTree_mapper(
            ps_root->ps_child_node_tr,
            ai4_tuSplitArray,
            u1_cu_size,
            ps_root->ps_child_node_tr->s_luma_data.u1_size,
            u1_min_tu_size,
            u1_max_tu_size,
            u1_is_skip);

        ihevce_tuSplitArray_to_tuTree_mapper(
            ps_root->ps_child_node_bl,
            ai4_tuSplitArray,
            u1_cu_size,
            ps_root->ps_child_node_bl->s_luma_data.u1_size,
            u1_min_tu_size,
            u1_max_tu_size,
            u1_is_skip);

        ihevce_tuSplitArray_to_tuTree_mapper(
            ps_root->ps_child_node_br,
            ai4_tuSplitArray,
            u1_cu_size,
            ps_root->ps_child_node_br->s_luma_data.u1_size,
            u1_min_tu_size,
            u1_max_tu_size,
            u1_is_skip);
    }
    else
    {
        ps_root->ps_child_node_tl = NULL;
        ps_root->ps_child_node_tr = NULL;
        ps_root->ps_child_node_bl = NULL;
        ps_root->ps_child_node_br = NULL;
    }
}
