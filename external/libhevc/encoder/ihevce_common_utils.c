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
* \file ihevce_common_utils.c
*
* \brief
*    Contains definitions of common utility functions used across encoder
*
* \date
*    18/09/2012
*
* \author
*    ittiam
*
* List of Functions
*  ihevce_copy_2d()
*  ihevce_hbd_copy_2d()
*  ihevce_2d_square_copy_luma()
*  ihevce_wt_avg_2d()
*  ihevce_itrans_recon_dc_compute()
*  ihevce_itrans_recon_dc()
*  ihevce_hbd_itrans_recon_dc()
*  ihevce_truncate_16bit_data_to_8bit()
*  ihevce_convert_16bit_recon_to_8bit()
*  ihevce_convert_16bit_input_to_8bit()
*  ihevce_find_num_clusters_of_identical_points_1D()
*  ihevce_hbd_compute_ssd()
*  ihevce_compare_pu_mv_t()
*  ihevce_set_pred_buf_as_free()
*  ihevce_get_free_pred_buf_indices()
*  ihevce_scale_mv()
*  ihevce_osal_alloc()
*  ihevce_osal_free()
*  ihevce_osal_init()
*  ihevce_osal_delete()
*  ihevce_sum_abs_seq()
*  ihevce_ssd_calculator()
*  ihevce_chroma_interleave_ssd_calculator()
*  ihevce_ssd_and_sad_calculator()
*  ihevce_chroma_interleave_2d_copy()
*  ihevce_hbd_chroma_interleave_2d_copy()
*  ihevce_hbd_chroma_interleave_ssd_calculator()
*  ihevce_get_chroma_eo_sao_params()
*  ihevce_get_chroma_eo_sao_params_hbd()
*  ihevce_compute_area_of_valid_cus_in_ctb()
*  ihevce_create_cuNode_children()
*  ihevce_cu_tree_init()
*
******************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

/* System include files */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevce_api.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevc_defs.h"
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
#include "ihevce_common_utils.h"
#include "ihevce_global_tables.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Performs the 2D copy
*
*  @par   Description
*  This routine Performs the 2D copy
*
*  @param[inout]   pu1_dst
*  pointer to the destination buffer
*
*  @param[in]   dst_strd
*  destination stride in terms of the size of input/output unit
*
*  @param[inout]   pu1_src
*  pointer to the source buffer
*
*  @param[in]   src_strd
*  source stride in terms of the size of input/output unit
*
*  @param[in]   blk_wd
*  number of samples to copy in a row
*
*  @param[in]   blk_ht
*  number of rows to copy
*
******************************************************************************
*/
void ihevce_copy_2d(
    UWORD8 *pu1_dst,
    WORD32 dst_stride,
    UWORD8 *pu1_src,
    WORD32 src_stride,
    WORD32 blk_wd,
    WORD32 blk_ht)
{
    WORD32 i;

    for(i = 0; i < blk_ht; i++)
    {
        memcpy(pu1_dst, pu1_src, blk_wd);
        pu1_dst += dst_stride;
        pu1_src += src_stride;
    }
}

/**
******************************************************************************
*
*  @brief Performs the 2D copy of luma data
*
*  @par   Description
*  This routine performs the 2D square copy of luma data
*
*  @param[inout]   p_dst
*  pointer to the destination buffer
*
*  @param[in]   dst_strd
*  destination stride in terms of the size of input/output unit
*
*  @param[inout]   p_src
*  pointer to the source buffer
*
*  @param[in]   src_strd
*  source stride in terms of the size of input/output unit
*
*  @param[in]   num_cols_to_copy
*  number of units in a line to copy from src to dst buffer
*  Assumption : num_cols_to_copy <= min (dst_strd, src_strd)
*
*  @param[in]   unit_size
*  size of the unit in bytes
*
*  @return      none
*
*  Assumptions : num_cols_to_copy = num_lines_to_copy,
*  num_lines_to_copy can have {4, 16, 32, 64}
*
******************************************************************************
*/
void ihevce_2d_square_copy_luma(
    void *p_dst,
    WORD32 dst_strd,
    void *p_src,
    WORD32 src_strd,
    WORD32 num_cols_to_copy,
    WORD32 unit_size)
{
    UWORD8 *pu1_dst = (UWORD8 *)p_dst;
    UWORD8 *pu1_src = (UWORD8 *)p_src;
    WORD32 i;

    for(i = 0; i < num_cols_to_copy; i++)
    {
        memcpy(pu1_dst, pu1_src, (num_cols_to_copy * unit_size));
        pu1_dst += (dst_strd * unit_size);
        pu1_src += (src_strd * unit_size);
    }
}

