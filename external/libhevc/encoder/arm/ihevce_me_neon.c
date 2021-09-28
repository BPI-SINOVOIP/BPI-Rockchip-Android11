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
* @file
*  ihevce_me_neon.c
*
* @brief
*  Subpel refinement modules for ME algo
*
* @author
*  Ittiam
*
* @par List of Functions:
*
* @remarks
*  None
*
********************************************************************************
*/

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <arm_neon.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "itt_video_api.h"
#include "ihevc_cmn_utils_neon.h"
#include "ihevc_chroma_itrans_recon.h"
#include "ihevc_chroma_intra_pred.h"
#include "ihevc_debug.h"
#include "ihevc_deblk.h"
#include "ihevc_defs.h"
#include "ihevc_itrans_recon.h"
#include "ihevc_intra_pred.h"
#include "ihevc_inter_pred.h"
#include "ihevc_macros.h"
#include "ihevc_mem_fns.h"
#include "ihevc_padding.h"
#include "ihevc_quant_iquant_ssd.h"
#include "ihevc_resi_trans.h"
#include "ihevc_sao.h"
#include "ihevc_structs.h"
#include "ihevc_weighted_pred.h"

#include "rc_cntrl_param.h"
#include "rc_frame_info_collector.h"
#include "rc_look_ahead_params.h"

#include "ihevce_api.h"
#include "ihevce_defs.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_function_selector.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_enc_structs.h"
#include "ihevce_had_satd.h"
#include "ihevce_ipe_instr_set_router.h"
#include "ihevce_global_tables.h"

#include "hme_datatype.h"
#include "hme_common_defs.h"
#include "hme_common_utils.h"
#include "hme_interface.h"
#include "hme_defs.h"
#include "hme_err_compute.h"
#include "hme_globals.h"

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/
typedef void ft_calc_sad4_nxn(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, UWORD32 *pu4_sad);

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define COMBINE_SADS(pps, as, i)                                                                   \
    {                                                                                              \
        pps[PART_ID_NxN_TL][i] = (as[0] + as[1] + as[4] + as[5]);                                  \
        pps[PART_ID_NxN_TR][i] = (as[2] + as[3] + as[6] + as[7]);                                  \
        pps[PART_ID_NxN_BL][i] = (as[8] + as[9] + as[12] + as[13]);                                \
        pps[PART_ID_NxN_BR][i] = (as[10] + as[11] + as[14] + as[15]);                              \
                                                                                                   \
        pps[PART_ID_Nx2N_L][i] = pps[PART_ID_NxN_TL][i] + pps[PART_ID_NxN_BL][i];                  \
        pps[PART_ID_Nx2N_R][i] = pps[PART_ID_NxN_TR][i] + pps[PART_ID_NxN_BR][i];                  \
        pps[PART_ID_2NxN_T][i] = pps[PART_ID_NxN_TR][i] + pps[PART_ID_NxN_TL][i];                  \
        pps[PART_ID_2NxN_B][i] = pps[PART_ID_NxN_BR][i] + pps[PART_ID_NxN_BL][i];                  \
                                                                                                   \
        pps[PART_ID_nLx2N_L][i] = (as[8] + as[0] + as[12] + as[4]);                                \
        pps[PART_ID_nRx2N_R][i] = (as[3] + as[7] + as[15] + as[11]);                               \
        pps[PART_ID_2NxnU_T][i] = (as[1] + as[0] + as[2] + as[3]);                                 \
        pps[PART_ID_2NxnD_B][i] = (as[15] + as[14] + as[12] + as[13]);                             \
                                                                                                   \
        pps[PART_ID_2Nx2N][i] = pps[PART_ID_2NxN_T][i] + pps[PART_ID_2NxN_B][i];                   \
                                                                                                   \
        pps[PART_ID_2NxnU_B][i] = pps[PART_ID_2Nx2N][i] - pps[PART_ID_2NxnU_T][i];                 \
        pps[PART_ID_2NxnD_T][i] = pps[PART_ID_2Nx2N][i] - pps[PART_ID_2NxnD_B][i];                 \
        pps[PART_ID_nRx2N_L][i] = pps[PART_ID_2Nx2N][i] - pps[PART_ID_nRx2N_R][i];                 \
        pps[PART_ID_nLx2N_R][i] = pps[PART_ID_2Nx2N][i] - pps[PART_ID_nLx2N_L][i];                 \
    }