/**
********************************************************************************
*
*  @brief  Weighted pred of 2 predictor buffers as per spec
*
*  @param[in] pu1_pred0 : Pred0 buffer
*
*  @param[in] pu1_pred1 : Pred1 buffer
*
*  @param[in] pred0_strd : Stride of pred0 buffer
*
*  @param[in] pred1_strd : Stride of pred1 buffer
*
*  @param[in] wd : Width of pred block
*
*  @param[in] ht : Height of pred block
*
*  @param[out] pu1_dst : Destination buffer that will hold result
*
*  @param[in] dst_strd : Stride of dest buffer
*
*  @param[in] w0 : Weighting factor of Pred0
*
*  @param[in] w1 : weighting factor of pred1
*
*  @param[in] o0 : offset for pred0
*
*  @param[in] o1 : offset for pred1
*
*  @param[in] log_wdc : shift factor as per spec
*
*  @return none
*
********************************************************************************
*/
void ihevce_wt_avg_2d(
    UWORD8 *pu1_pred0,
    UWORD8 *pu1_pred1,
    WORD32 pred0_strd,
    WORD32 pred1_strd,
    WORD32 wd,
    WORD32 ht,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 w0,
    WORD32 w1,
    WORD32 o0,
    WORD32 o1,
    WORD32 log_wdc)
{
    /* Total Rounding term to be added, including offset */
    WORD32 rnd = (o0 + o1 + 1) >> 1;  // << log_wdc;
    /* Downshift */
    WORD32 shift = log_wdc + 1;
    /* loop counters */
    WORD32 i, j;

    /* Dst = ((w0*p0 + w1*p1) + ((o0 + o1 + 1) << logWDc)) >> (logWDc + 1) */
    /* In above formula, the additive term is constant and is evaluated    */
    /* outside loop and stored as "rnd".                                   */
    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            WORD32 tmp;
            tmp = IHEVCE_WT_PRED(pu1_pred0[j], pu1_pred1[j], w0, w1, rnd, shift);
            pu1_dst[j] = (UWORD8)(CLIP3(tmp, 0, 255));
        }
        pu1_pred0 += pred0_strd;
        pu1_pred1 += pred1_strd;
        pu1_dst += dst_strd;
    }
}
/**
******************************************************************************
*
*  @brief Performs the Recon for DC only coefficient case
*
*  @par   Description
*  This routine performs the Recon for DC only coefficient case
*
*  @param[inout]   pu1_dst
*  pointer to the destination buffer
*
*  @param[in]   pu1_pred
*  pointer to the pred buffer
*
*  @param[in]   dst_strd
*  destination stride
*
*  @param[in]   pred_strd
*  pred buffer stride
*
*  @param[in]   trans_size
*  transform size
*
* @param[in] col_mult
*  chroma multiplier
*
*  @param[in]   dc_value
*  residue value
*
*  @return      none
*
******************************************************************************
*/
static INLINE void ihevce_itrans_recon_dc_compute(
    UWORD8 *pu1_dst,
    UWORD8 *pu1_pred,
    WORD32 dst_strd,
    WORD32 pred_strd,
    WORD32 trans_size,
    WORD32 col_mult,
    WORD32 dc_value)
{
    WORD32 row, col;

    for(row = 0; row < trans_size; row++)
    {
        for(col = 0; col < trans_size; col++)
        {
            pu1_dst[row * dst_strd + col * col_mult] =
                CLIP_U8(pu1_pred[row * pred_strd + col * col_mult] + dc_value);
        }
    }
}

/**
******************************************************************************
*
*  @brief Performs the IQ+IT+Recon for DC only coefficient case
*
*  @par   Description
*  This routine performs the IQ+IT+Recon for DC only coefficient case
*
*  @param[in]   pu1_pred
*  pointer to the pred buffer
*
*  @param[in]   pred_strd
*  pred buffer stride
*
*  @param[inout]   pu1_dst
*  pointer to the destination buffer
*
*  @param[in]   dst_strd
*  destination stride
*
*  @param[in]   trans_size
*  transform size
*
* @param[in] i2_deq_value
*  Dequant Coeffs
*
*  @param[in] chroma plane
*  -1 : luma, 0 : chroma U, 1 : chroma V
*
*  @return      none
*
******************************************************************************
*/
void ihevce_itrans_recon_dc(
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 trans_size,
    WORD16 i2_deq_value,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 add, shift;
    WORD32 dc_value;
    UWORD8 *pu1_pred_tmp, *pu1_dst_tmp;
    WORD32 col_mult;

    assert(e_chroma_plane == NULL_PLANE || e_chroma_plane == U_PLANE || e_chroma_plane == V_PLANE);
    if(e_chroma_plane == NULL_PLANE)
    {
        pu1_pred_tmp = pu1_pred;
        pu1_dst_tmp = pu1_dst;
        col_mult = 1;
    }
    else
    {
        col_mult = 2;
        pu1_pred_tmp = pu1_pred + e_chroma_plane;
        pu1_dst_tmp = pu1_dst + e_chroma_plane;
    }

    shift = IT_SHIFT_STAGE_1;
    add = 1 << (shift - 1);
    dc_value = CLIP_S16((i2_deq_value * 64 + add) >> shift);
    shift = IT_SHIFT_STAGE_2;
    add = 1 << (shift - 1);
    dc_value = CLIP_S16((dc_value * 64 + add) >> shift);
    ihevce_itrans_recon_dc_compute(
        pu1_dst_tmp, pu1_pred_tmp, dst_strd, pred_strd, trans_size, col_mult, dc_value);
}

/*!
******************************************************************************
* \if Function name : ihevce_find_num_clusters_of_identical_points_1D \endif
*
* \brief
*
*
*****************************************************************************
*/
WORD32 ihevce_find_num_clusters_of_identical_points_1D(
    UWORD8 *pu1_inp_array,
    UWORD8 *pu1_out_array,
    UWORD8 *pu1_freq_of_out_data_in_inp,
    WORD32 i4_num_inp_array_elements)
{
    WORD32 i;
    UWORD8 u1_value = pu1_inp_array[0];
    WORD32 i4_num_clusters = i4_num_inp_array_elements;
    WORD32 i4_output_array_idx = 1;

    pu1_freq_of_out_data_in_inp[0] = 1;
    pu1_out_array[0] = u1_value;

    if(1 == i4_num_inp_array_elements)
    {
        return 1;
    }

    for(i = 1; i < i4_num_inp_array_elements; i++)
    {
        if(pu1_inp_array[i] == u1_value)
        {
            pu1_freq_of_out_data_in_inp[0]++;
            i4_num_clusters--;
        }
        else
        {
            pu1_out_array[i4_output_array_idx] = pu1_inp_array[i];

            i4_output_array_idx++;
        }
    }

    if(i4_num_clusters > 1)
    {
        WORD32 i4_num_sub_clusters;

        i4_num_sub_clusters = ihevce_find_num_clusters_of_identical_points_1D(
            &pu1_out_array[1],
            &pu1_out_array[1],
            &pu1_freq_of_out_data_in_inp[1],
            i4_num_clusters - 1);

        i4_num_clusters = 1 + i4_num_sub_clusters;
    }

    return i4_num_clusters;
}

/**
*******************************************************************************
*
* @brief Compare Motion vectors function
*
* @par Description:
*   Checks if MVs and Reference idx are excatly matching.
*
* @param[inout] ps_1
*   motion vector 1 to be compared
*
* @param[in] ps_2
*   motion vector 2 to be compared
*
* @returns
*  0 : if not matching 1 : if matching
*
* @remarks
*
*******************************************************************************
*/
WORD32 ihevce_compare_pu_mv_t(
    pu_mv_t *ps_pu_mv_1, pu_mv_t *ps_pu_mv_2, WORD32 i4_pred_mode_1, WORD32 i4_pred_mode_2)
{
    WORD32 i4_l0_match, i4_l1_match;
    WORD32 i4_pred_l0, i4_pred_l1;

    i4_pred_l0 = (i4_pred_mode_1 != PRED_L1);
    i4_pred_l1 = (i4_pred_mode_1 != PRED_L0);

    if(i4_pred_mode_1 != i4_pred_mode_2)
        return 0;

    i4_l0_match = 0;
    i4_l1_match = 0;

    if(i4_pred_l0)
    {
        if(ps_pu_mv_1->i1_l0_ref_idx == ps_pu_mv_2->i1_l0_ref_idx)
        {
            if(0 == memcmp(&ps_pu_mv_1->s_l0_mv, &ps_pu_mv_2->s_l0_mv, sizeof(mv_t)))
                i4_l0_match = 1;
        }
    }
    if(i4_pred_l1)
    {
        if(ps_pu_mv_1->i1_l1_ref_idx == ps_pu_mv_2->i1_l1_ref_idx)
        {
            if(0 == memcmp(&ps_pu_mv_1->s_l1_mv, &ps_pu_mv_2->s_l1_mv, sizeof(mv_t)))
                i4_l1_match = 1;
        }
    }

    if(i4_pred_l0 && i4_pred_l1)
        return (i4_l0_match & i4_l1_match);
    else if(i4_pred_l0)
        return i4_l0_match;
    else
        return i4_l1_match;

} /* End of ihevce_compare_pu_mv_t */

/*!
******************************************************************************
* \if Function name : ihevce_set_pred_buf_as_free \endif
*
* \brief
*    Mark buffer as free
*
*****************************************************************************
*/
void ihevce_set_pred_buf_as_free(UWORD32 *pu4_idx_array, UWORD8 u1_buf_id)
{
    (*pu4_idx_array) &= ~(1 << u1_buf_id);
}

/*!
******************************************************************************
* \if Function name : ihevce_get_free_pred_buf_indices \endif
*
* \brief
*    get free buffer indices
*
*****************************************************************************
*/
UWORD8 ihevce_get_free_pred_buf_indices(
    UWORD8 *pu1_idx_array, UWORD32 *pu4_bitfield, UWORD8 u1_num_bufs_requested)
{
    UWORD8 i;

    UWORD8 u1_num_free_bufs_found = 0;
    UWORD32 u4_local_bitfield = *pu4_bitfield;

    ASSERT(u1_num_bufs_requested <= (32 - ihevce_num_ones_generic(u4_local_bitfield)));

    for(i = 0; u1_num_free_bufs_found < u1_num_bufs_requested; i++)
    {
        if(!(u4_local_bitfield & (1 << i)))
        {
            pu1_idx_array[u1_num_free_bufs_found++] = i;
            u4_local_bitfield |= (1 << i);
        }
    }

    (*pu4_bitfield) = u4_local_bitfield;

    return u1_num_free_bufs_found;
}