#define COMBINE_SADS_2(ps, as)                                                                     \
    {                                                                                              \
        ps[PART_ID_NxN_TL] = (as[0] + as[1] + as[4] + as[5]);                                      \
        ps[PART_ID_NxN_TR] = (as[2] + as[3] + as[6] + as[7]);                                      \
        ps[PART_ID_NxN_BL] = (as[8] + as[9] + as[12] + as[13]);                                    \
        ps[PART_ID_NxN_BR] = (as[10] + as[11] + as[14] + as[15]);                                  \
                                                                                                   \
        ps[PART_ID_Nx2N_L] = ps[PART_ID_NxN_TL] + ps[PART_ID_NxN_BL];                              \
        ps[PART_ID_Nx2N_R] = ps[PART_ID_NxN_TR] + ps[PART_ID_NxN_BR];                              \
        ps[PART_ID_2NxN_T] = ps[PART_ID_NxN_TR] + ps[PART_ID_NxN_TL];                              \
        ps[PART_ID_2NxN_B] = ps[PART_ID_NxN_BR] + ps[PART_ID_NxN_BL];                              \
                                                                                                   \
        ps[PART_ID_nLx2N_L] = (as[8] + as[0] + as[12] + as[4]);                                    \
        ps[PART_ID_nRx2N_R] = (as[3] + as[7] + as[15] + as[11]);                                   \
        ps[PART_ID_2NxnU_T] = (as[1] + as[0] + as[2] + as[3]);                                     \
        ps[PART_ID_2NxnD_B] = (as[15] + as[14] + as[12] + as[13]);                                 \
                                                                                                   \
        ps[PART_ID_2Nx2N] = ps[PART_ID_2NxN_T] + ps[PART_ID_2NxN_B];                               \
                                                                                                   \
        ps[PART_ID_2NxnU_B] = ps[PART_ID_2Nx2N] - ps[PART_ID_2NxnU_T];                             \
        ps[PART_ID_2NxnD_T] = ps[PART_ID_2Nx2N] - ps[PART_ID_2NxnD_B];                             \
        ps[PART_ID_nRx2N_L] = ps[PART_ID_2Nx2N] - ps[PART_ID_nRx2N_R];                             \
        ps[PART_ID_nLx2N_R] = ps[PART_ID_2Nx2N] - ps[PART_ID_nLx2N_L];                             \
    }

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static void ihevce_sad4_2x2_neon(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, UWORD32 *pu4_sad)
{
    uint16x8_t abs = vdupq_n_u16(0);
    uint32x4_t sad;
    WORD32 i;

    /* -------- Compute four 2x2 SAD Transforms of 8x2 in one call--------- */
    for(i = 0; i < 2; i++)
    {
        const uint8x8_t src = vld1_u8(pu1_src);
        const uint8x8_t pred = vld1_u8(pu1_pred);

        abs = vabal_u8(abs, src, pred);
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    sad = vpaddlq_u16(abs);
    vst1q_u32(pu4_sad, sad);
}

static void ihevce_sad4_4x4_neon(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, UWORD16 *pu2_sad)
{
    uint16x8_t abs_01 = vdupq_n_u16(0);
    uint16x8_t abs_23 = vdupq_n_u16(0);
    uint16x4_t tmp_a0, tmp_a1;
    WORD32 i;

    /* -------- Compute four 4x4 SAD Transforms of 16x4 in one call--------- */
    for(i = 0; i < 4; i++)
    {
        const uint8x16_t src = vld1q_u8(pu1_src);
        const uint8x16_t pred = vld1q_u8(pu1_pred);

        abs_01 = vabal_u8(abs_01, vget_low_u8(src), vget_low_u8(pred));
        abs_23 = vabal_u8(abs_23, vget_high_u8(src), vget_high_u8(pred));
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a0 = vpadd_u16(vget_low_u16(abs_01), vget_high_u16(abs_01));
    tmp_a1 = vpadd_u16(vget_low_u16(abs_23), vget_high_u16(abs_23));
    abs_01 = vcombine_u16(tmp_a0, tmp_a1);
    tmp_a0 = vpadd_u16(vget_low_u16(abs_01), vget_high_u16(abs_01));
    vst1_u16(pu2_sad, tmp_a0);
}

static void ihevce_sad4_8x8_neon(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, UWORD32 *pu4_sad)
{
    uint16x8_t abs_0 = vdupq_n_u16(0);
    uint16x8_t abs_1 = vdupq_n_u16(0);
    uint16x8_t abs_2 = vdupq_n_u16(0);
    uint16x8_t abs_3 = vdupq_n_u16(0);
    uint16x4_t tmp_a0, tmp_a1;
    uint32x4_t sad;
    WORD32 i;

    /* -------- Compute four 8x8 SAD Transforms of 32x8 in one call--------- */
    for(i = 0; i < 8; i++)
    {
        uint8x16_t src_01 = vld1q_u8(pu1_src);
        uint8x16_t pred_01 = vld1q_u8(pu1_pred);
        uint8x16_t src_23 = vld1q_u8(pu1_src + 16);
        uint8x16_t pred_23 = vld1q_u8(pu1_pred + 16);

        abs_0 = vabal_u8(abs_0, vget_low_u8(src_01), vget_low_u8(pred_01));
        abs_1 = vabal_u8(abs_1, vget_high_u8(src_01), vget_high_u8(pred_01));
        abs_2 = vabal_u8(abs_2, vget_low_u8(src_23), vget_low_u8(pred_23));
        abs_3 = vabal_u8(abs_3, vget_high_u8(src_23), vget_high_u8(pred_23));
        pu1_src += src_strd;
        pu1_pred += pred_strd;
    }
    tmp_a0 = vpadd_u16(vget_low_u16(abs_0), vget_high_u16(abs_0));
    tmp_a1 = vpadd_u16(vget_low_u16(abs_1), vget_high_u16(abs_1));
    abs_0 = vcombine_u16(tmp_a0, tmp_a1);
    tmp_a0 = vpadd_u16(vget_low_u16(abs_2), vget_high_u16(abs_2));
    tmp_a1 = vpadd_u16(vget_low_u16(abs_3), vget_high_u16(abs_3));
    abs_1 = vcombine_u16(tmp_a0, tmp_a1);
    tmp_a0 = vpadd_u16(vget_low_u16(abs_0), vget_high_u16(abs_0));
    tmp_a1 = vpadd_u16(vget_low_u16(abs_1), vget_high_u16(abs_1));
    abs_0 = vcombine_u16(tmp_a0, tmp_a1);
    sad = vpaddlq_u16(abs_0);
    vst1q_u32(pu4_sad, sad);
}

static void ihevce_sad4_16x16_neon(
    UWORD8 *pu1_src, WORD32 src_strd, UWORD8 *pu1_pred, WORD32 pred_strd, UWORD32 *pu4_sad)
{
    WORD32 i;

    /* ------ Compute four 16x16 SAD Transforms of 64x16 in one call-------- */
    for(i = 0; i < 4; i++)
    {
        pu4_sad[i] = ihevce_nxn_sad_computer_neon(
            pu1_src + (i * 16), src_strd, pu1_pred + (i * 16), pred_strd, 16);
    }
}

void compute_part_sads_for_MxM_blk_neon(
    grid_ctxt_t *ps_grid,
    UWORD8 *pu1_cur_ptr,
    WORD32 cur_buf_stride,
    WORD32 **pp_part_sads,
    cand_t *ps_cand,
    WORD32 *num_cands,
    CU_SIZE_T e_cu_size)
{
    WORD16 grd_sz_y = (ps_grid->grd_sz_y_x & 0xFFFF0000) >> 16;
    WORD16 grd_sz_x = (ps_grid->grd_sz_y_x & 0xFFFF);

    /* Assumes the following order: C, L, T, R, B, TL, TR, BL, BR */
    WORD32 offset_x[NUM_CANDIDATES_IN_GRID] = { 0,         -grd_sz_x, 0,         grd_sz_x, 0,
                                                -grd_sz_x, grd_sz_x,  -grd_sz_x, grd_sz_x };
    WORD32 offset_y[NUM_CANDIDATES_IN_GRID] = { 0,         0,         -grd_sz_y, 0,       grd_sz_y,
                                                -grd_sz_y, -grd_sz_y, grd_sz_y,  grd_sz_y };
    WORD32 shift = (WORD32)e_cu_size;

    WORD32 ref_buf_stride = ps_grid->ref_buf_stride;
    WORD32 cur_buf_stride_lsN = (cur_buf_stride << (1 + shift));
    WORD32 ref_buf_stride_lsN = (ref_buf_stride << (1 + shift));

    cand_t *cand0 = ps_cand;

    ft_calc_sad4_nxn *calc_sad4 = NULL;

    /* for a 2Nx2N partition we evaluate (N/2)x(N/2) SADs. This is needed for
     * AMP cases */
    UWORD32 au4_nxn_sad[16];

    WORD32 i, j;

    *num_cands = 0;

    /* Loop to fill up the cand_t array and to calculate num_cands */
    for(i = 0; i < ps_grid->num_grids; i++)
    {
        WORD32 j;
        WORD32 mask = ps_grid->pi4_grd_mask[i];
        UWORD8 *pu1_ref_ptr_center = ps_grid->ppu1_ref_ptr[i];
        WORD32 mv_x = ps_grid->p_mv[i].i2_mv_x;
        WORD32 mv_y = (ps_grid->p_mv[i].i2_mv_y);

        for(j = 0; j < NUM_CANDIDATES_IN_GRID; j++, mask >>= 1)
        {
            if(mask & 1)
            {
                *num_cands = *num_cands + 1;
                cand0->grid_ix = i;
                cand0->ref_idx = ps_grid->p_ref_idx[i];
                cand0->pu1_ref_ptr =
                    pu1_ref_ptr_center + offset_x[j] + ref_buf_stride * offset_y[j];
                cand0->mv.i2_mv_x = (S16)(mv_x) + offset_x[j];
                cand0->mv.i2_mv_y = (S16)(mv_y) + offset_y[j];
                cand0++;
            }
        }
    }

    /* fn selector */
    if(e_cu_size == CU_8x8)
        calc_sad4 = ihevce_sad4_2x2_neon;
    else if(e_cu_size == CU_32x32)
        calc_sad4 = ihevce_sad4_8x8_neon;
    else if(e_cu_size == CU_64x64)
        calc_sad4 = ihevce_sad4_16x16_neon;

    /* Loop to compute the SAD's */
    for(i = 0; i < *num_cands; i++)
    {
        cand_t *cand = ps_cand + i;

        for(j = 0; j < 4; j++)
            (*calc_sad4)(
                pu1_cur_ptr + j * cur_buf_stride_lsN,
                cur_buf_stride,
                cand->pu1_ref_ptr + j * ref_buf_stride_lsN,
                ref_buf_stride,
                &au4_nxn_sad[4 * j]);

        COMBINE_SADS(pp_part_sads, au4_nxn_sad, i);
    }
}

void compute_4x4_sads_for_16x16_blk_neon(
    grid_ctxt_t *ps_grid,
    UWORD8 *pu1_cur_ptr,
    WORD32 cur_buf_stride,
    UWORD16 **pp_part_sads,
    cand_t *ps_cand,
    WORD32 *num_cands)
{
    WORD16 grd_sz_y = (ps_grid->grd_sz_y_x & 0xFFFF0000) >> 16;
    WORD16 grd_sz_x = (ps_grid->grd_sz_y_x & 0xFFFF);

    /* Assumes the following order: C, L, T, R, B, TL, TR, BL, BR */
    WORD32 offset_x[NUM_CANDIDATES_IN_GRID] = { 0,         -grd_sz_x, 0,         grd_sz_x, 0,
                                                -grd_sz_x, grd_sz_x,  -grd_sz_x, grd_sz_x };
    WORD32 offset_y[NUM_CANDIDATES_IN_GRID] = { 0,         0,         -grd_sz_y, 0,       grd_sz_y,
                                                -grd_sz_y, -grd_sz_y, grd_sz_y,  grd_sz_y };

    WORD32 ref_buf_stride = ps_grid->ref_buf_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    cand_t *cand0 = ps_cand;

    /* for a 2Nx2N partition we evaluate (N/2)x(N/2) SADs. This is needed for
     * AMP cases */
    UWORD16 au2_4x4_sad[16];

    WORD32 i, j;

    *num_cands = 0;

    /* Loop to fill up the cand_t array and to calculate num_cands */
    for(i = 0; i < ps_grid->num_grids; i++)
    {
        WORD32 j;
        WORD32 mask = ps_grid->pi4_grd_mask[i];
        UWORD8 *pu1_ref_ptr_center = ps_grid->ppu1_ref_ptr[i];
        WORD32 mv_x = ps_grid->p_mv[i].i2_mv_x;
        WORD32 mv_y = (ps_grid->p_mv[i].i2_mv_y);

        for(j = 0; j < NUM_CANDIDATES_IN_GRID; j++, mask >>= 1)
        {
            if(mask & 1)
            {
                *num_cands = *num_cands + 1;
                cand0->grid_ix = i;
                cand0->ref_idx = ps_grid->p_ref_idx[i];
                cand0->pu1_ref_ptr =
                    pu1_ref_ptr_center + offset_x[j] + ref_buf_stride * offset_y[j];
                cand0->mv.i2_mv_x = (S16)(mv_x) + offset_x[j];
                cand0->mv.i2_mv_y = (S16)(mv_y) + offset_y[j];
                cand0++;
            }
        }
    }

    /* Loop to compute the SAD's */
    for(i = 0; i < *num_cands; i++)
    {
        cand_t *cand = ps_cand + i;

        for(j = 0; j < 4; j++)
            ihevce_sad4_4x4_neon(
                pu1_cur_ptr + j * cur_buf_stride_ls2,
                cur_buf_stride,
                cand->pu1_ref_ptr + j * ref_buf_stride_ls2,
                ref_buf_stride,
                &au2_4x4_sad[4 * j]);

        COMBINE_SADS(pp_part_sads, au2_4x4_sad, i);
    }
}

void hme_evalsad_grid_npu_MxN_neon(err_prms_t *ps_prms)
{
    S32 *pi4_sad = ps_prms->pi4_sad_grid;
    S32 i, grid_count = 0;
    S32 x_off = ps_prms->i4_step;
    S32 y_off = ps_prms->i4_step * ps_prms->i4_ref_stride;

    assert((ps_prms->i4_part_mask & (ps_prms->i4_part_mask - 1)) == 0);

    for(i = 0; i < 9; i++)
    {
        if(ps_prms->i4_grid_mask & (1 << i))
            grid_count++;
    }
    pi4_sad += (ps_prms->pi4_valid_part_ids[0] * grid_count);

    for(i = 0; i < 9; i++)
    {
        U08 *pu1_inp = ps_prms->pu1_inp;
        U08 *pu1_ref = ps_prms->pu1_ref;

        if(!(ps_prms->i4_grid_mask & (1 << i)))
            continue;

        pu1_ref += x_off * gai1_grid_id_to_x[i];
        pu1_ref += y_off * gai1_grid_id_to_y[i];
        *pi4_sad++ = ihevce_4mx4n_sad_computer_neon(
            pu1_inp,
            pu1_ref,
            ps_prms->i4_inp_stride,
            ps_prms->i4_ref_stride,
            ps_prms->i4_blk_wd,
            ps_prms->i4_blk_ht);
    }
}

void hme_evalsad_pt_npu_MxN_8bit_neon(err_prms_t *ps_prms)
{
    ps_prms->pi4_sad_grid[0] = ihevce_4mx4n_sad_computer_neon(
        ps_prms->pu1_inp,
        ps_prms->pu1_ref,
        ps_prms->i4_inp_stride,
        ps_prms->i4_ref_stride,
        ps_prms->i4_blk_wd,
        ps_prms->i4_blk_ht);
}

void hme_calc_sad_and_1_best_result_neon(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    mv_refine_ctxt_t *refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    search_node_t *ps_search_node = ps_search_prms->ps_search_nodes;
    S32 i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    S32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    S32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    S32 ref_buf_stride_ls2 = (ref_buf_stride << 2);
    S32 i4_inp_off, i4_ref_off;
    S32 i;

    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += (ps_search_prms->i4_cu_y_off * cur_buf_stride);
    i4_ref_off = ps_search_prms->i4_x_off;
    i4_ref_off += (ps_search_prms->i4_y_off * i4_ref_stride);

    /* Run through each of the candts in a loop */
    for(i = 0; i < i4_num_nodes; i++)
    {
        U16 au2_4x4_sad[16];
        S32 i4_mv_cost;
        S32 j;

        if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
        {
            continue;
        }

        ps_err_prms->pu1_inp = ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
        ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_off;
        ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
        ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

        /* Loop to compute the SAD's */
        for(j = 0; j < 4; j++)
        {
            UWORD8 *pu1_curr = ps_err_prms->pu1_inp;
            UWORD8 *pu1_ref = ps_err_prms->pu1_ref;

            ihevce_sad4_4x4_neon(
                pu1_curr + j * cur_buf_stride_ls2,
                cur_buf_stride,
                pu1_ref + j * ref_buf_stride_ls2,
                ref_buf_stride,
                &au2_4x4_sad[4 * j]);
        }

        COMBINE_SADS_2(pi4_sad_grid, au2_4x4_sad);

        // calculate MV cost
        {
            S16 mvdx1, mvdy1;
            S32 i4_ref_idx = ps_result_prms->i1_ref_idx;
            search_results_t *ps_srch_rslts = ps_result_prms->ps_search_results;

            pred_ctxt_t *ps_pred_ctxt = &ps_srch_rslts->as_pred_ctxt[i4_ref_idx];
            pred_candt_nodes_t *ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[PART_2Nx2N];
            search_node_t *ps_mvp_node = ps_pred_nodes->ps_mvp_node;

            S32 inp_shift = 2;
            S32 pred_shift = ps_mvp_node->u1_subpel_done ? 0 : 2;
            S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
            S32 lambda = ps_pred_ctxt->lambda;
            S32 rnd = 1 << (lambda_q_shift - 1);
            S32 mv_p_x = ps_mvp_node->s_mv.i2_mvx;
            S32 mv_p_y = ps_mvp_node->s_mv.i2_mvy;
            S32 ref_bits =
                ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_search_node->i1_ref_idx];

            COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_search_node, mv_p_x, mv_p_y, inp_shift, pred_shift);

            mvdx1 = ABS(mvdx1);
            mvdy1 = ABS(mvdy1);

            i4_mv_cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) + (mvdy1 > 0) +
                         ref_bits + 2;

            i4_mv_cost *= lambda;
            i4_mv_cost += rnd;
            i4_mv_cost >>= lambda_q_shift;

            i4_mv_cost = CLIP_U16(i4_mv_cost);
        }

        {
            S32 i4_sad, i4_tot_cost;
            S32 *pi4_valid_part_ids = &refine_ctxt->ai4_part_id[0];
            S32 best_node_cost;

            /* For each valid partition, update the refine_prm structure to
             * reflect the best and second best candidates for that partition */
            for(j = 0; j < refine_ctxt->i4_num_valid_parts; j++)
            {
                S32 part_id = pi4_valid_part_ids[j];
                S32 id = (refine_ctxt->i4_num_valid_parts > 8) ? part_id : j;

                i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
                i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);

                best_node_cost = CLIP_S16(refine_ctxt->i2_tot_cost[0][id]);

                if(i4_tot_cost < best_node_cost)
                {
                    refine_ctxt->i2_tot_cost[0][id] = i4_tot_cost;
                    refine_ctxt->i2_mv_cost[0][id] = i4_mv_cost;
                    refine_ctxt->i2_mv_x[0][id] = ps_search_node->s_mv.i2_mvx;
                    refine_ctxt->i2_mv_y[0][id] = ps_search_node->s_mv.i2_mvy;
                    refine_ctxt->i2_ref_idx[0][id] = ps_search_node->i1_ref_idx;
                }
            }
        }

        ps_search_node++;
    }

    ps_search_node = ps_search_prms->ps_search_nodes;

    for(i = 0; i < refine_ctxt->i4_num_valid_parts; i++)
    {
        S32 part_id = refine_ctxt->ai4_part_id[i];

        if(refine_ctxt->i2_tot_cost[0][part_id] >= MAX_SIGNED_16BIT_VAL)
        {
            assert(refine_ctxt->i2_mv_cost[0][part_id] == MAX_SIGNED_16BIT_VAL);
            assert(refine_ctxt->i2_mv_x[0][part_id] == 0);
            assert(refine_ctxt->i2_mv_y[0][part_id] == 0);

            refine_ctxt->i2_ref_idx[0][part_id] = ps_search_node->i1_ref_idx;
        }
        if(refine_ctxt->i2_tot_cost[1][part_id] >= MAX_SIGNED_16BIT_VAL)
        {
            assert(refine_ctxt->i2_mv_cost[1][part_id] == MAX_SIGNED_16BIT_VAL);
            assert(refine_ctxt->i2_mv_x[1][part_id] == 0);
            assert(refine_ctxt->i2_mv_y[1][part_id] == 0);

            refine_ctxt->i2_ref_idx[1][part_id] = ps_search_node->i1_ref_idx;
        }
    }
}