/*!
******************************************************************************
* \if Function name : ihevce_scale_mv \endif
*
* \brief
*    Scale mv basing on displacement of POC
*
*****************************************************************************
*/
void ihevce_scale_mv(mv_t *ps_mv, WORD32 i4_poc_to, WORD32 i4_poc_from, WORD32 i4_curr_poc)
{
    WORD32 td, tb, tx;
    WORD32 dist_scale_factor;
    WORD32 mvx, mvy;

    td = CLIP_S8(i4_curr_poc - i4_poc_from);
    tb = CLIP_S8(i4_curr_poc - i4_poc_to);

    tx = (16384 + (abs(td) >> 1)) / td;

    dist_scale_factor = (tb * tx + 32) >> 6;
    dist_scale_factor = CLIP3(dist_scale_factor, -4096, 4095);

    mvx = ps_mv->i2_mvx;
    mvy = ps_mv->i2_mvy;

    mvx = SIGN(dist_scale_factor * mvx) * ((abs(dist_scale_factor * mvx) + 127) >> 8);
    mvy = SIGN(dist_scale_factor * mvy) * ((abs(dist_scale_factor * mvy) + 127) >> 8);

    ps_mv->i2_mvx = CLIP_S16(mvx);
    ps_mv->i2_mvy = CLIP_S16(mvy);
}

/*!
******************************************************************************
* \if Function name : ihevce_osal_alloc \endif
*
* \brief
*    Memory allocate call back function passed to OSAL
*
* \param[in] pv_handle : handle to hle ctxt
* \param[in] u4_size : size of memory required
*
* \return
*    Memory pointer
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_osal_alloc(void *pv_handle, UWORD32 u4_size)
{
    ihevce_hle_ctxt_t *ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_handle;
    iv_mem_rec_t s_mem_tab;

    /* def init of memtab */
    s_mem_tab.i4_size = sizeof(iv_mem_rec_t);
    s_mem_tab.i4_mem_alignment = 8;
    s_mem_tab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

    /* allocate memory for required size */
    s_mem_tab.i4_mem_size = u4_size;

    ps_hle_ctxt->ihevce_mem_alloc(
        ps_hle_ctxt->pv_mem_mgr_hdl, &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api, &s_mem_tab);

    return (s_mem_tab.pv_base);
}

/*!
******************************************************************************
* \if Function name : ihevce_osal_free \endif
*
* \brief
*    Memory free call back function passed to OSAL
*
* \param[in] pv_handle : handle to hle ctxt
* \param[in] pv_mem : memory to be freed
*
* \return
*    none
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_osal_free(void *pv_handle, void *pv_mem)
{
    ihevce_hle_ctxt_t *ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_handle;
    iv_mem_rec_t s_mem_tab;

    /* def init of memtab */
    s_mem_tab.i4_size = sizeof(iv_mem_rec_t);
    s_mem_tab.i4_mem_alignment = 8;
    s_mem_tab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

    /* free memory */
    s_mem_tab.pv_base = pv_mem;

    ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_mem_tab);

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_osal_init \endif
*
* \brief
*    Function to initialise OSAL handle
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_osal_init(void *pv_hle_ctxt)
{
    /* local variables */
    ihevce_hle_ctxt_t *ps_hle_ctxt;
    osal_cb_funcs_t s_cb_funcs;
    WORD32 status = 0;
    void *pv_osal_handle;
    iv_mem_rec_t s_mem_tab;

    ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_hle_ctxt;

    /* def init of memtab */
    s_mem_tab.i4_size = sizeof(iv_mem_rec_t);
    s_mem_tab.i4_mem_alignment = 8;
    s_mem_tab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

    /* --------------------------------------------------------------------- */
    /*                      OSAL Hanndle create                              */
    /* --------------------------------------------------------------------- */

    /* Allocate memory for the handle */
    s_mem_tab.i4_mem_size = OSAL_HANDLE_SIZE;

    ps_hle_ctxt->ihevce_mem_alloc(
        ps_hle_ctxt->pv_mem_mgr_hdl, &ps_hle_ctxt->ps_static_cfg_prms->s_sys_api, &s_mem_tab);
    if(NULL == s_mem_tab.pv_base)
    {
        ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.ihevce_printf(
            ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.pv_cb_handle,
            "IHEVCE ERROR: Error in OSAL initialization\n");
        return (-1);
    }

    pv_osal_handle = s_mem_tab.pv_base;

    /* Initialize OSAL call back functions */
    s_cb_funcs.mmr_handle = (void *)ps_hle_ctxt;
    s_cb_funcs.osal_alloc = &ihevce_osal_alloc;
    s_cb_funcs.osal_free = &ihevce_osal_free;

    status = osal_init(pv_osal_handle);
    if(OSAL_SUCCESS != status)
    {
        ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.ihevce_printf(
            ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.pv_cb_handle,
            "IHEVCE ERROR: Error in OSAL initialization\n");
        return (-1);
    }

    status = osal_register_callbacks(pv_osal_handle, &s_cb_funcs);
    if(OSAL_SUCCESS != status)
    {
        ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.ihevce_printf(
            ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.pv_cb_handle,
            "IHEVCE ERROR: Error in OSAL initialization\n");
        return (-1);
    }
    ps_hle_ctxt->pv_osal_handle = pv_osal_handle;

    return (0);
}

/*!
******************************************************************************
* \if Function name : ihevce_osal_delete \endif
*
* \brief
*    Function to delete OSAL handle
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_osal_delete(void *pv_hle_ctxt)
{
    /* local variables */
    ihevce_hle_ctxt_t *ps_hle_ctxt;
    void *pv_osal_handle;
    iv_mem_rec_t s_mem_tab;

    ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_hle_ctxt;
    pv_osal_handle = ps_hle_ctxt->pv_osal_handle;

    /* def init of memtab */
    s_mem_tab.i4_size = sizeof(iv_mem_rec_t);
    s_mem_tab.i4_mem_alignment = 8;
    s_mem_tab.e_mem_type = IV_EXT_CACHEABLE_NORMAL_MEM;

    if(0 != osal_close(pv_osal_handle))
    {
        ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.ihevce_printf(
            ps_hle_ctxt->ps_static_cfg_prms->s_sys_api.pv_cb_handle,
            "IHEVCE ERROR>> Unable to close OSAL\n");
        return (-1);
    }

    /* free osal handle */
    s_mem_tab.pv_base = pv_osal_handle;

    ps_hle_ctxt->ihevce_mem_free(ps_hle_ctxt->pv_mem_mgr_hdl, &s_mem_tab);

    return (0);
}

/**
*******************************************************************************
*
* @brief
*  Compute SSD between two blocks (8 bit input)
*
* @par Description:
*
* @param[in] pu1_inp
*  UWORD8 pointer to the src block
*
* @param[in] pu1_ref
*  UWORD8 pointer to the ref block
*
* @param[in] inp_stride
*  UWORD32 Source stride
*
* @param[in] ref_stride
*  UWORD32 ref stride
*
* @param[in] wd
*  UWORD32 width of the block
*
* @param[in] ht
*  UWORD32 height of the block
*
* @returns SSD
*
* @remarks none
*
*******************************************************************************
*/
LWORD64 ihevce_ssd_calculator(
    UWORD8 *pu1_inp, UWORD8 *pu1_ref, UWORD32 inp_stride, UWORD32 ref_stride, UWORD32 wd,
    UWORD32 ht, CHROMA_PLANE_ID_T chroma_plane)
{
    UWORD32 i, j;
    LWORD64 ssd = 0;
    UNUSED(chroma_plane);
    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            ssd += (pu1_inp[j] - pu1_ref[j]) * (pu1_inp[j] - pu1_ref[j]);
        }

        pu1_inp += inp_stride;
        pu1_ref += ref_stride;
    }

    return ssd;
}

/**
*******************************************************************************
*
* @brief
*  Compute SSD between two blocks (8 bit input, chroma interleaved input)
*
* @par Description:
*
* @param[in] pu1_inp
*  UWORD8 pointer to the src block
*
* @param[in] pu1_ref
*  UWORD8 pointer to the ref block
*
* @param[in] inp_stride
*  UWORD32 Source stride
*
* @param[in] ref_stride
*  UWORD32 ref stride
*
* @param[in] wd
*  UWORD32 width of the block
*
* @param[in] ht
*  UWORD32 height of the block
*
* @returns SSD
*
* @remarks none
*
*******************************************************************************
*/
LWORD64 ihevce_chroma_interleave_ssd_calculator(
    UWORD8 *pu1_inp, UWORD8 *pu1_ref, UWORD32 inp_stride, UWORD32 ref_stride, UWORD32 wd,
    UWORD32 ht, CHROMA_PLANE_ID_T chroma_plane)
{
    UWORD32 i, j;
    LWORD64 ssd = 0;
    pu1_inp += chroma_plane;
    pu1_ref += chroma_plane;

    /* run a loop and find the ssd by doing diff followed by square */
    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            WORD32 val;

            /* note that chroma is interleaved */
            val = pu1_inp[j * 2] - pu1_ref[j * 2];
            ssd += val * val;
        }
        /* row level update */
        pu1_inp += inp_stride;
        pu1_ref += ref_stride;
    }

    return (ssd);
}

/**
*******************************************************************************
*
* @brief
*  Compute SSD & SAD between two blocks (8 bit input)
*
* @par Description:
*
* @param[in] pu1_recon
*  UWORD8 pointer to the block 1
*
* @param[in] recon_strd
*  UWORD32 stride of block 1
*
* @param[in] pu1_src
*  UWORD8 pointer to the block 2
*
* @param[in] src_strd
*  UWORD32 stride of block 2
*
* @param[in] trans_size
*  UWORD32 block wd/ht
*
* @param[out] *pu4_blk_sad
*  UWORD32 block SAD
*
* @returns SSD
*
* @remarks none
*
*******************************************************************************
*/
LWORD64 ihevce_ssd_and_sad_calculator(
    UWORD8 *pu1_recon,
    WORD32 recon_strd,
    UWORD8 *pu1_src,
    WORD32 src_strd,
    WORD32 trans_size,
    UWORD32 *pu4_blk_sad)
{
    WORD32 i, j, sad = 0;
    LWORD64 ssd = 0;

    /* run a loop and find the ssd by doing diff followed by square */
    for(i = 0; i < trans_size; i++)
    {
        for(j = 0; j < trans_size; j++)
        {
            WORD32 val;

            val = *pu1_src++ - *pu1_recon++;
            ssd += val * val;
            sad += abs(val);
        }
        /* row level update */
        pu1_src += src_strd - trans_size;
        pu1_recon += recon_strd - trans_size;
    }
    *pu4_blk_sad = sad;

    /* The return value is of type WORD32 */
    ssd = CLIP3(ssd, 0, 0x7fffffff);

    return (ssd);
}