void hme_calc_sad_and_1_best_result_subpel_neon(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms)
{
    mv_refine_ctxt_t *refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 *pi4_valid_part_ids = &refine_ctxt->ai4_part_id[0];
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    S32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    S32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    S32 ref_buf_stride_ls2 = (ref_buf_stride << 2);
    U16 au2_4x4_sad[16];
    S32 i;

    /* Loop to compute the SAD's */
    for(i = 0; i < 4; i++)
    {
        UWORD8 *pu1_curr = ps_err_prms->pu1_inp;
        UWORD8 *pu1_ref = ps_err_prms->pu1_ref;

        ihevce_sad4_4x4_neon(
            pu1_curr + i * cur_buf_stride_ls2,
            cur_buf_stride,
            pu1_ref + i * ref_buf_stride_ls2,
            ref_buf_stride,
            &au2_4x4_sad[4 * i]);
    }

    COMBINE_SADS_2(pi4_sad_grid, au2_4x4_sad);

    /* For each valid partition, update the refine_prm structure to
     * reflect the best and second best candidates for that partition */
    for(i = 0; i < refine_ctxt->i4_num_valid_parts; i++)
    {
        S32 part_id = pi4_valid_part_ids[i];
        S32 id = (refine_ctxt->i4_num_valid_parts > 8) ? part_id : i;
        S32 i4_mv_cost = refine_ctxt->i2_mv_cost[0][id];
        S32 i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
        S32 i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);
        S32 best_node_cost = CLIP_S16(refine_ctxt->i2_tot_cost[0][id]);

        if(i4_tot_cost < best_node_cost)
        {
            refine_ctxt->i2_tot_cost[0][id] = i4_tot_cost;
            refine_ctxt->i2_mv_cost[0][id] = i4_mv_cost;
            refine_ctxt->i2_mv_x[0][id] = ps_result_prms->i2_mv_x;
            refine_ctxt->i2_mv_y[0][id] = ps_result_prms->i2_mv_y;
            refine_ctxt->i2_ref_idx[0][id] = ps_result_prms->i1_ref_idx;
        }
    }

    for(i = 0; i < TOT_NUM_PARTS; i++)
    {
        if(refine_ctxt->i2_tot_cost[0][i] >= MAX_SIGNED_16BIT_VAL)
        {
            refine_ctxt->ai2_fullpel_satd[0][i] = MAX_SIGNED_16BIT_VAL;
        }
    }
}