/*!
******************************************************************************
* \if Function name : ihevce_chroma_interleave_2d_copy \endif
*
* \brief
*    This function copies one plane (u/v) of interleaved chroma buffer from
*    source to destination
******************************************************************************
*/
void ihevce_chroma_interleave_2d_copy(
    UWORD8 *pu1_uv_src_bp,
    WORD32 src_strd,
    UWORD8 *pu1_uv_dst_bp,
    WORD32 dst_strd,
    WORD32 w,
    WORD32 h,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    WORD32 i, j;

    UWORD8 *pu1_src = (U_PLANE == e_chroma_plane) ? pu1_uv_src_bp : pu1_uv_src_bp + 1;
    UWORD8 *pu1_dst = (U_PLANE == e_chroma_plane) ? pu1_uv_dst_bp : pu1_uv_dst_bp + 1;

    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            /* note that chroma is interleaved */
            pu1_dst[j * 2] = pu1_src[j * 2];
        }

        /* row level update */
        pu1_src += src_strd;
        pu1_dst += dst_strd;
    }
}

/**
*******************************************************************************
*
* @brief
*     Gets edge offset params
*
* @par Description:
*     Given the ctb and sao angle this function will calculate accumulated
*     error between source and recon and the corresponding count for 4 edge
*     indexes one each for peak,valley, half peak and half valley.
*
* @param[in]
*   ps_sao_ctxt:   Pointer to SAO context
*   eo_sao_class: specifies edge offset class
*   pi4_acc_error_category: pointer to an array to store accumulated error between source and recon
*   pi4_category_count    : pointer to an array to store number of peaks,valleys,half peaks and half valleys.
* @returns
*
* @remarks
*  None
*
*******************************************************************************/
void ihevce_get_chroma_eo_sao_params(
    void *pv_sao_ctxt,
    WORD32 eo_sao_class,
    WORD32 *pi4_acc_error_category,
    WORD32 *pi4_category_count)
{
    WORD32 row_start, row_end, col_start, col_end, row, col;
    WORD32 row_offset = 0, col_offset = 0;
    WORD32 a, b, c, pel_error, edgeidx;
    sao_ctxt_t *ps_sao_ctxt = (sao_ctxt_t *)pv_sao_ctxt;

    row_start = 0;
    row_end = ps_sao_ctxt->i4_sao_blk_ht >> 1;
    col_start = 0;
    col_end = ps_sao_ctxt->i4_sao_blk_wd;

    if((ps_sao_ctxt->i4_ctb_x == 0) && (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_start = 2;
    }

    if(((ps_sao_ctxt->i4_ctb_x + 1) == ps_sao_ctxt->ps_sps->i2_pic_wd_in_ctb) &&
       (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_end = ps_sao_ctxt->i4_sao_blk_wd - 2;
    }

    if((ps_sao_ctxt->i4_ctb_y == 0) && (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_y + 1) == ps_sao_ctxt->ps_sps->i2_pic_ht_in_ctb) &&
       (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_end = row_end - 1;  //ps_sao_ctxt->i4_sao_blk_ht - 1;
    }

    if(eo_sao_class == SAO_EDGE_0_DEG)
    {
        row_offset = 0;
        col_offset = 2;
    }
    else if(eo_sao_class == SAO_EDGE_90_DEG)
    {
        row_offset = 1;
        col_offset = 0;
    }
    else if(eo_sao_class == SAO_EDGE_135_DEG)
    {
        row_offset = 1;
        col_offset = 2;
    }
    else if(eo_sao_class == SAO_EDGE_45_DEG)
    {
        row_offset = 1;
        col_offset = -2;
    }

    for(row = row_start; row < row_end; row++)
    {
        for(col = col_start; col < col_end; col++)
        {
            c = ps_sao_ctxt
                    ->pu1_cur_chroma_recon_buf[col + row * ps_sao_ctxt->i4_cur_chroma_recon_stride];
            a = ps_sao_ctxt->pu1_cur_chroma_recon_buf
                    [(col - col_offset) +
                     (row - row_offset) * ps_sao_ctxt->i4_cur_chroma_recon_stride];
            b = ps_sao_ctxt->pu1_cur_chroma_recon_buf
                    [(col + col_offset) +
                     (row + row_offset) * ps_sao_ctxt->i4_cur_chroma_recon_stride];
            pel_error =
                ps_sao_ctxt
                    ->pu1_cur_chroma_src_buf[col + row * ps_sao_ctxt->i4_cur_chroma_src_stride] -
                ps_sao_ctxt
                    ->pu1_cur_chroma_recon_buf[col + row * ps_sao_ctxt->i4_cur_chroma_recon_stride];
            edgeidx = 2 + SIGN(c - a) + SIGN(c - b);

            if(pel_error != 0)
            {
                pi4_acc_error_category[edgeidx] += pel_error;
                pi4_category_count[edgeidx]++;
            }
        }
    }
}

/**
*******************************************************************************
*
* @brief
*     Gets edge offset params
*
* @par Description:
*     Given the ctb and sao angle this function will calculate accumulated
*     error between source and recon and the coresponding count for 4 edge
*     indexes one each for peak,valley, half peak and half valley.
*
* @param[in]
*   ps_sao_ctxt:   Pointer to SAO context
*   eo_sao_class: specifies edge offset class
*   pi4_acc_error_category: pointer to an array to store accumulated error between source and recon
*   pi4_category_count    : pointer to an array to store number of peaks,valleys,half peaks and half valleys.
* @returns
*
* @remarks
*  None
*
*******************************************************************************/
void ihevce_get_luma_eo_sao_params(
    void *pv_sao_ctxt,
    WORD32 eo_sao_class,
    WORD32 *pi4_acc_error_category,
    WORD32 *pi4_category_count)
{
    WORD32 row_start, row_end, col_start, col_end, row, col;
    WORD32 row_offset = 0, col_offset = 0;
    WORD32 a, b, c, pel_error, edgeidx;
    sao_ctxt_t *ps_sao_ctxt = (sao_ctxt_t *)pv_sao_ctxt;

    row_start = 0;
    row_end = ps_sao_ctxt->i4_sao_blk_ht;
    col_start = 0;
    col_end = ps_sao_ctxt->i4_sao_blk_wd;

    if((ps_sao_ctxt->i4_ctb_x == 0) && (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_x + 1) == ps_sao_ctxt->ps_sps->i2_pic_wd_in_ctb) &&
       (eo_sao_class != SAO_EDGE_90_DEG))
    {
        col_end = ps_sao_ctxt->i4_sao_blk_wd - 1;
    }

    if((ps_sao_ctxt->i4_ctb_y == 0) && (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_start = 1;
    }

    if(((ps_sao_ctxt->i4_ctb_y + 1) == ps_sao_ctxt->ps_sps->i2_pic_ht_in_ctb) &&
       (eo_sao_class != SAO_EDGE_0_DEG))
    {
        row_end = ps_sao_ctxt->i4_sao_blk_ht - 1;
    }

    if(eo_sao_class == SAO_EDGE_0_DEG)
    {
        row_offset = 0;
        col_offset = 1;
    }
    else if(eo_sao_class == SAO_EDGE_90_DEG)
    {
        row_offset = 1;
        col_offset = 0;
    }
    else if(eo_sao_class == SAO_EDGE_135_DEG)
    {
        row_offset = 1;
        col_offset = 1;
    }
    else if(eo_sao_class == SAO_EDGE_45_DEG)
    {
        row_offset = 1;
        col_offset = -1;
    }

    for(row = row_start; row < row_end; row++)
    {
        for(col = col_start; col < col_end; col++)
        {
            c = ps_sao_ctxt
                    ->pu1_cur_luma_recon_buf[col + row * ps_sao_ctxt->i4_cur_luma_recon_stride];
            a = ps_sao_ctxt->pu1_cur_luma_recon_buf
                    [(col - col_offset) +
                     (row - row_offset) * ps_sao_ctxt->i4_cur_luma_recon_stride];
            b = ps_sao_ctxt->pu1_cur_luma_recon_buf
                    [(col + col_offset) +
                     (row + row_offset) * ps_sao_ctxt->i4_cur_luma_recon_stride];
            pel_error =
                ps_sao_ctxt->pu1_cur_luma_src_buf[col + row * ps_sao_ctxt->i4_cur_luma_src_stride] -
                ps_sao_ctxt
                    ->pu1_cur_luma_recon_buf[col + row * ps_sao_ctxt->i4_cur_luma_recon_stride];
            edgeidx = 2 + SIGN(c - a) + SIGN(c - b);

            if(pel_error != 0)
            {
                pi4_acc_error_category[edgeidx] += pel_error;
                pi4_category_count[edgeidx]++;
            }
        }
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_compute_area_of_valid_cus_in_ctb \endif
*
* \brief
*
*
*****************************************************************************
*/
WORD32 ihevce_compute_area_of_valid_cus_in_ctb(cur_ctb_cu_tree_t *ps_cu_tree)
{
    WORD32 i4_area;

    if(NULL == ps_cu_tree)
    {
        return 0;
    }

    if(ps_cu_tree->is_node_valid)
    {
        i4_area = ps_cu_tree->u1_cu_size * ps_cu_tree->u1_cu_size;
    }
    else
    {
        i4_area = ihevce_compute_area_of_valid_cus_in_ctb(ps_cu_tree->ps_child_node_tl) +
                  ihevce_compute_area_of_valid_cus_in_ctb(ps_cu_tree->ps_child_node_tr) +
                  ihevce_compute_area_of_valid_cus_in_ctb(ps_cu_tree->ps_child_node_bl) +
                  ihevce_compute_area_of_valid_cus_in_ctb(ps_cu_tree->ps_child_node_br);
    }

    return i4_area;
}

/*!
******************************************************************************
* \if Function name : ihevce_create_cuNode_children \endif
*
* \brief
*
*
*****************************************************************************
*/
static WORD32 ihevce_create_cuNode_children(
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    cur_ctb_cu_tree_t *ps_cu_tree_cur_node,
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
    /*
    ps_tl = (ai4_child_node_enable[0]) ? ps_tl : NULL;
    ps_tr = (ai4_child_node_enable[1]) ? ps_tr : NULL;
    ps_bl = (ai4_child_node_enable[2]) ? ps_bl : NULL;
    ps_br = (ai4_child_node_enable[3]) ? ps_br : NULL;
    */
    ps_cu_tree_cur_node->ps_child_node_tl = ps_tl;
    ps_cu_tree_cur_node->ps_child_node_tr = ps_tr;
    ps_cu_tree_cur_node->ps_child_node_bl = ps_bl;
    ps_cu_tree_cur_node->ps_child_node_br = ps_br;

    return 4;
}

/*!
******************************************************************************
* \if Function name : ihevce_cu_tree_init \endif
*
* \brief
*
*
*****************************************************************************
*/
void ihevce_cu_tree_init(
    cur_ctb_cu_tree_t *ps_cu_tree,
    cur_ctb_cu_tree_t *ps_cu_tree_root,
    WORD32 *pi4_nodes_created_in_cu_tree,
    WORD32 tree_depth,
    CU_POS_T e_grandparent_blk_pos,
    CU_POS_T e_parent_blk_pos,
    CU_POS_T e_cur_blk_pos)
{
    WORD32 cu_pos_x = 0;
    WORD32 cu_pos_y = 0;
    WORD32 cu_size = 0;

    WORD32 children_nodes_required = 1;
    WORD32 node_validity = 0;

    switch(tree_depth)
    {
    case 0:
    {
        /* 64x64 block */
        cu_size = 64;
        cu_pos_x = 0;
        cu_pos_y = 0;

        break;
    }
    case 1:
    {
        /* 32x32 block */
        cu_size = 32;

        /* Explanation for logic below - */
        /* * pos_x and pos_y are in units of 8x8 CU's */
        /* * pos_x = 0 for TL and BL children */
        /* * pos_x = 4 for TR and BR children */
        /* * pos_y = 0 for TL and TR children */
        /* * pos_y = 4 for BL and BR children */
        cu_pos_x = (e_cur_blk_pos & 1) << 2;
        cu_pos_y = (e_cur_blk_pos & 2) << 1;

        break;
    }
    case 2:
    {
        /* 16x16 block */
        WORD32 cu_pos_x_parent;
        WORD32 cu_pos_y_parent;

        cu_size = 16;

        /* Explanation for logic below - */
        /* See similar explanation above */
        cu_pos_x_parent = (e_parent_blk_pos & 1) << 2;
        cu_pos_y_parent = (e_parent_blk_pos & 2) << 1;
        cu_pos_x = cu_pos_x_parent + ((e_cur_blk_pos & 1) << 1);
        cu_pos_y = cu_pos_y_parent + (e_cur_blk_pos & 2);

        break;
    }
    case 3:
    {
        /* 8x8 block */
        WORD32 cu_pos_x_grandparent;
        WORD32 cu_pos_y_grandparent;

        WORD32 cu_pos_x_parent;
        WORD32 cu_pos_y_parent;

        cu_size = 8;

        cu_pos_x_grandparent = (e_grandparent_blk_pos & 1) << 2;
        cu_pos_y_grandparent = (e_grandparent_blk_pos & 2) << 1;
        cu_pos_x_parent = cu_pos_x_grandparent + ((e_parent_blk_pos & 1) << 1);
        cu_pos_y_parent = cu_pos_y_grandparent + (e_parent_blk_pos & 2);
        cu_pos_x = cu_pos_x_parent + (e_cur_blk_pos & 1);
        cu_pos_y = cu_pos_y_parent + ((e_cur_blk_pos & 2) >> 1);

        children_nodes_required = 0;

        break;
    }
    }

    /* Fill the current cu_tree node */
    CU_TREE_NODE_FILL(ps_cu_tree, node_validity, cu_pos_x, cu_pos_y, cu_size, 1);

    if(children_nodes_required)
    {
        tree_depth++;

        (*pi4_nodes_created_in_cu_tree) += ihevce_create_cuNode_children(
            ps_cu_tree_root, ps_cu_tree, (*pi4_nodes_created_in_cu_tree));

        ihevce_cu_tree_init(
            ps_cu_tree->ps_child_node_tl,
            ps_cu_tree_root,
            pi4_nodes_created_in_cu_tree,
            tree_depth,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TL);

        ihevce_cu_tree_init(
            ps_cu_tree->ps_child_node_tr,
            ps_cu_tree_root,
            pi4_nodes_created_in_cu_tree,
            tree_depth,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_TR);

        ihevce_cu_tree_init(
            ps_cu_tree->ps_child_node_bl,
            ps_cu_tree_root,
            pi4_nodes_created_in_cu_tree,
            tree_depth,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BL);

        ihevce_cu_tree_init(
            ps_cu_tree->ps_child_node_br,
            ps_cu_tree_root,
            pi4_nodes_created_in_cu_tree,
            tree_depth,
            e_parent_blk_pos,
            e_cur_blk_pos,
            POS_BR);
    }
    else
    {
        NULLIFY_THE_CHILDREN_NODES(ps_cu_tree);
    }
}
