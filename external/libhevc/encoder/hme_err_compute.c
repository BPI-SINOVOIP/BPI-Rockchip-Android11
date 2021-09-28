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
***************************************************************************
* \file hme_err_compute.c
*
* \brief
*    SAD / SATD routines for error computation
*
* Detailed_description : Contains various types of SAD/SATD routines for
*   error computation between a given input and reference ptr. The SAD
*   routines can evaluate for either a single point or a grid, and can
*   evaluate with either partial updates or no partial updates. Partial
*   updates means evaluating sub block SADs, e.g. 4 4x4 subblock SAD in
*   addition to the main 8x8 block SAD.
*
* \date
*    22/9/2012
*
* \author  Ittiam
***************************************************************************
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
#include "hme_refine.h"
#include "hme_err_compute.h"
#include "hme_common_utils.h"
#include "hme_search_algo.h"
#include "ihevce_stasino_helpers.h"

/******************************************************************************
*                         MACRO DEFINITIONS
******************************************************************************/

/*****************************************************************************/
/* Theoritically, the various types of SAD functions that are needed for     */
/* reasons of optimality. SADs that are to be evaluated at a single pt can be*/
/* more optimal than SADs that are to be evaluated for a grid of 3x3. The    */
/* SADs to be evaluated at a grid are classified as separate functions, since*/
/* evaluating them on a single function call helps reuse inputs for a small  */
/* grid of 3x3. Also, if no partial updates are required, there are 3 basic  */
/* funcitons, width 4K (K = odd number), width 8K (K = odd number) and width */
/* 16K, K any number. For partial updates, it is assumed that the block size */
/* is square (8x8, 16x16, 32x32, 64x64) and further differentiation is done  */
/* based on the basic evaluation unit. E.g. if 16x16 blk size requires, part */
/* update on AMP partitions, then basic SAD unit is 4x4, if it doesnt, then  */
/* basic SAD unit is 8x8.                                                    */
/*****************************************************************************/

#define UPD_RES_PT_NPU_BEST1 hme_update_results_grid_pu_bestn
#define UPD_RES_PT_NPU_BESTN hme_update_results_grid_pu_bestn
#define UPD_RES_PT_PU_BEST1 hme_update_results_grid_pu_bestn
#define UPD_RES_PT_PU_BESTN hme_update_results_grid_pu_bestn
#define UPD_RES_GRID_NPU_BEST1 hme_update_results_grid_pu_bestn
#define UPD_RES_GRID_NPU_BESTN hme_update_results_grid_pu_bestn
#define UPD_RES_GRID_PU_BEST1 hme_update_results_grid_pu_bestn
#define UPD_RES_GRID_PU_BESTN hme_update_results_grid_pu_bestn

/*******************************************************************************
*                         FUNCTION DEFINITIONS
*******************************************************************************/
S32 hme_cmp_nodes(search_node_t *ps_best_node1, search_node_t *ps_best_node2)
{
    if((ps_best_node1->s_mv.i2_mvx == ps_best_node2->s_mv.i2_mvx) &&
       (ps_best_node1->s_mv.i2_mvy == ps_best_node2->s_mv.i2_mvy) &&
       (ps_best_node1->i1_ref_idx == ps_best_node2->i1_ref_idx))
    {
        return 0;
    }
    return -1;
}

void compute_4x4_sads_for_16x16_blk(
    grid_ctxt_t *ps_grid, /* Grid ctxt */
    UWORD8 *pu1_cur_ptr, /* Pointer to top-left of current block */
    WORD32 cur_buf_stride, /* Buffer stride of current buffer */
    UWORD16 **
        u2_part_sads, /* 2D Array containing SADs for all 17 partitions. As many rows as partitions. SADs in a row correspond to each of the candidates */
    cand_t *ps_cand, /* Return the list of candidates evaluated */
    WORD32 *num_cands /* Number of candidates that were processed */
)
{
    WORD32 a, b, c, d, i;
    WORD16 grd_sz_y = (ps_grid->grd_sz_y_x & 0xFFFF0000) >> 16;
    WORD16 grd_sz_x = (ps_grid->grd_sz_y_x & 0xFFFF);
    //WORD32 offset_x[9] = {-grd_sz_x, 0, grd_sz_x, -grd_sz_x, 0, grd_sz_x, grd_sz_x, 0, -grd_sz_x};
    //WORD32 offset_y[9] = {-grd_sz_y, -grd_sz_y, -grd_sz_y, 0, 0, 0, grd_sz_y, grd_sz_y, grd_sz_y};
    /* Assumes the following order: C, L, T, R, B, TL, TR, BL, BR */
    WORD32 offset_x[9] = { 0, -grd_sz_x, 0, grd_sz_x, 0, -grd_sz_x, grd_sz_x, -grd_sz_x, grd_sz_x };
    WORD32 offset_y[9] = { 0, 0, -grd_sz_y, 0, grd_sz_y, -grd_sz_y, -grd_sz_y, grd_sz_y, grd_sz_y };
    WORD32 ref_buf_stride = ps_grid->ref_buf_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);
    cand_t *cand0 = ps_cand;
    UWORD16 au2_4x4_sad[NUM_4X4];

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
    for(a = 0; a < *num_cands; a++)
    {
        cand_t *cand = ps_cand + a;
        memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
        for(b = 0; b < NUM_4X4; b++)
        {
            WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
            WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

            for(c = 0; c < NUM_ROWS_IN_4X4; c++)
            {
                WORD32 z_cur = (cur_buf_stride)*c + t1;
                WORD32 z_ref = (ref_buf_stride)*c + t2;
                for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                {
                    au2_4x4_sad[b] += (UWORD16)ABS(
                        (((S32)cand->pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                }
            }
        }

        u2_part_sads[PART_ID_NxN_TL][a] =
            (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
        u2_part_sads[PART_ID_NxN_TR][a] =
            (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
        u2_part_sads[PART_ID_NxN_BL][a] =
            (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
        u2_part_sads[PART_ID_NxN_BR][a] =
            (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
        u2_part_sads[PART_ID_Nx2N_L][a] =
            u2_part_sads[PART_ID_NxN_TL][a] + u2_part_sads[PART_ID_NxN_BL][a];
        u2_part_sads[PART_ID_Nx2N_R][a] =
            u2_part_sads[PART_ID_NxN_TR][a] + u2_part_sads[PART_ID_NxN_BR][a];
        u2_part_sads[PART_ID_2NxN_T][a] =
            u2_part_sads[PART_ID_NxN_TR][a] + u2_part_sads[PART_ID_NxN_TL][a];
        u2_part_sads[PART_ID_2NxN_B][a] =
            u2_part_sads[PART_ID_NxN_BR][a] + u2_part_sads[PART_ID_NxN_BL][a];
        u2_part_sads[PART_ID_nLx2N_L][a] =
            (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
        u2_part_sads[PART_ID_nRx2N_R][a] =
            (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
        u2_part_sads[PART_ID_2NxnU_T][a] =
            (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
        u2_part_sads[PART_ID_2NxnD_B][a] =
            (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
        u2_part_sads[PART_ID_2Nx2N][a] =
            u2_part_sads[PART_ID_2NxN_T][a] + u2_part_sads[PART_ID_2NxN_B][a];
        u2_part_sads[PART_ID_2NxnU_B][a] =
            u2_part_sads[PART_ID_2Nx2N][a] - u2_part_sads[PART_ID_2NxnU_T][a];
        u2_part_sads[PART_ID_2NxnD_T][a] =
            u2_part_sads[PART_ID_2Nx2N][a] - u2_part_sads[PART_ID_2NxnD_B][a];
        u2_part_sads[PART_ID_nRx2N_L][a] =
            u2_part_sads[PART_ID_2Nx2N][a] - u2_part_sads[PART_ID_nRx2N_R][a];
        u2_part_sads[PART_ID_nLx2N_R][a] =
            u2_part_sads[PART_ID_2Nx2N][a] - u2_part_sads[PART_ID_nLx2N_L][a];
    }
}

/**
********************************************************************************
*  @fn     compute_part_sads_for_MxM_blk(grid_ctxt_t *ps_grid,
*                                       UWORD8      *pu1_cur_ptr,
*                                       WORD32      cur_buf_stride,
*                                       WORD32     **pi4_part_sads,
*                                       cand_t      *ps_cand,
*                                       WORD32      *num_cands
*
*  @brief  Computes partial SADs and updates partition results for an MxM blk
*          and does so for several grids of points. This can be used for
*          32x32/64x64 blks with 17 partition updates
*
*
*  @param[in]  ps_grid : Pointer to grid ctxt that has multiple grid of max
*                        9 pts per grid
*
*  @param[in]  pu1_cur_ptr : Top left of input buffer
*
*  @param[in]  pi4_part_sads : array of pointers, each entry pointing to
*                             results to be updated for a given partition
*
*  @return   The ps_search_results structure has the best result updated for
*            the 2Nx2N partition alone.

********************************************************************************
*/
void compute_part_sads_for_MxM_blk(
    grid_ctxt_t *ps_grid,
    UWORD8 *pu1_cur_ptr,
    WORD32 cur_buf_stride,
    WORD32 **pp_part_sads,
    cand_t *ps_cand,
    WORD32 *num_cands,
    CU_SIZE_T e_cu_size)
{
    WORD32 a, b, c, d, i;
    WORD16 grd_sz_y = (ps_grid->grd_sz_y_x & 0xFFFF0000) >> 16;
    WORD16 grd_sz_x = (ps_grid->grd_sz_y_x & 0xFFFF);

    /* Assumes the following order: C, L, T, R, B, TL, TR, BL, BR */
    WORD32 offset_x[9] = { 0, -grd_sz_x, 0, grd_sz_x, 0, -grd_sz_x, grd_sz_x, -grd_sz_x, grd_sz_x };
    WORD32 offset_y[9] = { 0, 0, -grd_sz_y, 0, grd_sz_y, -grd_sz_y, -grd_sz_y, grd_sz_y, grd_sz_y };
    WORD32 shift = (WORD32)e_cu_size;

    WORD32 ref_buf_stride = ps_grid->ref_buf_stride;
    WORD32 cur_buf_stride_lsN = (cur_buf_stride << (1 + shift));
    WORD32 ref_buf_stride_lsN = (ref_buf_stride << (1 + shift));
    /* Num rows and pixels per row: 8 for CU_32x32 and 16 for CU_64x64 */
    WORD32 num_rows_in_nxn = 2 << shift;
    WORD32 num_pixels_in_row = 2 << shift;
    cand_t *cand0 = ps_cand;
    /* for a 2Nx2N partition we evaluate nxn SADs, where n = N/2. This is */
    /* needed for AMP cases.                                              */
    WORD32 a_nxn_sad[NUM_4X4];
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
    for(a = 0; a < *num_cands; a++)
    {
        cand_t *cand = ps_cand + a;
        memset(&a_nxn_sad[0], 0, NUM_4X4 * sizeof(WORD32));
        for(b = 0; b < NUM_4X4; b++)
        {
            WORD32 t1 = (b % 4) * num_pixels_in_row + (b >> 2) * cur_buf_stride_lsN;
            WORD32 t2 = (b % 4) * num_pixels_in_row + (b >> 2) * ref_buf_stride_lsN;

            for(c = 0; c < num_rows_in_nxn; c++)
            {
                WORD32 z_cur = (cur_buf_stride)*c + t1;
                WORD32 z_ref = (ref_buf_stride)*c + t2;
                for(d = 0; d < num_pixels_in_row; d++)
                {
                    a_nxn_sad[b] += (WORD32)ABS(
                        (((WORD32)cand->pu1_ref_ptr[(z_ref + d)]) -
                         ((WORD32)pu1_cur_ptr[(z_cur + d)])));
                }
            }
        }

        pp_part_sads[PART_ID_NxN_TL][a] =
            (a_nxn_sad[0] + a_nxn_sad[1] + a_nxn_sad[4] + a_nxn_sad[5]);
        pp_part_sads[PART_ID_NxN_TR][a] =
            (a_nxn_sad[2] + a_nxn_sad[3] + a_nxn_sad[6] + a_nxn_sad[7]);
        pp_part_sads[PART_ID_NxN_BL][a] =
            (a_nxn_sad[8] + a_nxn_sad[9] + a_nxn_sad[12] + a_nxn_sad[13]);
        pp_part_sads[PART_ID_NxN_BR][a] =
            (a_nxn_sad[10] + a_nxn_sad[11] + a_nxn_sad[14] + a_nxn_sad[15]);
        pp_part_sads[PART_ID_Nx2N_L][a] =
            pp_part_sads[PART_ID_NxN_TL][a] + pp_part_sads[PART_ID_NxN_BL][a];
        pp_part_sads[PART_ID_Nx2N_R][a] =
            pp_part_sads[PART_ID_NxN_TR][a] + pp_part_sads[PART_ID_NxN_BR][a];
        pp_part_sads[PART_ID_2NxN_T][a] =
            pp_part_sads[PART_ID_NxN_TR][a] + pp_part_sads[PART_ID_NxN_TL][a];
        pp_part_sads[PART_ID_2NxN_B][a] =
            pp_part_sads[PART_ID_NxN_BR][a] + pp_part_sads[PART_ID_NxN_BL][a];
        pp_part_sads[PART_ID_nLx2N_L][a] =
            (a_nxn_sad[8] + a_nxn_sad[0] + a_nxn_sad[12] + a_nxn_sad[4]);
        pp_part_sads[PART_ID_nRx2N_R][a] =
            (a_nxn_sad[3] + a_nxn_sad[7] + a_nxn_sad[15] + a_nxn_sad[11]);
        pp_part_sads[PART_ID_2NxnU_T][a] =
            (a_nxn_sad[1] + a_nxn_sad[0] + a_nxn_sad[2] + a_nxn_sad[3]);
        pp_part_sads[PART_ID_2NxnD_B][a] =
            (a_nxn_sad[15] + a_nxn_sad[14] + a_nxn_sad[12] + a_nxn_sad[13]);
        pp_part_sads[PART_ID_2Nx2N][a] =
            pp_part_sads[PART_ID_2NxN_T][a] + pp_part_sads[PART_ID_2NxN_B][a];
        pp_part_sads[PART_ID_2NxnU_B][a] =
            pp_part_sads[PART_ID_2Nx2N][a] - pp_part_sads[PART_ID_2NxnU_T][a];
        pp_part_sads[PART_ID_2NxnD_T][a] =
            pp_part_sads[PART_ID_2Nx2N][a] - pp_part_sads[PART_ID_2NxnD_B][a];
        pp_part_sads[PART_ID_nRx2N_L][a] =
            pp_part_sads[PART_ID_2Nx2N][a] - pp_part_sads[PART_ID_nRx2N_R][a];
        pp_part_sads[PART_ID_nLx2N_R][a] =
            pp_part_sads[PART_ID_2Nx2N][a] - pp_part_sads[PART_ID_nLx2N_L][a];
    }
}

void hme_evalsad_grid_pu_16x16(err_prms_t *ps_prms)
{
    grid_ctxt_t s_grid;
    cand_t as_candt[9];
    U16 au2_sad_grid[TOT_NUM_PARTS * 9];
    U16 *apu2_sad_grid[TOT_NUM_PARTS];
    hme_mv_t s_mv = { 0, 0 };
    S32 i4_ref_idx = 0, i;
    S32 num_candts = 0;
    s_grid.num_grids = 1;
    s_grid.ref_buf_stride = ps_prms->i4_ref_stride;
    s_grid.grd_sz_y_x = ((ps_prms->i4_step << 16) | ps_prms->i4_step);
    s_grid.ppu1_ref_ptr = &ps_prms->pu1_ref;
    s_grid.pi4_grd_mask = &ps_prms->i4_grid_mask;
    s_grid.p_mv = &s_mv;
    s_grid.p_ref_idx = &i4_ref_idx;
    for(i = 0; i < 9; i++)
    {
        if(s_grid.pi4_grd_mask[0] & (1 << i))
            num_candts++;
    }

    for(i = 0; i < TOT_NUM_PARTS; i++)
        apu2_sad_grid[i] = &au2_sad_grid[i * num_candts];

    compute_4x4_sads_for_16x16_blk(
        &s_grid, ps_prms->pu1_inp, ps_prms->i4_inp_stride, apu2_sad_grid, as_candt, &num_candts);
    for(i = 0; i < TOT_NUM_PARTS * num_candts; i++)
    {
        ps_prms->pi4_sad_grid[i] = au2_sad_grid[i];
    }
}

void hme_evalsad_grid_npu_MxN(err_prms_t *ps_prms)
{
    U08 *pu1_inp_base, *pu1_ref_c;
    S32 *pi4_sad = ps_prms->pi4_sad_grid;
    S32 i, grid_count = 0;
    S32 step = ps_prms->i4_step;
    S32 x_off = step, y_off = step * ps_prms->i4_ref_stride;

    ASSERT((ps_prms->i4_part_mask & (ps_prms->i4_part_mask - 1)) == 0);

    //assert(ps_prms->i4_blk_ht <= 8);
    //assert(ps_prms->i4_blk_wd <= 8);
    for(i = 0; i < 9; i++)
    {
        if(ps_prms->i4_grid_mask & (1 << i))
            grid_count++;
    }
    pi4_sad += (ps_prms->pi4_valid_part_ids[0] * grid_count);

    pu1_inp_base = ps_prms->pu1_inp;
    pu1_ref_c = ps_prms->pu1_ref;
    for(i = 0; i < 9; i++)
    {
        S32 sad = 0, j, k;
        U08 *pu1_inp, *pu1_ref;

        if(!(ps_prms->i4_grid_mask & (1 << i)))
            continue;
        pu1_ref = pu1_ref_c + x_off * gai1_grid_id_to_x[i];
        pu1_ref += y_off * gai1_grid_id_to_y[i];
        pu1_inp = pu1_inp_base;

        for(j = 0; j < ps_prms->i4_blk_ht; j++)
        {
            for(k = 0; k < ps_prms->i4_blk_wd; k++)
            {
                sad += (ABS((pu1_inp[k] - pu1_ref[k])));
            }
            pu1_inp += ps_prms->i4_inp_stride;
            pu1_ref += ps_prms->i4_ref_stride;
        }
        *pi4_sad++ = sad;
    }
}

WORD32 hme_evalsad_pt_npu_MxN_8bit_compute(
    WORD32 ht,
    WORD32 wd,
    UWORD8 *pu1_inp,
    UWORD8 *pu1_ref,
    WORD32 i4_inp_stride,
    WORD32 i4_ref_stride)
{
    WORD32 i, j;
    WORD32 sad = 0;
    for(i = 0; i < ht; i++)
    {
        for(j = 0; j < wd; j++)
        {
            sad += (ABS(((S32)pu1_inp[j] - (S32)pu1_ref[j])));
        }
        pu1_inp += i4_inp_stride;
        pu1_ref += i4_ref_stride;
    }
    return sad;
}

void hme_evalsad_pt_npu_MxN_8bit(err_prms_t *ps_prms)
{
    S32 wd, ht;
    U08 *pu1_inp, *pu1_ref;

    wd = ps_prms->i4_blk_wd;
    ht = ps_prms->i4_blk_ht;

    pu1_inp = ps_prms->pu1_inp;
    pu1_ref = ps_prms->pu1_ref;

    ps_prms->pi4_sad_grid[0] = hme_evalsad_pt_npu_MxN_8bit_compute(
        ht, wd, pu1_inp, pu1_ref, ps_prms->i4_inp_stride, ps_prms->i4_ref_stride);
}

void compute_satd_8bit(err_prms_t *ps_prms)
{
    U08 *pu1_origin;
    S32 src_strd;
    U08 *pu1_pred_buf;
    S32 dst_strd;
    S32 wd, ht;
    U32 u4_sad = 0;
    WORD32 x, y;
    U08 *u1_pi0, *u1_pi1;

    pu1_origin = ps_prms->pu1_inp;
    pu1_pred_buf = ps_prms->pu1_ref;
    src_strd = ps_prms->i4_inp_stride;
    dst_strd = ps_prms->i4_ref_stride;
    wd = ps_prms->i4_blk_wd;
    ht = ps_prms->i4_blk_ht;

    u1_pi0 = pu1_origin;
    u1_pi1 = pu1_pred_buf;

    /* Follows the following logic:
    For block sizes less than or equal to 16X16, the basic transform size is 4x4
    For block sizes greater than or equal to 32x32, the basic transform size is 8x8 */
    if((wd > 0x10) || (ht > 0x10))
    {
        for(y = 0; y < ht; y += 8)
        {
            for(x = 0; x < wd; x += 8)
            {
                u4_sad += ps_prms->ps_cmn_utils_optimised_function_list->pf_HAD_8x8_8bit(
                    &u1_pi0[x], src_strd, &u1_pi1[x], dst_strd, NULL, 1);
            }
            u1_pi0 += src_strd * 8;
            u1_pi1 += dst_strd * 8;
        }
    }
    else
    {
        for(y = 0; y < ht; y += 4)
        {
            for(x = 0; x < wd; x += 4)
            {
                u4_sad += ps_prms->ps_cmn_utils_optimised_function_list->pf_HAD_4x4_8bit(
                    &u1_pi0[x], src_strd, &u1_pi1[x], dst_strd, NULL, 1);
            }
            u1_pi0 += src_strd * 4;
            u1_pi1 += dst_strd * 4;
        }
    }

    ps_prms->pi4_sad_grid[0] = (S32)u4_sad;
}

void hme_init_pred_part(
    pred_ctxt_t *ps_pred_ctxt,
    search_node_t *ps_tl,
    search_node_t *ps_t,
    search_node_t *ps_tr,
    search_node_t *ps_l,
    search_node_t *ps_bl,
    search_node_t *ps_coloc,
    search_node_t *ps_zeromv,
    search_node_t **pps_proj_coloc,
    PART_ID_T e_part_id)
{
    pred_candt_nodes_t *ps_candt_nodes;

    ps_candt_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];

    ps_candt_nodes->ps_tl = ps_tl;
    ps_candt_nodes->ps_tr = ps_tr;
    ps_candt_nodes->ps_t = ps_t;
    ps_candt_nodes->ps_l = ps_l;
    ps_candt_nodes->ps_bl = ps_bl;
    ps_candt_nodes->ps_coloc = ps_coloc;
    ps_candt_nodes->ps_zeromv = ps_zeromv;
    ps_candt_nodes->pps_proj_coloc = pps_proj_coloc;
}

void hme_init_pred_ctxt_no_encode(
    pred_ctxt_t *ps_pred_ctxt,
    search_results_t *ps_search_results,
    search_node_t *ps_top_candts,
    search_node_t *ps_left_candts,
    search_node_t **pps_proj_coloc_candts,
    search_node_t *ps_coloc_candts,
    search_node_t *ps_zeromv_candt,
    S32 pred_lx,
    S32 lambda,
    S32 lambda_q_shift,
    U08 **ppu1_ref_bits_tlu,
    S16 *pi2_ref_scf)
{
    search_node_t *ps_invalid, *ps_l, *ps_t, *ps_tl, *ps_tr, *ps_bl;
    search_node_t *ps_coloc;
    PART_ID_T e_part_id;

    /* Assume that resolution is subpel to begin with */
    ps_pred_ctxt->mv_pel = 0;  // FPEL

    /* lambda and pred_lx (PRED_L0/PRED_L1) */
    ps_pred_ctxt->lambda = lambda;
    ps_pred_ctxt->lambda_q_shift = lambda_q_shift;
    ps_pred_ctxt->pred_lx = pred_lx;
    ps_pred_ctxt->ppu1_ref_bits_tlu = ppu1_ref_bits_tlu;
    ps_pred_ctxt->pi2_ref_scf = pi2_ref_scf;
    ps_pred_ctxt->proj_used = 0;

    /* Bottom left should not be valid */
    ASSERT(ps_left_candts[2].u1_is_avail == 0);
    ps_invalid = &ps_left_candts[2];

    /*************************************************************************/
    /* for the case of no encode, the idea is to set up cants as follows     */
    /*                                                                       */
    /*    ____ ______________                                                */
    /*   | TL | T  | T1 | TR |                                               */
    /*   |____|____|____|____|                                               */
    /*   | L  | b0 | b1 |                                                    */
    /*   |____|____|____|                                                    */
    /*   | L1 | b2 | b3 |                                                    */
    /*   |____|____|____|                                                    */
    /*   | BL |                                                              */
    /*   |____|                                                              */
    /*                                                                       */
    /*  If use_4x4 is 0, then b0,b1,b2,b3 are single 8x8 blk. then T=T1      */
    /* and L=L1. topleft, top and topright are TL,T,TR respectively          */
    /* Left and bottom left is L and BL respectively.                        */
    /* If use_4x4 is 1: then the above holds true only for PARTID = 0 (8x8)  */
    /*  For the 4 subblocks (partids 4-7)                                    */
    /*                                                                       */
    /*  Block   Left   Top   Top Left   Top Right   Bottom Left             */
    /*    b0    L      T      TL          T1          L1                     */
    /*    b1    b0     T1     T           TR          BL(invalid)            */
    /*    b2    L1     b0     L0          b1          BL (invalid)           */
    /*    b3    b2     b1     b0          BL(inv)     BL (inv)               */
    /*                                                                       */
    /* Note : For block b1, bottom left pts to b2, which is not yet ready    */
    /*  hence it is kept invalid and made to pt to BL. For block b3 top rt   */
    /* is invalid and hence made to pt to BL which is invalid.               */
    /* BL is invalid since it lies in a bottom left 8x8 blk and not yet ready*/
    /*************************************************************************/

    /* ps_coloc always points to a fixe candt (global) */
    /* TODO : replace incoming ps_coloc from global to geniune coloc */
    ps_coloc = ps_coloc_candts;

    /* INITIALIZATION OF 8x8 BLK */
    ps_tl = ps_top_candts;
    ps_t = ps_tl + 2;
    ps_tr = ps_t + 1;
    ps_l = ps_left_candts + 1;
    ps_bl = ps_invalid;
    e_part_id = PART_ID_2Nx2N;
    hme_init_pred_part(
        ps_pred_ctxt,
        ps_tl,
        ps_t,
        ps_tr,
        ps_l,
        ps_bl,
        ps_coloc,
        ps_zeromv_candt,
        pps_proj_coloc_candts,
        e_part_id);

    /* INITIALIZATION OF 4x4 TL BLK */
    e_part_id = PART_ID_NxN_TL;
    ps_tl = ps_top_candts;
    ps_t = ps_tl + 1;
    ps_tr = ps_t + 1;
    ps_l = ps_left_candts;
    ps_bl = ps_l + 1;
    hme_init_pred_part(
        ps_pred_ctxt,
        ps_tl,
        ps_t,
        ps_tr,
        ps_l,
        ps_bl,
        ps_coloc,
        ps_zeromv_candt,
        pps_proj_coloc_candts,
        e_part_id);

    /* INITIALIZATION OF 4x4 TR BLK */
    e_part_id = PART_ID_NxN_TR;
    ps_tl = ps_top_candts + 1;
    ps_t = ps_tl + 1;
    ps_tr = ps_t + 1;
    ps_l = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TL];
    ps_bl = ps_invalid;
    hme_init_pred_part(
        ps_pred_ctxt,
        ps_tl,
        ps_t,
        ps_tr,
        ps_l,
        ps_bl,
        ps_coloc,
        ps_zeromv_candt,
        pps_proj_coloc_candts,
        e_part_id);

    /* INITIALIZATION OF 4x4 BL BLK */
    e_part_id = PART_ID_NxN_BL;
    ps_tl = ps_left_candts;
    ps_t = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TL];
    ps_tr = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TR];
    ps_l = ps_left_candts + 1;
    ps_bl = ps_invalid;  //invalid
    hme_init_pred_part(
        ps_pred_ctxt,
        ps_tl,
        ps_t,
        ps_tr,
        ps_l,
        ps_bl,
        ps_coloc,
        ps_zeromv_candt,
        pps_proj_coloc_candts,
        e_part_id);

    /* INITIALIZATION OF 4x4 BR BLK */
    e_part_id = PART_ID_NxN_BR;
    ps_tl = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TL];
    ps_t = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TR];
    ps_tr = ps_invalid;  // invalid
    ps_l = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_BL];
    ps_bl = ps_invalid;  // invalid
    hme_init_pred_part(
        ps_pred_ctxt,
        ps_tl,
        ps_t,
        ps_tr,
        ps_l,
        ps_bl,
        ps_coloc,
        ps_zeromv_candt,
        pps_proj_coloc_candts,
        e_part_id);
}

void hme_init_pred_ctxt_encode(
    pred_ctxt_t *ps_pred_ctxt,
    search_results_t *ps_search_results,
    search_node_t *ps_coloc_candts,
    search_node_t *ps_zeromv_candt,
    mv_grid_t *ps_mv_grid,
    S32 pred_lx,
    S32 lambda,
    S32 lambda_q_shift,
    U08 **ppu1_ref_bits_tlu,
    S16 *pi2_ref_scf)
{
    search_node_t *ps_invalid, *ps_l, *ps_t, *ps_tl, *ps_tr, *ps_bl;
    search_node_t *ps_coloc;
    search_node_t *ps_grid_cu_base;
    CU_SIZE_T e_cu_size = ps_search_results->e_cu_size;

    /* Part Start, Part sizes in 4x4 units */
    S32 part_wd, part_ht, part_start_x, part_start_y;

    /* Partition type, number of partitions in type */
    S32 part_id;

    /* Coordinates of the CU in 4x4 units */
    S32 cu_start_x, cu_start_y;
    S32 shift = e_cu_size;

    /* top right and bot left validity at CU level */
    S32 cu_tr_valid, cu_bl_valid;
    /* strideo f the grid */
    S32 grid_stride = ps_mv_grid->i4_stride;

    ps_pred_ctxt->lambda = lambda;
    ps_pred_ctxt->lambda_q_shift = lambda_q_shift;
    ps_pred_ctxt->pred_lx = pred_lx;
    ps_pred_ctxt->mv_pel = 0;
    ps_pred_ctxt->ppu1_ref_bits_tlu = ppu1_ref_bits_tlu;
    ps_pred_ctxt->pi2_ref_scf = pi2_ref_scf;
    ps_pred_ctxt->proj_used = 1;

    cu_start_x = ps_search_results->u1_x_off >> 2;
    cu_start_y = ps_search_results->u1_y_off >> 2;

    /* Coloc always points to fixed global candt */
    ps_coloc = ps_coloc_candts;

    /* Go to base of the CU in the MV Grid */
    ps_grid_cu_base = &ps_mv_grid->as_node[0];
    ps_grid_cu_base += (ps_mv_grid->i4_start_offset + cu_start_x);
    ps_grid_cu_base += (grid_stride * cu_start_y);

    /* points to the real bottom left of the grid, will never be valid */
    ps_invalid = &ps_mv_grid->as_node[0];
    ps_invalid += (grid_stride * 17);

    {
        S32 shift = 1 + e_cu_size;
        cu_tr_valid = gau1_cu_tr_valid[cu_start_y >> shift][cu_start_x >> shift];
        cu_bl_valid = gau1_cu_bl_valid[cu_start_y >> shift][cu_start_x >> shift];
    }

    /*************************************************************************/
    /* for the case of    encode, the idea is to set up cants as follows     */
    /*                                                                       */
    /*    ____ ______________ ____ ____                                      */
    /*   | T0 | T1 | T2 | T3 | T4 | T5 |                                     */
    /*   |____|____|____|____|____|____|                                     */
    /*   | L1 |    |              |                                          */
    /*   |____|    |              |                                          */
    /*   | L2 | p0 |     p1       |                                          */
    /*   |____|    |              |                                          */
    /*   | L3 |    |              |                                          */
    /*   |____|    |              |                                          */
    /*   | L4 | L' |              |                                          */
    /*   |____|____|______________|                                          */
    /*   | BL |                                                              */
    /*   |____|                                                              */
    /*  The example is shown with 16x16 CU, though it can be generalized     */
    /*  This CU has 2 partitions, cu_wd = 4. also p_wd, p_ht are partition   */
    /*  width and ht in 4x4 units.                                           */
    /*  For a given CU, derive the top left, top and bottom left and top rt  */
    /*  pts. Left and top are assumed to be valid.                           */
    /*  IF there aretwo partitions in the CU (like p0 and p1) and vertical,  */
    /*  then for first partition, left, top, top left and top right valid    */
    /*  Bottom left is valid. store these validity flags. Also store the     */
    /*  grid offsets of the partitions w.r.t. CU start in units of 4x4.For p0*/
    /*  Left grid offset = -1, 3. Top Grd offset = -1, 0.                    */
    /*  Top left grid offset = -1, -1. Top right = 1, -1. BL = -1, 4.        */
    /*  For p1, validity flags are left, top, top left, top right, valid.    */
    /*  BL is invalid. Grid offsets are: Left = dont care. T = 1, -1 (T2)    */
    /*  TR = 4, -1 (T5). TL = 0, -1 (T1). BL = don't care.                   */
    /*  For p1, set the left pred candt to the best search result of p0.     */
    /*************************************************************************/

    /* Loop over all partitions, and identify the 5 neighbours */
    for(part_id = 0; part_id < TOT_NUM_PARTS; part_id++)
    {
        part_attr_t *ps_part_attr = &gas_part_attr_in_cu[part_id];
        S32 tr_valid, bl_valid, is_vert;
        search_node_t *ps_grid_pu_base;
        PART_TYPE_T e_part_type;
        PART_ID_T first_part;
        S32 part_num;

        e_part_type = ge_part_id_to_part_type[part_id];
        first_part = ge_part_type_to_part_id[e_part_type][0];
        is_vert = gau1_is_vert_part[e_part_type];
        part_num = gau1_part_id_to_part_num[part_id];
        tr_valid = gau1_partid_tr_valid[part_id] & cu_tr_valid;
        bl_valid = gau1_partid_bl_valid[part_id] & cu_bl_valid;

        part_start_x = (ps_part_attr->u1_x_start << shift) >> 2;
        part_start_y = (ps_part_attr->u1_y_start << shift) >> 2;
        part_wd = (ps_part_attr->u1_x_count << shift) >> 2;
        part_ht = (ps_part_attr->u1_y_count << shift) >> 2;

        /* go to top left of part */
        ps_grid_pu_base = ps_grid_cu_base + part_start_x;
        ps_grid_pu_base += (part_start_y * grid_stride);

        ps_tl = ps_grid_pu_base - 1 - grid_stride;
        ps_t = ps_grid_pu_base - grid_stride + part_wd - 1;
        ps_l = ps_grid_pu_base - 1 + ((part_ht - 1) * grid_stride);
        ps_tr = ps_t + 1;
        ps_bl = ps_l + grid_stride;

        if(!tr_valid)
            ps_tr = ps_invalid;
        if(!bl_valid)
            ps_bl = ps_invalid;

        if(part_num == 1)
        {
            /* for cases of two partitions 2nd part has 1st part as candt */
            /* if vertical type, left candt of 2nd part is 1st part.      */
            /* if horz type, top candt of 2nd part is 1st part.           */
            if(is_vert)
            {
                ps_l = ps_search_results->aps_part_results[pred_lx][first_part];
            }
            else
            {
                ps_t = ps_search_results->aps_part_results[pred_lx][first_part];
            }
        }
        if(part_num == 2)
        {
            /* only possible for NxN_BL */
            ps_t = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TL];
            ps_tr = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TR];
        }
        if(part_num == 3)
        {
            /* only possible for NxN_BR */
            ps_t = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TR];
            ps_tl = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_TL];
            ps_l = ps_search_results->aps_part_results[pred_lx][PART_ID_NxN_BL];
        }
        hme_init_pred_part(
            ps_pred_ctxt,
            ps_tl,
            ps_t,
            ps_tr,
            ps_l,
            ps_bl,
            ps_coloc,
            ps_zeromv_candt,
            NULL,
            (PART_ID_T)part_id);
    }
}

/**
********************************************************************************
*  @fn     compute_mv_cost_explicit(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for explicit search in layers not encoded
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_explicit(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
#define RETURN_FIXED_COST 0
    search_node_t *ps_pred_node_a = NULL, *ps_pred_node_b = NULL;
    pred_candt_nodes_t *ps_pred_nodes;
    S32 inp_shift = 2 - inp_mv_pel;
    S32 pred_shift = 2 - ps_pred_ctxt->mv_pel;
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdx2, mvdy1, mvdy2;
    S32 cost, ref_bits;

    /*************************************************************************/
    /* Logic for cost computation for explicit search. For such a search,    */
    /* it is guaranteed that all predictor candts have same ref id. The only */
    /* probable issue is with the availability which needs checking. This fxn*/
    /* does not suffer the need to scale predictor candts due to diff ref id */
    /*************************************************************************/

    /* Hack: currently we always assume 2Nx2N. */
    /* TODO: get rid of this hack and return cost tuned to each partition */
    ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];
    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_node->i1_ref_idx];

    /*************************************************************************/
    /* Priority to bottom left availability. Else we go to left. If both are */
    /* not available, then a remains null                                    */
    /*************************************************************************/
    if(ps_pred_nodes->ps_tl->u1_is_avail)
        ps_pred_node_a = ps_pred_nodes->ps_tl;
    else if(ps_pred_nodes->ps_l->u1_is_avail)
        ps_pred_node_a = ps_pred_nodes->ps_l;

    /*************************************************************************/
    /* For encoder, top left may not be really needed unless we use slices,  */
    /* and even then in ME it may not be relevant. So we only consider T or  */
    /* TR, as, if both T and TR are not available, TL also will not be       */
    /*************************************************************************/
    if(ps_pred_nodes->ps_tr->u1_is_avail)
        ps_pred_node_b = ps_pred_nodes->ps_tr;
    else if(ps_pred_nodes->ps_t->u1_is_avail)
        ps_pred_node_b = ps_pred_nodes->ps_t;

    if(ps_pred_node_a == NULL)
    {
        ps_pred_node_a = ps_pred_nodes->ps_coloc;
        if(ps_pred_node_b == NULL)
            ps_pred_node_b = ps_pred_nodes->ps_zeromv;
    }
    else if(ps_pred_node_b == NULL)
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
    else if(0 == hme_cmp_nodes(ps_pred_node_a, ps_pred_node_b))
    {
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
    }

    mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
    mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    mv_p_x = ps_pred_node_b->s_mv.i2_mvx;
    mv_p_y = ps_pred_node_b->s_mv.i2_mvy;
    COMPUTE_DIFF_MV(mvdx2, mvdy2, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx2 = ABS(mvdx2);
    mvdy2 = ABS(mvdy2);

    if((mvdx1 + mvdy1) < (mvdx2 + mvdy2))
    {
        cost =
            hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) + (mvdy1 > 0) + ref_bits + 2;
    }
    else
    {
        cost =
            hme_get_range(mvdx2) + hme_get_range(mvdy2) + (mvdx2 > 0) + (mvdy2 > 0) + ref_bits + 2;
    }
    {
        S32 rnd = 1 << (ps_pred_ctxt->lambda_q_shift - 1);
        return ((cost * ps_pred_ctxt->lambda + rnd) >> ps_pred_ctxt->lambda_q_shift);
    }
}
/**
********************************************************************************
*  @fn     compute_mv_cost_coarse(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_coarse(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    ARG_NOT_USED(e_part_id);

    return (compute_mv_cost_explicit(ps_node, ps_pred_ctxt, PART_ID_2Nx2N, inp_mv_pel));
}

/**
********************************************************************************
*  @fn     compute_mv_cost_coarse_high_speed(search_node_t *ps_node,
*                                            pred_ctxt_t *ps_pred_ctxt,
*                                            PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_coarse_high_speed(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    S32 rnd, mvx, mvy, i4_search_idx;
    S32 cost;

    mvx = ps_node->s_mv.i2_mvx;
    mvy = ps_node->s_mv.i2_mvy;
    i4_search_idx = ps_node->i1_ref_idx;

    cost = (2 * hme_get_range(ABS(mvx)) - 1) + (2 * hme_get_range(ABS(mvy)) - 1) + i4_search_idx;
    cost += (mvx != 0) ? 1 : 0;
    cost += (mvy != 0) ? 1 : 0;
    rnd = 1 << (ps_pred_ctxt->lambda_q_shift - 1);
    cost = (cost * ps_pred_ctxt->lambda + rnd) >> ps_pred_ctxt->lambda_q_shift;
    return cost;
}

/**
********************************************************************************
*  @fn     compute_mv_cost_explicit_refine(search_node_t *ps_node,
*                                          pred_ctxt_t *ps_pred_ctxt,
*                                          PART_ID_T e_part_id)
*
*  @brief  MV cost for explicit search in layers not encoded. Always returns
*          cost of the projected colocated candidate
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_explicit_refine(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    search_node_t *ps_pred_node_a = NULL;
    pred_candt_nodes_t *ps_pred_nodes;
    S32 inp_shift = 2 - inp_mv_pel;
    S32 pred_shift = 2 - ps_pred_ctxt->mv_pel;
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdy1;
    S32 cost, ref_bits;

    ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];
    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_node->i1_ref_idx];

    ps_pred_node_a = ps_pred_nodes->pps_proj_coloc[0];

    mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
    mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) + (mvdy1 > 0) + ref_bits + 2;

    {
        S32 rnd = 1 << (ps_pred_ctxt->lambda_q_shift - 1);
        return ((cost * ps_pred_ctxt->lambda + rnd) >> ps_pred_ctxt->lambda_q_shift);
    }
}

/**
********************************************************************************
*  @fn     compute_mv_cost_refine(search_node_t *ps_node,
*                   pred_ctxt_t *ps_pred_ctxt,
*                   PART_ID_T e_part_id)
*
*  @brief  MV cost for coarse explicit search in coarsest layer
*
*  @param[in]  ps_node: search node having mv and ref id for which to eval cost
*
*  @param[in]  ps_pred_ctxt : mv pred context
*
*  @param[in]  e_part_id : Partition id.
*
*  @return   Cost value

********************************************************************************
*/
S32 compute_mv_cost_refine(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    return (compute_mv_cost_explicit_refine(ps_node, ps_pred_ctxt, e_part_id, inp_mv_pel));
}

S32 compute_mv_cost_implicit(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    search_node_t *ps_pred_node_a = NULL, *ps_pred_node_b = NULL;
    pred_candt_nodes_t *ps_pred_nodes;
    S08 i1_ref_idx;
    S08 i1_ref_tl = -1, i1_ref_tr = -1, i1_ref_t = -1;
    S08 i1_ref_bl = -1, i1_ref_l = -1;
    S32 inp_shift = 2 - inp_mv_pel;
    S32 pred_shift; /* = 2 - ps_pred_ctxt->mv_pel;*/
    S32 ref_bits, cost;
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdx2, mvdy1, mvdy2;

    //return 0;
    i1_ref_idx = ps_node->i1_ref_idx;

    /*************************************************************************/
    /* Logic for cost computation for explicit search. For such a search,    */
    /* it is guaranteed that all predictor candts have same ref id. The only */
    /* probable issue is with the availability which needs checking. This fxn*/
    /* does not suffer the need to scale predictor candts due to diff ref id */
    /*************************************************************************/

    ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];
    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][i1_ref_idx];

    /*************************************************************************/
    /* Priority to bottom left availability. Else we go to left. If both are */
    /* not available, then a remains null                                    */
    /*************************************************************************/
    if(ps_pred_nodes->ps_bl->u1_is_avail)
        i1_ref_bl = ps_pred_nodes->ps_bl->i1_ref_idx;
    if(ps_pred_nodes->ps_l->u1_is_avail)
        i1_ref_l = ps_pred_nodes->ps_l->i1_ref_idx;
    if(i1_ref_bl == i1_ref_idx)
        ps_pred_node_a = ps_pred_nodes->ps_bl;
    else if(i1_ref_l == i1_ref_idx)
        ps_pred_node_a = ps_pred_nodes->ps_l;
    if(ps_pred_node_a == NULL)
    {
        if(i1_ref_bl != -1)
            ps_pred_node_a = ps_pred_nodes->ps_bl;
        else if(i1_ref_l != -1)
            ps_pred_node_a = ps_pred_nodes->ps_l;
    }

    /*************************************************************************/
    /* For encoder, top left may not be really needed unless we use slices,  */
    /* and even then in ME it may not be relevant. So we only consider T or  */
    /* TR, as, if both T and TR are not available, TL also will not be       */
    /*************************************************************************/
    if(ps_pred_nodes->ps_tr->u1_is_avail)
        i1_ref_tr = ps_pred_nodes->ps_tr->i1_ref_idx;
    if(ps_pred_nodes->ps_t->u1_is_avail)
        i1_ref_t = ps_pred_nodes->ps_t->i1_ref_idx;
    if(ps_pred_nodes->ps_tl->u1_is_avail)
        i1_ref_tl = ps_pred_nodes->ps_tl->i1_ref_idx;
    if(i1_ref_tr == i1_ref_idx)
        ps_pred_node_b = ps_pred_nodes->ps_tr;
    else if(i1_ref_t == i1_ref_idx)
        ps_pred_node_b = ps_pred_nodes->ps_t;
    else if(i1_ref_tl == i1_ref_idx)
        ps_pred_node_b = ps_pred_nodes->ps_tl;

    if(ps_pred_node_b == NULL)
    {
        if(i1_ref_tr != -1)
            ps_pred_node_b = ps_pred_nodes->ps_tr;
        else if(i1_ref_t != -1)
            ps_pred_node_b = ps_pred_nodes->ps_t;
        else if(i1_ref_tl != -1)
            ps_pred_node_b = ps_pred_nodes->ps_tl;
    }
    if(ps_pred_node_a == NULL)
    {
        ps_pred_node_a = ps_pred_nodes->ps_coloc;
        if(ps_pred_node_b == NULL)
            ps_pred_node_b = ps_pred_nodes->ps_zeromv;
    }
    else if(ps_pred_node_b == NULL)
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
    else if(0 == hme_cmp_nodes(ps_pred_node_a, ps_pred_node_b))
    {
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
    }

    if(ps_pred_node_a->i1_ref_idx != i1_ref_idx)
    {
        SCALE_FOR_POC_DELTA(mv_p_x, mv_p_y, ps_pred_node_a, i1_ref_idx, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    }
    pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
    COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    if(ps_pred_node_b->i1_ref_idx != i1_ref_idx)
    {
        SCALE_FOR_POC_DELTA(mv_p_x, mv_p_y, ps_pred_node_b, i1_ref_idx, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_b->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_b->s_mv.i2_mvy;
    }
    pred_shift = ps_pred_node_b->u1_subpel_done ? 0 : 2;
    COMPUTE_DIFF_MV(mvdx2, mvdy2, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx2 = ABS(mvdx2);
    mvdy2 = ABS(mvdy2);

    if((mvdx1 + mvdy1) < (mvdx2 + mvdy2))
    {
        cost = 2 * hme_get_range(mvdx1) + 2 * hme_get_range(mvdy1) + 2 * (mvdx1 > 0) +
               2 * (mvdy1 > 0) + ref_bits + 2;
    }
    else
    {
        cost = 2 * hme_get_range(mvdx2) + 2 * hme_get_range(mvdy2) + 2 * (mvdx2 > 0) +
               2 * (mvdy2 > 0) + ref_bits + 2;
    }
    {
        /* Part bits in Q1, so evaluate cost as ((mv_cost<<1) + partbitsQ1 + rnd)>>(q+1)*/
        S32 rnd = 1 << (ps_pred_ctxt->lambda_q_shift);
        S32 tot_cost = (cost * ps_pred_ctxt->lambda) << 1;

        tot_cost += (gau1_bits_for_part_id_q1[e_part_id] * ps_pred_ctxt->lambda);
        return ((tot_cost + rnd) >> (ps_pred_ctxt->lambda_q_shift + 1));
    }
}

S32 compute_mv_cost_implicit_high_speed(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    search_node_t *ps_pred_node_a = NULL, *ps_pred_node_b = NULL;
    pred_candt_nodes_t *ps_pred_nodes;
    S08 i1_ref_idx;
    S08 i1_ref_tr = -1;
    S08 i1_ref_l = -1;
    S32 inp_shift = 2 - inp_mv_pel;
    S32 pred_shift; /* = 2 - ps_pred_ctxt->mv_pel; */
    S32 ref_bits, cost;
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdx2, mvdy1, mvdy2;

    i1_ref_idx = ps_node->i1_ref_idx;

    ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];
    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][i1_ref_idx];

    /*************************************************************************/
    /* Priority to bottom left availability. Else we go to left. If both are */
    /* not available, then a remains null                                    */
    /*************************************************************************/
    if(ps_pred_nodes->ps_l->u1_is_avail)
    {
        i1_ref_l = ps_pred_nodes->ps_l->i1_ref_idx;
        ps_pred_node_a = ps_pred_nodes->ps_l;
    }

    /*************************************************************************/
    /* For encoder, top left may not be really needed unless we use slices,  */
    /* and even then in ME it may not be relevant. So we only consider T or  */
    /* TR, as, if both T and TR are not available, TL also will not be       */
    /*************************************************************************/

    if((!(ps_pred_ctxt->proj_used) && (ps_pred_nodes->ps_tr->u1_is_avail)))
    {
        i1_ref_tr = ps_pred_nodes->ps_tr->i1_ref_idx;
        ps_pred_node_b = ps_pred_nodes->ps_tr;
    }
    else
    {
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
    }

    if(ps_pred_node_a == NULL)
    {
        ps_pred_node_a = ps_pred_nodes->ps_coloc;

        if(ps_pred_node_b == ps_pred_nodes->ps_coloc)
            ps_pred_node_b = ps_pred_nodes->ps_zeromv;
    }

    if(ps_pred_node_a->i1_ref_idx != i1_ref_idx)
    {
        SCALE_FOR_POC_DELTA(mv_p_x, mv_p_y, ps_pred_node_a, i1_ref_idx, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    }

    pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
    COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    if(ps_pred_node_b->i1_ref_idx != i1_ref_idx)
    {
        SCALE_FOR_POC_DELTA(mv_p_x, mv_p_y, ps_pred_node_b, i1_ref_idx, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_b->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_b->s_mv.i2_mvy;
    }

    pred_shift = ps_pred_node_b->u1_subpel_done ? 0 : 2;
    COMPUTE_DIFF_MV(mvdx2, mvdy2, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx2 = ABS(mvdx2);
    mvdy2 = ABS(mvdy2);

    if((mvdx1 + mvdy1) < (mvdx2 + mvdy2))
    {
        cost =
            hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) + (mvdy1 > 0) + ref_bits + 2;
    }
    else
    {
        cost =
            hme_get_range(mvdx2) + hme_get_range(mvdy2) + (mvdx2 > 0) + (mvdy2 > 0) + ref_bits + 2;
    }
    {
        /* Part bits in Q1, so evaluate cost as ((mv_cost<<1) + partbitsQ1 + rnd)>>(q+1)*/
        S32 rnd = 1 << (ps_pred_ctxt->lambda_q_shift - 1);
        S32 tot_cost = (cost * ps_pred_ctxt->lambda);

        return ((tot_cost + rnd) >> (ps_pred_ctxt->lambda_q_shift));
    }
}

S32 compute_mv_cost_implicit_high_speed_modified(
    search_node_t *ps_node, pred_ctxt_t *ps_pred_ctxt, PART_ID_T e_part_id, S32 inp_mv_pel)
{
    search_node_t *ps_pred_node_a = NULL;
    pred_candt_nodes_t *ps_pred_nodes;
    S32 inp_shift = 2 - inp_mv_pel;
    S32 pred_shift; /* = 2 - ps_pred_ctxt->mv_pel; */
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdy1;
    S32 cost, ref_bits;

    ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[e_part_id];
    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_node->i1_ref_idx];

    ps_pred_node_a = ps_pred_nodes->ps_mvp_node;

    mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
    mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
    COMPUTE_DIFF_MV(mvdx1, mvdy1, ps_node, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) + (mvdy1 > 0) + ref_bits + 2;

    {
        S32 rnd = 1 << (ps_pred_ctxt->lambda_q_shift - 1);
        return ((cost * ps_pred_ctxt->lambda + rnd) >> ps_pred_ctxt->lambda_q_shift);
    }
}

void hme_update_results_grid_pu_bestn_xtreme_speed(result_upd_prms_t *ps_result_prms)
{
    /*The function modified with assumption that only 2NxN_B and Nx2N_R is modified */

    search_node_t s_search_node_grid;
    const search_node_t *ps_search_node_base;
    search_node_t *ps_search_node_grid, *ps_best_node;
    S32 i4_min_cost = (MAX_32BIT_VAL), i4_search_idx;
    S32 num_results, i4_unique_id = -1, i4_grid_pt;
    search_results_t *ps_search_results;
    S32 *pi4_valid_part_ids;
    S32 i4_step = ps_result_prms->i4_step;
    S32 i4_grid_mask, i, i4_min_id;
    S32 i4_tot_cost, i4_mv_cost, i4_sad, id;
    S32 *pi4_sad_grid = ps_result_prms->pi4_sad_grid;
    S32 grid_count = 0;
    S32 pred_lx;

    i4_min_id = (S32)PT_C;
    i4_min_cost = MAX_32BIT_VAL;
    ps_search_node_grid = &s_search_node_grid;
    ps_search_node_base = ps_result_prms->ps_search_node_base;
    *ps_search_node_grid = *ps_search_node_base;
    pi4_valid_part_ids = ps_result_prms->pi4_valid_part_ids;
    ps_search_results = ps_result_prms->ps_search_results;
    num_results = (S32)ps_search_results->u1_num_results_per_part;
    i4_grid_mask = ps_result_prms->i4_grid_mask;

    for(i = 0; i < 9; i++)
    {
        if(i4_grid_mask & (1 << i))
            grid_count++;
    }

    /* Some basic assumptions: only single pt, only part updates */
    /* and more than 1 best result to be computed.               */
    //ASSERT(ps_result_prms->i4_grid_mask != 1);
    //ASSERT(ps_result_prms->i4_part_mask != ENABLE_2Nx2N);
    //ASSERT(ps_search_results->num_results > 1);

    i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
    pred_lx = 1 - ps_search_results->pu1_is_past[i4_search_idx];

    /*************************************************************************/
    /* Supposing we do hte result update for a unique partid, we can */
    /* store the best pt id in the grid and also min cost is return */
    /* param. This will be useful for early exit cases.             */
    /* TODO : once we have separate fxn for unique part+grid, we can */
    /* do away with this code here                                   */
    /*************************************************************************/
    //if (pi4_valid_part_ids[1] == -1)
    i4_unique_id = pi4_valid_part_ids[0];

    /* pi4_valid_part_ids contains all the valid ids. We loop through */
    /* this till we encounter -1. This is easier than having to       */
    /* figure out part by part, besides, active part decision is      */
    /* usually fixed for a given duration of search, e.g. entire fpel */
    /* refinement for a blk/cu will use fixed valid part mask         */
    id = pi4_valid_part_ids[0];

    /*****************************************************************/
    /* points to the best search results corresponding to this       */
    /* specific part type.                                           */
    /*****************************************************************/
    ps_best_node = ps_search_results->aps_part_results[i4_search_idx][id];

    /*************************************************************************/
    /* Outer loop runs through all active pts in the grid                    */
    /*************************************************************************/
    for(i4_grid_pt = 0; i4_grid_pt < (S32)NUM_GRID_PTS; i4_grid_pt++)
    {
        if(!(i4_grid_mask & (1 << i4_grid_pt)))
            continue;

        /* For the pt in the grid, update mvx and y depending on */
        /* location of pt. Updates are in FPEL units.            */
        ps_search_node_grid->s_mv.i2_mvx = ps_search_node_base->s_mv.i2_mvx;
        ps_search_node_grid->s_mv.i2_mvy = ps_search_node_base->s_mv.i2_mvy;
        ps_search_node_grid->s_mv.i2_mvx += (S16)(i4_step * gai1_grid_id_to_x[i4_grid_pt]);
        ps_search_node_grid->s_mv.i2_mvy += (S16)(i4_step * gai1_grid_id_to_y[i4_grid_pt]);

        {
            /* evaluate mv cost and totalcost for this part for this given mv*/
            i4_mv_cost = compute_mv_cost_coarse_high_speed(
                ps_search_node_grid,
                &ps_search_results->as_pred_ctxt[pred_lx],
                (PART_ID_T)id,
                MV_RES_FPEL);

            i4_sad = pi4_sad_grid[grid_count * id];
            i4_tot_cost = i4_sad + i4_mv_cost;

            ASSERT(i4_unique_id == id);
            ASSERT(num_results == 1);

            /*****************************************************************/
            /* We do not labor through the results if the total cost worse   */
            /* than the last of the results.                                 */
            /*****************************************************************/
            if(i4_tot_cost < ps_best_node[num_results - 1].i4_tot_cost)
            {
                i4_min_id = i4_grid_pt;
                ps_result_prms->i4_min_cost = i4_tot_cost;

                ps_best_node[0] = *ps_search_node_grid;
                ps_best_node[0].i4_sad = i4_sad;
                ps_best_node[0].i4_mv_cost = i4_mv_cost;
                ps_best_node[0].i4_tot_cost = i4_tot_cost;
            }
        }
        pi4_sad_grid++;
    }
    ps_result_prms->i4_min_id = i4_min_id;
}

void hme_update_results_grid_pu_bestn(result_upd_prms_t *ps_result_prms)
{
    search_node_t s_search_node_grid;
    const search_node_t *ps_search_node_base;
    search_node_t *ps_search_node_grid, *ps_best_node;
    S32 i4_min_cost = (MAX_32BIT_VAL), i4_search_idx;
    S32 num_results, i4_unique_id = -1, i4_grid_pt;
    search_results_t *ps_search_results;
    S32 *pi4_valid_part_ids;
    S32 i4_step = ps_result_prms->i4_step;
    S32 i4_grid_mask, i4_count, i, i4_min_id;
    S32 i4_tot_cost, i4_mv_cost, i4_sad, id;
    S32 *pi4_sad_grid = ps_result_prms->pi4_sad_grid;
    S32 grid_count = 0;
    S32 pred_lx;

    i4_min_id = (S32)PT_C;
    i4_min_cost = MAX_32BIT_VAL;
    ps_search_node_grid = &s_search_node_grid;
    ps_search_node_base = ps_result_prms->ps_search_node_base;
    *ps_search_node_grid = *ps_search_node_base;
    pi4_valid_part_ids = ps_result_prms->pi4_valid_part_ids;
    ps_search_results = ps_result_prms->ps_search_results;
    num_results = (S32)ps_search_results->u1_num_results_per_part;
    i4_grid_mask = ps_result_prms->i4_grid_mask;

    for(i = 0; i < 9; i++)
    {
        if(i4_grid_mask & (1 << i))
        {
            grid_count++;
        }
    }

    i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
    pred_lx = 1 - ps_search_results->pu1_is_past[i4_search_idx];

    i4_unique_id = pi4_valid_part_ids[0];

    /*************************************************************************/
    /* Outer loop runs through all active pts in the grid                    */
    /*************************************************************************/
    for(i4_grid_pt = 0; i4_grid_pt < (S32)NUM_GRID_PTS; i4_grid_pt++)
    {
        if(!(i4_grid_mask & (1 << i4_grid_pt)))
        {
            continue;
        }

        /* For the pt in the grid, update mvx and y depending on */
        /* location of pt. Updates are in FPEL units.            */
        ps_search_node_grid->s_mv.i2_mvx = ps_search_node_base->s_mv.i2_mvx;
        ps_search_node_grid->s_mv.i2_mvy = ps_search_node_base->s_mv.i2_mvy;
        ps_search_node_grid->s_mv.i2_mvx += (S16)(i4_step * gai1_grid_id_to_x[i4_grid_pt]);
        ps_search_node_grid->s_mv.i2_mvy += (S16)(i4_step * gai1_grid_id_to_y[i4_grid_pt]);

        i4_count = 0;

        while((id = pi4_valid_part_ids[i4_count]) >= 0)
        {
            /*****************************************************************/
            /* points to the best search results corresponding to this       */
            /* specific part type.                                           */
            /*****************************************************************/
            ps_best_node = ps_search_results->aps_part_results[i4_search_idx][id];

            /* evaluate mv cost and totalcost for this part for this given mv*/
            i4_mv_cost = ps_result_prms->pf_mv_cost_compute(
                ps_search_node_grid,
                &ps_search_results->as_pred_ctxt[pred_lx],
                (PART_ID_T)id,
                MV_RES_FPEL);

            i4_sad = pi4_sad_grid[grid_count * id];
            i4_tot_cost = i4_sad + i4_mv_cost;

            if(i4_unique_id == id)
            {
                if(i4_tot_cost < ps_result_prms->i4_min_cost)
                {
                    i4_min_id = i4_grid_pt;
                    ps_result_prms->i4_min_cost = i4_tot_cost;
                }
            }

            if(i4_tot_cost < ps_best_node[num_results - 1].i4_tot_cost)
            {
                for(i = 0; i < num_results - 1; i++)
                {
                    if(i4_tot_cost < ps_best_node[i].i4_tot_cost)
                    {
                        memmove(
                            ps_best_node + i + 1,
                            ps_best_node + i,
                            sizeof(search_node_t) * (num_results - 1 - i));
                        break;
                    }
                    else if(i4_tot_cost == ps_best_node[i].i4_tot_cost)
                    {
                        if(0 == hme_cmp_nodes(ps_search_node_grid, ps_best_node + i))
                            break;
                    }
                }
                ps_best_node[i] = *ps_search_node_grid;
                ps_best_node[i].i4_sad = i4_sad;
                ps_best_node[i].i4_mv_cost = i4_mv_cost;
                ps_best_node[i].i4_tot_cost = i4_tot_cost;
            }
            i4_count++;
        }
        pi4_sad_grid++;
    }
    ps_result_prms->i4_min_id = i4_min_id;
}

/**
********************************************************************************
*  @fn     hme_update_results_grid_pu_bestn_no_encode(result_upd_prms_t *ps_result_prms)
*
*  @brief  Updates results for the case where 1 best result is to be updated
*          for a given pt, for several parts
*          Note : The function is replicated for CLIPing the cost to 16bit to make
*                  bit match with SIMD version
*
*  @param[in]  result_upd_prms_t : Contains the input parameters to this fxn
*
*  @return   The result_upd_prms_t structure is updated for all the active
*            parts in case the current candt has results for any given part
*             that is the best result for that part
********************************************************************************
*/
void hme_update_results_grid_pu_bestn_no_encode(result_upd_prms_t *ps_result_prms)
{
    search_node_t s_search_node_grid;
    const search_node_t *ps_search_node_base;
    search_node_t *ps_search_node_grid, *ps_best_node;
    S32 i4_min_cost = (MAX_32BIT_VAL), i4_search_idx;
    S32 num_results, i4_unique_id = -1, i4_grid_pt;
    search_results_t *ps_search_results;
    S32 *pi4_valid_part_ids;
    S32 i4_step = ps_result_prms->i4_step;
    S32 i4_grid_mask, i4_count, i, i4_min_id;
    S32 i4_tot_cost, i4_mv_cost, i4_sad, id;
    S32 *pi4_sad_grid = ps_result_prms->pi4_sad_grid;
    S32 grid_count = 0;
    S32 pred_lx;

    i4_min_id = (S32)PT_C;
    i4_min_cost = MAX_32BIT_VAL;
    ps_search_node_grid = &s_search_node_grid;
    ps_search_node_base = ps_result_prms->ps_search_node_base;
    *ps_search_node_grid = *ps_search_node_base;
    pi4_valid_part_ids = ps_result_prms->pi4_valid_part_ids;
    ps_search_results = ps_result_prms->ps_search_results;
    num_results = (S32)ps_search_results->u1_num_results_per_part;
    i4_grid_mask = ps_result_prms->i4_grid_mask;

    for(i = 0; i < 9; i++)
    {
        if(i4_grid_mask & (1 << i))
            grid_count++;
    }

    /* Some basic assumptions: only single pt, only part updates */
    /* and more than 1 best result to be computed.               */

    i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
    pred_lx = 1 - ps_search_results->pu1_is_past[i4_search_idx];

    /*************************************************************************/
    /* Supposing we do hte result update for a unique partid, we can */
    /* store the best pt id in the grid and also min cost is return */
    /* param. This will be useful for early exit cases.             */
    /* TODO : once we have separate fxn for unique part+grid, we can */
    /* do away with this code here                                   */
    /*************************************************************************/
    //if (pi4_valid_part_ids[1] == -1)
    i4_unique_id = pi4_valid_part_ids[0];

    /*************************************************************************/
    /* Outer loop runs through all active pts in the grid                    */
    /*************************************************************************/
    for(i4_grid_pt = 0; i4_grid_pt < (S32)NUM_GRID_PTS; i4_grid_pt++)
    {
        if(!(i4_grid_mask & (1 << i4_grid_pt)))
            continue;

        /* For the pt in the grid, update mvx and y depending on */
        /* location of pt. Updates are in FPEL units.            */
        ps_search_node_grid->s_mv.i2_mvx = ps_search_node_base->s_mv.i2_mvx;
        ps_search_node_grid->s_mv.i2_mvy = ps_search_node_base->s_mv.i2_mvy;
        ps_search_node_grid->s_mv.i2_mvx += (S16)(i4_step * gai1_grid_id_to_x[i4_grid_pt]);
        ps_search_node_grid->s_mv.i2_mvy += (S16)(i4_step * gai1_grid_id_to_y[i4_grid_pt]);

        i4_count = 0;

        /* pi4_valid_part_ids contains all the valid ids. We loop through */
        /* this till we encounter -1. This is easier than having to       */
        /* figure out part by part, besides, active part decision is      */
        /* usually fixed for a given duration of search, e.g. entire fpel */
        /* refinement for a blk/cu will use fixed valid part mask         */

        while((id = pi4_valid_part_ids[i4_count]) >= 0)
        {
            //ps_search_node_grid->e_part_type = (PART_TYPE_T)id;

            /*****************************************************************/
            /* points to the best search results corresponding to this       */
            /* specific part type.                                           */
            /*****************************************************************/
            ps_best_node = ps_search_results->aps_part_results[i4_search_idx][id];

            /* evaluate mv cost and totalcost for this part for this given mv*/
            i4_mv_cost = ps_result_prms->pf_mv_cost_compute(
                ps_search_node_grid,
                &ps_search_results->as_pred_ctxt[pred_lx],
                (PART_ID_T)id,
                MV_RES_FPEL);

            i4_sad = pi4_sad_grid[grid_count * id];

            /* Clipping to 16 bit to bit match with SIMD version */
            i4_mv_cost = CLIP_S16(i4_mv_cost);
            i4_sad = CLIP_S16(i4_sad);

            i4_tot_cost = i4_sad + i4_mv_cost;
            /* Clipping to 16 bit to bit match with SIMD version */
            i4_tot_cost = CLIP_S16(i4_tot_cost);

            if(i4_unique_id == id)
            {
                if(i4_tot_cost < ps_result_prms->i4_min_cost)
                {
                    i4_min_id = i4_grid_pt;
                    ps_result_prms->i4_min_cost = i4_tot_cost;
                }
            }

            /*****************************************************************/
            /* We do not labor through the results if the total cost worse   */
            /* than the last of the results.                                 */
            /*****************************************************************/
            if(i4_tot_cost < ps_best_node[num_results - 1].i4_tot_cost)
            {
                S32 eq_cost = 0;
                /*************************************************************/
                /* Identify where the current result isto be placed.Basically*/
                /* find the node which has cost just higher thannodeundertest*/
                /*************************************************************/
                for(i = 0; i < num_results - 1; i++)
                {
                    if(i4_tot_cost < ps_best_node[i].i4_tot_cost)
                    {
                        memmove(
                            ps_best_node + i + 1,
                            ps_best_node + i,
                            sizeof(search_node_t) * (num_results - 1 - i));
                        break;
                    }
                    else if(i4_tot_cost == ps_best_node[i].i4_tot_cost)
                    {
                        //if (0 == hme_cmp_nodes(ps_search_node_grid, ps_best_node+i))
                        //  break;
                        /* When cost is same we comp. the nodes and if it's same skip. */
                        /* We don't want to add this code to intrinsic. So we are      */
                        /* commenting it. The quality impact was minor when we did the */
                        /* regression.                                                 */
                        eq_cost = 1;
                    }
                }
                if(!eq_cost)
                {
                    ps_best_node[i] = *ps_search_node_grid;
                    ps_best_node[i].i4_sad = i4_sad;
                    ps_best_node[i].i4_mv_cost = i4_mv_cost;
                    ps_best_node[i].i4_tot_cost = i4_tot_cost;
                }
            }
            i4_count++;
        }
        pi4_sad_grid++;
    }
    ps_result_prms->i4_min_id = i4_min_id;
}

/**
********************************************************************************
*  @fn     hme_update_results_pt_npu_best1(result_upd_prms_t *ps_result_prms)
*
*  @brief  Updates results for the case where 1 best result is to be updated
*          for a given pt, for several parts
*
*  @param[in]  ps_result_prms. Contains the input parameters to this fxn
*              ::ps_pred_info : contains cost fxn ptr and predictor info
*              ::pi4_sad : 17x9 SAD Grid, this case, only 1st 17 entries valid
*              ::ps_search_results: Search results structure
*              ::i1_ref_id : Reference index
*              ::i4_grid_mask: Dont Care for this fxn
*              ::pi4_valid_part_ids : valid part ids
*              ::ps_search_node_base: Contains the centre pt candt info.
*
*  @return   The ps_search_results structure is updated for all the active
*            parts in case the current candt has results for any given part
*             that is the best result for that part
********************************************************************************
*/

void hme_update_results_pt_pu_best1_subpel_hs(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms)
{
    search_node_t *ps_search_node_base, *ps_best_node;
    search_results_t *ps_search_results;
    S32 id, i4_search_idx = ps_result_prms->u1_pred_lx;
    S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
    S32 num_results, i;
    S32 *pi4_valid_part_ids;

    pi4_valid_part_ids = ps_result_prms->pi4_valid_part_ids;
    /* Some basic assumptions: only single pt, only part updates */
    /* and more than 1 best result to be computed.               */
    ASSERT(ps_result_prms->i4_grid_mask == 1);

    ps_search_results = ps_result_prms->ps_search_results;
    num_results = (S32)ps_search_results->u1_num_results_per_part;

    /* Compute mv cost, total cost */
    ps_search_node_base = (search_node_t *)ps_result_prms->ps_search_node_base;

    while((id = pi4_valid_part_ids[i4_count]) >= 0)
    {
        S32 update_required = 1;

        ps_best_node = ps_search_results->aps_part_results[i4_search_idx][id];
        /* Use a pre-computed cost instead of freshly evaluating subpel cost */
        i4_mv_cost = ps_best_node->i4_mv_cost;
        i4_sad = ps_result_prms->pi4_sad_grid[id];
        i4_tot_cost = i4_sad + i4_mv_cost;

        /* We do not labor through the results if the total cost is worse than   */
        /* the last of the results.                                              */
        if(i4_tot_cost < ps_best_node[num_results - 1].i4_tot_cost)
        {
            /* Identify where the current result is to be placed. Basically find  */
            /* the node which has cost just higher than node under test           */
            for(i = 0; i < num_results - 1; i++)
            {
                if(ps_best_node[i].i1_ref_idx != -1)
                {
                    if(i4_tot_cost < ps_best_node[i].i4_tot_cost)
                    {
                        memmove(
                            ps_best_node + i + 1,
                            ps_best_node + i,
                            sizeof(search_node_t) * (num_results - 1 - i));
                        break;
                    }
                    else if(i4_tot_cost == ps_best_node[i].i4_tot_cost)
                    {
                        update_required = 0;
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            if(update_required)
            {
                /* Update when either ref_idx or mv's are different */
                ps_best_node[i] = *ps_search_node_base;
                ps_best_node[i].i4_sad = i4_sad;
                ps_best_node[i].i4_mv_cost = i4_mv_cost;
                ps_best_node[i].i4_tot_cost = i4_tot_cost;
            }
        }
        i4_count++;
    }
}

void hme_update_results_pt_pu_best1_subpel_hs_1(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms)
{
    search_node_t *ps_search_node_base, *ps_best_node;
    search_results_t *ps_search_results;
    S32 id, i4_search_idx = ps_result_prms->u1_pred_lx;
    S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
    S32 num_results;
    S32 *pi4_valid_part_ids;

    pi4_valid_part_ids = ps_result_prms->pi4_valid_part_ids;
    /* Some basic assumptions: only single pt, only part updates */
    /* and more than 1 best result to be computed.               */
    ASSERT(ps_result_prms->i4_grid_mask == 1);

    ps_search_results = ps_result_prms->ps_search_results;
    num_results = (S32)ps_search_results->u1_num_results_per_part;

    /* Compute mv cost, total cost */
    ps_search_node_base = (search_node_t *)ps_result_prms->ps_search_node_base;

    while((id = pi4_valid_part_ids[i4_count]) >= 0)
    {
        S32 update_required = 0;

        ps_best_node = ps_search_results->aps_part_results[i4_search_idx][id];
        /* Use a pre-computed cost instead of freshly evaluating subpel cost */
        i4_mv_cost = ps_best_node->i4_mv_cost;
        i4_sad = ps_result_prms->pi4_sad_grid[id];
        i4_tot_cost = i4_sad + i4_mv_cost;

        /* We do not labor through the results if the total cost is worse than   */
        /* the last of the results.                                              */
        if(i4_tot_cost < ps_best_node[1].i4_tot_cost)
        {
            S32 sdi_value = 0;

            update_required = 2;
            /* Identify where the current result is to be placed. Basically find  */
            /* the node which has cost just higher than node under test           */
            {
                if(i4_tot_cost < ps_best_node[0].i4_tot_cost)
                {
                    update_required = 1;
                    sdi_value = ps_best_node[0].i4_sad - i4_sad;
                }
                else if(
                    (ps_result_prms->i2_mv_x == ps_best_node[0].s_mv.i2_mvx) &&
                    (ps_result_prms->i2_mv_y == ps_best_node[0].s_mv.i2_mvy) &&
                    (ps_best_node[0].i1_ref_idx == ps_result_prms->i1_ref_idx))
                {
                    update_required = 0;
                }
            }
            if(update_required == 2)
            {
                subpel_refine_ctxt_t *ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;

                ps_subpel_refine_ctxt->i2_tot_cost[1][i4_count] = i4_tot_cost;
                ps_subpel_refine_ctxt->i2_mv_cost[1][i4_count] = i4_mv_cost;
                ps_subpel_refine_ctxt->i2_mv_x[1][i4_count] = ps_result_prms->i2_mv_x;
                ps_subpel_refine_ctxt->i2_mv_y[1][i4_count] = ps_result_prms->i2_mv_y;
                ps_subpel_refine_ctxt->i2_ref_idx[1][i4_count] = ps_result_prms->i1_ref_idx;
            }
            else if(update_required == 1)
            {
                subpel_refine_ctxt_t *ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;

                ps_subpel_refine_ctxt->i2_tot_cost[1][i4_count] =
                    ps_subpel_refine_ctxt->i2_tot_cost[0][i4_count];
                ps_subpel_refine_ctxt->i2_mv_cost[1][i4_count] =
                    ps_subpel_refine_ctxt->i2_mv_cost[0][i4_count];
                ps_subpel_refine_ctxt->i2_mv_x[1][i4_count] =
                    ps_subpel_refine_ctxt->i2_mv_x[0][i4_count];
                ps_subpel_refine_ctxt->i2_mv_y[1][i4_count] =
                    ps_subpel_refine_ctxt->i2_mv_y[0][i4_count];
                ps_subpel_refine_ctxt->i2_ref_idx[1][i4_count] =
                    ps_subpel_refine_ctxt->i2_ref_idx[0][i4_count];

                ps_subpel_refine_ctxt->i2_tot_cost[0][i4_count] = i4_tot_cost;
                ps_subpel_refine_ctxt->i2_mv_cost[0][i4_count] = i4_mv_cost;
                ps_subpel_refine_ctxt->i2_mv_x[0][i4_count] = ps_result_prms->i2_mv_x;
                ps_subpel_refine_ctxt->i2_mv_y[0][i4_count] = ps_result_prms->i2_mv_y;
                ps_subpel_refine_ctxt->i2_ref_idx[0][i4_count] = ps_result_prms->i1_ref_idx;
            }
        }
        i4_count++;
    }
}

/**
******************************************************************************
*  @brief Gives a result fxn ptr for a index [x] where x is as:
*         0 : single pt, no partial updates, 1 best result
*         1 : single pt, no partial updates, N best results
*         2 : single pt,    partial updates, 1 best result
*         3 : single pt,    partial updates, N best results
*         0 : grid     , no partial updates, 1 best result
*         1 : grid     , no partial updates, N best results
*         2 : grid     ,    partial updates, 1 best result
*         3 : grid     ,    partial updates, N best results
******************************************************************************
*/

static PF_RESULT_FXN_T g_pf_result_fxn[8] = { UPD_RES_PT_NPU_BEST1,   UPD_RES_PT_NPU_BESTN,
                                              UPD_RES_PT_PU_BEST1,    UPD_RES_PT_PU_BESTN,
                                              UPD_RES_GRID_NPU_BEST1, UPD_RES_GRID_NPU_BESTN,
                                              UPD_RES_GRID_PU_BEST1,  UPD_RES_GRID_PU_BESTN };

/**
********************************************************************************
*  @fn     hme_get_result_fxn(i4_grid_mask, i4_part_mask, i4_num_results)
*
*  @brief  Obtains the suitable result function that evaluates COST and also
*           computes one or more best results for point/grid, single part or
*           more than one part.
*
*  @param[in]  i4_grid_mask : Mask containing which of 9 grid pts active
*
*  @param[in]  i4_part_mask : Mask containing which of the 17 parts active
*
*  @param[in]  i4_num_results: Number of active results
*
*  @return   Pointer to the appropriate result update function
********************************************************************************
*/
PF_RESULT_FXN_T hme_get_result_fxn(S32 i4_grid_mask, S32 i4_part_mask, S32 i4_num_results)
{
    S32 i4_is_grid = (i4_grid_mask != 1);
    S32 i4_is_pu = ((i4_part_mask & (i4_part_mask - 1)) != 0);
    S32 i4_res_gt1 = (i4_num_results > 1);
    S32 id;

    id = (i4_is_grid << 2) + (i4_is_pu << 1) + i4_res_gt1;

    return (g_pf_result_fxn[id]);
}

void hme_calc_sad_and_2_best_results(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    S32 i4_candt;
    S32 i4_inp_off;
    S32 i4_ref_offset;
    S32 i4_num_nodes;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    mv_refine_ctxt_t *ps_mv_refine_ctxt;
    search_node_t *ps_search_node;

    ps_mv_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * cur_buf_stride;
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    ps_search_node = ps_search_prms->ps_search_nodes;

    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES THE SAD AND UPDATES THE SAD GRID   */
        /**********************************************************************/
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
            {
                continue;
            }

            ps_err_prms->pu1_inp =
                ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
            ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_offset;
            ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
            ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }

        {
            S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
            S32 *pi4_valid_part_ids = &ps_mv_refine_ctxt->ai4_part_id[0];
            S32 best_node_cost;
            S32 second_best_node_cost;

            {
                S16 mvdx1, mvdy1;
                S32 i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
                search_results_t *ps_search_results = ps_result_prms->ps_search_results;
                S32 pred_lx = i4_search_idx;

                pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
                pred_candt_nodes_t *ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[PART_2Nx2N];
                search_node_t *ps_pred_node_a = ps_pred_nodes->ps_mvp_node;

                S32 inp_shift = 2;
                S32 pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
                S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
                S32 lambda = ps_pred_ctxt->lambda;
                S32 rnd = 1 << (lambda_q_shift - 1);
                S32 mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
                S32 mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
                S32 ref_bits =
                    ps_pred_ctxt
                        ->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_search_node->i1_ref_idx];

                COMPUTE_DIFF_MV(
                    mvdx1, mvdy1, ps_search_node, mv_p_x, mv_p_y, inp_shift, pred_shift);

                mvdx1 = ABS(mvdx1);
                mvdy1 = ABS(mvdy1);

                i4_mv_cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) +
                             (mvdy1 > 0) + ref_bits + 2;

                i4_mv_cost *= lambda;
                i4_mv_cost += rnd;
                i4_mv_cost >>= lambda_q_shift;

                i4_mv_cost = CLIP_U16(i4_mv_cost);
            }

            /*For each valid partition, update the refine_prm structure to reflect the best and second
            best candidates for that partition*/

            for(i4_count = 0; i4_count < ps_mv_refine_ctxt->i4_num_valid_parts; i4_count++)
            {
                S32 update_required = 0;
                S32 part_id = pi4_valid_part_ids[i4_count];
                S32 index = (ps_mv_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

                /*Calculate total cost*/
                i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
                i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);

                /*****************************************************************/
                /* We do not labor through the results if the total cost worse   */
                /* than the last of the results.                                 */
                /*****************************************************************/
                best_node_cost = CLIP_S16(ps_mv_refine_ctxt->i2_tot_cost[0][index]);
                second_best_node_cost = CLIP_S16(ps_mv_refine_ctxt->i2_tot_cost[1][index]);

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
                    else if(i4_tot_cost == best_node_cost)
                    {
                        update_required = 0;
                    }

                    if(update_required == 2)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] = ps_search_node->i1_ref_idx;
                    }
                    else if(update_required == 1)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] =
                            ps_mv_refine_ctxt->i2_tot_cost[0][index];
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] =
                            ps_mv_refine_ctxt->i2_mv_cost[0][index];
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_mv_refine_ctxt->i2_mv_x[0][index];
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_mv_refine_ctxt->i2_mv_y[0][index];
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] =
                            ps_mv_refine_ctxt->i2_ref_idx[0][index];

                        ps_mv_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[0][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[0][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[0][index] = ps_search_node->i1_ref_idx;
                    }
                }
            }
        }
        ps_search_node++;
    }

    {
        WORD32 i4_i;
        WORD32 part_id;
        search_node_t *ps_search_node = ps_search_prms->ps_search_nodes;
        for(i4_i = 0; i4_i < ps_mv_refine_ctxt->i4_num_valid_parts; i4_i++)
        {
            part_id = ps_mv_refine_ctxt->ai4_part_id[i4_i];
            if(ps_mv_refine_ctxt->i2_tot_cost[0][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[0][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[0][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[0][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[0][part_id] = ps_search_node->i1_ref_idx;
            }
            if(ps_mv_refine_ctxt->i2_tot_cost[1][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[1][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[1][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[1][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[1][part_id] = ps_search_node->i1_ref_idx;
            }
        }
    }
}

void hme_calc_sad_and_2_best_results_subpel(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms)
{
    S32 i4_candt;
    S32 i4_num_nodes;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    mv_refine_ctxt_t *ps_subpel_refine_ctxt;
    ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
    i4_num_nodes = 1;

    /* Run through each of the candts in a loop */
    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES THE SAD AND UPDATES THE SAD GRID   */
        /**********************************************************************/
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES UPDATES THE BEST RESULTS           */
        /**********************************************************************/
        {
            S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
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

    {
        WORD32 i4_count = 0;
        for(i4_count = 0; i4_count < TOT_NUM_PARTS; i4_count++)
        {
            WORD32 j;
            for(j = 0; j < 2; j++)
            {
                if(ps_subpel_refine_ctxt->i2_tot_cost[j][i4_count] >= MAX_SIGNED_16BIT_VAL)
                {
                    ps_subpel_refine_ctxt->ai2_fullpel_satd[j][i4_count] = MAX_SIGNED_16BIT_VAL;
                }
            }
        }
    }
}

void hme_calc_stim_injected_sad_and_2_best_results(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    mv_refine_ctxt_t *ps_mv_refine_ctxt;
    search_node_t *ps_search_node;

    S32 i4_candt;
    S32 i4_count;
    S32 i4_inp_off;
    S32 i4_ref_offset;
    S32 i4_num_nodes;
    ULWORD64 *au8_final_src_sigmaX, *au8_final_src_sigmaXSquared, au8_final_ref_sigmaX[17],
        au8_final_ref_sigmaXSquared[17];
    UWORD32 au4_4x4_ref_sigmaX[NUM_4X4], au4_4x4_ref_sigmaXSquared[NUM_4X4];
    S32 *pi4_valid_part_ids;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    ps_mv_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * cur_buf_stride;
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    ps_search_node = ps_search_prms->ps_search_nodes;
    pi4_valid_part_ids = &ps_mv_refine_ctxt->ai4_part_id[0];

    /* Set local pointer to point to partition level sigma values calculated in hme_refine */
    au8_final_src_sigmaX = ps_search_prms->pu8_part_src_sigmaX;
    au8_final_src_sigmaXSquared = ps_search_prms->pu8_part_src_sigmaXSquared;

    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
            {
                continue;
            }

            ps_err_prms->pu1_inp =
                ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
            ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_offset;
            ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
            ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                /* Compute sigmaX and sigmaX_Squared at 4x4 level for ref from ref_ptr */
                hme_compute_sigmaX_and_sigmaXSquared(
                    pu1_ref_ptr,
                    ref_buf_stride,
                    au4_4x4_ref_sigmaX,
                    au4_4x4_ref_sigmaXSquared,
                    4,
                    4,
                    16,
                    16,
                    1,
                    4);

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }

        {
            S32 i4_sad, i4_mv_cost, i4_tot_cost;
            S32 best_node_cost;
            S32 second_best_node_cost;
            ULWORD64 u8_temp_var, u8_temp_var1;
            ULWORD64 u8_ref_X_Square, u8_pure_dist, u8_src_var, u8_ref_var;

            {
                S16 mvdx1, mvdy1;
                S32 i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
                search_results_t *ps_search_results = ps_result_prms->ps_search_results;
                S32 pred_lx = i4_search_idx;

                pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
                pred_candt_nodes_t *ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[PART_2Nx2N];
                search_node_t *ps_pred_node_a = ps_pred_nodes->ps_mvp_node;

                S32 inp_shift = 2;
                S32 pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
                S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
                S32 lambda = ps_pred_ctxt->lambda;
                S32 rnd = 1 << (lambda_q_shift - 1);
                S32 mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
                S32 mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
                S32 ref_bits =
                    ps_pred_ctxt
                        ->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_search_node->i1_ref_idx];

                COMPUTE_DIFF_MV(
                    mvdx1, mvdy1, ps_search_node, mv_p_x, mv_p_y, inp_shift, pred_shift);

                mvdx1 = ABS(mvdx1);
                mvdy1 = ABS(mvdy1);

                i4_mv_cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) +
                             (mvdy1 > 0) + ref_bits + 2;

                i4_mv_cost *= lambda;
                i4_mv_cost += rnd;
                i4_mv_cost >>= lambda_q_shift;

                i4_mv_cost = CLIP_U16(i4_mv_cost);
            }

            for(i4_count = 0; i4_count < ps_mv_refine_ctxt->i4_num_valid_parts; i4_count++)
            {
                S32 i4_stim_injected_sad;
                S32 i4_stim_injected_cost;
                S32 i4_noise_term;
                unsigned long u4_shift_val;
                S32 i4_bits_req;

                S32 update_required = 0;
                S32 part_id = pi4_valid_part_ids[i4_count];
                S32 index = (ps_mv_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

                WORD32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

                S32 i4_inv_wt = ps_wt_inp_prms->a_inv_wpred_wt[ps_search_node->i1_ref_idx];

                if(ps_search_prms->i4_alpha_stim_multiplier)
                {
                    /* Compute ref sigmaX and sigmaX_Squared values for valid partitions from previously computed ref 4x4 level values */
                    hme_compute_final_sigma_of_pu_from_base_blocks(
                        au4_4x4_ref_sigmaX,
                        au4_4x4_ref_sigmaXSquared,
                        au8_final_ref_sigmaX,
                        au8_final_ref_sigmaXSquared,
                        16,
                        4,
                        part_id,
                        4);

                    u8_ref_X_Square =
                        (au8_final_ref_sigmaX[part_id] * au8_final_ref_sigmaX[part_id]);
                    u8_ref_var = (au8_final_ref_sigmaXSquared[part_id] - u8_ref_X_Square);

                    /* Multiply un-normalized src_var with inv_wt if its not same as default wt */
                    /* and shift the resulting src_var if its more than 27 bits to avoid overflow */
                    /* The amount by which it is shifted is passed on to u4_shift_val and applied equally on ref_var */
                    u4_shift_val = ihevce_calc_stim_injected_variance(
                        au8_final_src_sigmaX,
                        au8_final_src_sigmaXSquared,
                        &u8_src_var,
                        i4_inv_wt,
                        ps_wt_inp_prms->ai4_shift_val[ps_search_node->i1_ref_idx],
                        ps_wt_inp_prms->wpred_log_wdc,
                        part_id);

                    u8_ref_var = u8_ref_var >> u4_shift_val;

                    /* Do the same check on ref_var to avoid overflow and apply similar shift on src_var */
                    GETRANGE64(i4_bits_req, u8_ref_var);

                    if(i4_bits_req > 27)
                    {
                        u8_ref_var = u8_ref_var >> (i4_bits_req - 27);
                        u8_src_var = u8_src_var >> (i4_bits_req - 27);
                    }

                    if(u8_src_var == u8_ref_var)
                    {
                        u8_temp_var = (1 << STIM_Q_FORMAT);
                    }
                    else
                    {
                        u8_temp_var = (2 * u8_src_var * u8_ref_var);
                        u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                        u8_temp_var1 = (u8_src_var * u8_src_var) + (u8_ref_var * u8_ref_var);
                        u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                        u8_temp_var = (u8_temp_var / u8_temp_var1);
                    }

                    i4_noise_term = (UWORD32)u8_temp_var;

                    ASSERT(i4_noise_term >= 0);

                    i4_noise_term *= ps_search_prms->i4_alpha_stim_multiplier;
                }
                else
                {
                    i4_noise_term = 0;
                }
                u8_pure_dist = pi4_sad_grid[part_id];
                u8_pure_dist *= ((1 << (i4_q_level)) - (i4_noise_term));
                u8_pure_dist += (1 << ((i4_q_level)-1));
                i4_stim_injected_sad = (UWORD32)(u8_pure_dist >> (i4_q_level));

                i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
                i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);
                i4_stim_injected_sad = CLIP3(i4_stim_injected_sad, 0, 0x7fff);
                i4_stim_injected_cost = CLIP_S16(i4_stim_injected_sad + i4_mv_cost);

                best_node_cost = CLIP_S16(ps_mv_refine_ctxt->i2_stim_injected_cost[0][index]);
                second_best_node_cost =
                    CLIP_S16(ps_mv_refine_ctxt->i2_stim_injected_cost[1][index]);

                if(i4_stim_injected_cost < second_best_node_cost)
                {
                    update_required = 2;

                    if(i4_stim_injected_cost < best_node_cost)
                    {
                        update_required = 1;
                    }
                    else if(i4_stim_injected_cost == best_node_cost)
                    {
                        update_required = 0;
                    }

                    if(update_required == 2)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_stim_injected_cost[1][index] = i4_stim_injected_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] = ps_search_node->i1_ref_idx;
                    }
                    else if(update_required == 1)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] =
                            ps_mv_refine_ctxt->i2_tot_cost[0][index];
                        ps_mv_refine_ctxt->i2_stim_injected_cost[1][index] =
                            ps_mv_refine_ctxt->i2_stim_injected_cost[0][index];
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] =
                            ps_mv_refine_ctxt->i2_mv_cost[0][index];
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_mv_refine_ctxt->i2_mv_x[0][index];
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_mv_refine_ctxt->i2_mv_y[0][index];
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] =
                            ps_mv_refine_ctxt->i2_ref_idx[0][index];

                        ps_mv_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_stim_injected_cost[0][index] = i4_stim_injected_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[0][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[0][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[0][index] = ps_search_node->i1_ref_idx;
                    }
                }
            }
        }

        ps_search_node++;
    }

    {
        WORD32 i4_i;
        WORD32 part_id;
        search_node_t *ps_search_node = ps_search_prms->ps_search_nodes;
        for(i4_i = 0; i4_i < ps_mv_refine_ctxt->i4_num_valid_parts; i4_i++)
        {
            part_id = ps_mv_refine_ctxt->ai4_part_id[i4_i];
            if(ps_mv_refine_ctxt->i2_stim_injected_cost[0][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[0][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[0][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[0][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[0][part_id] = ps_search_node->i1_ref_idx;
            }
            if(ps_mv_refine_ctxt->i2_stim_injected_cost[1][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[1][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[1][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[1][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[1][part_id] = ps_search_node->i1_ref_idx;
            }
        }
    }
}

void hme_calc_sad_and_1_best_result(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    S32 i4_candt;
    S32 i4_inp_off;
    S32 i4_ref_offset;
    S32 i4_num_nodes;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    mv_refine_ctxt_t *ps_mv_refine_ctxt;
    search_node_t *ps_search_node;

    ps_mv_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * cur_buf_stride;
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    ps_search_node = ps_search_prms->ps_search_nodes;

    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES THE SAD AND UPDATES THE SAD GRID   */
        /**********************************************************************/
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
            {
                continue;
            }

            ps_err_prms->pu1_inp =
                ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
            ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_offset;
            ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
            ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }

        {
            S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
            S32 *pi4_valid_part_ids = &ps_mv_refine_ctxt->ai4_part_id[0];
            S32 best_node_cost;
            S32 second_best_node_cost;

            {
                S16 mvdx1, mvdy1;
                S32 i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
                search_results_t *ps_search_results = ps_result_prms->ps_search_results;
                S32 pred_lx = i4_search_idx;

                pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
                pred_candt_nodes_t *ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[PART_2Nx2N];
                search_node_t *ps_pred_node_a = ps_pred_nodes->ps_mvp_node;

                S32 inp_shift = 2;
                S32 pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
                S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
                S32 lambda = ps_pred_ctxt->lambda;
                S32 rnd = 1 << (lambda_q_shift - 1);
                S32 mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
                S32 mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
                S32 ref_bits =
                    ps_pred_ctxt
                        ->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_search_node->i1_ref_idx];

                COMPUTE_DIFF_MV(
                    mvdx1, mvdy1, ps_search_node, mv_p_x, mv_p_y, inp_shift, pred_shift);

                mvdx1 = ABS(mvdx1);
                mvdy1 = ABS(mvdy1);

                i4_mv_cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) +
                             (mvdy1 > 0) + ref_bits + 2;

                i4_mv_cost *= lambda;
                i4_mv_cost += rnd;
                i4_mv_cost >>= lambda_q_shift;

                i4_mv_cost = CLIP_U16(i4_mv_cost);
            }

            /*For each valid partition, update the refine_prm structure to reflect the best and second
            best candidates for that partition*/

            for(i4_count = 0; i4_count < ps_mv_refine_ctxt->i4_num_valid_parts; i4_count++)
            {
                S32 update_required = 0;
                S32 part_id = pi4_valid_part_ids[i4_count];
                S32 index = (ps_mv_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

                /*Calculate total cost*/
                i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
                i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);

                /*****************************************************************/
                /* We do not labor through the results if the total cost worse   */
                /* than the last of the results.                                 */
                /*****************************************************************/
                best_node_cost = CLIP_S16(ps_mv_refine_ctxt->i2_tot_cost[0][index]);
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
                    else if(i4_tot_cost == best_node_cost)
                    {
                        update_required = 0;
                    }

                    if(update_required == 2)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] = ps_search_node->i1_ref_idx;
                    }
                    else if(update_required == 1)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[0][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[0][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[0][index] = ps_search_node->i1_ref_idx;
                    }
                }
            }
        }
        ps_search_node++;
    }

    {
        WORD32 i4_i;
        WORD32 part_id;
        search_node_t *ps_search_node = ps_search_prms->ps_search_nodes;
        for(i4_i = 0; i4_i < ps_mv_refine_ctxt->i4_num_valid_parts; i4_i++)
        {
            part_id = ps_mv_refine_ctxt->ai4_part_id[i4_i];
            if(ps_mv_refine_ctxt->i2_tot_cost[0][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[0][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[0][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[0][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[0][part_id] = ps_search_node->i1_ref_idx;
            }
        }
    }
}

void hme_calc_stim_injected_sad_and_1_best_result(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    mv_refine_ctxt_t *ps_mv_refine_ctxt;
    search_node_t *ps_search_node;

    S32 i4_candt;
    S32 i4_count;
    S32 i4_inp_off;
    S32 i4_ref_offset;
    S32 i4_num_nodes;
    ULWORD64 *au8_final_src_sigmaX, *au8_final_src_sigmaXSquared, au8_final_ref_sigmaX[17],
        au8_final_ref_sigmaXSquared[17];
    UWORD32 au4_4x4_ref_sigmaX[NUM_4X4], au4_4x4_ref_sigmaXSquared[NUM_4X4];
    S32 *pi4_valid_part_ids;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;
    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    ps_mv_refine_ctxt = ps_search_prms->ps_fullpel_refine_ctxt;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * cur_buf_stride;
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    ps_search_node = ps_search_prms->ps_search_nodes;
    pi4_valid_part_ids = &ps_mv_refine_ctxt->ai4_part_id[0];

    /* Set local pointer to point to partition level sigma values calculated in hme_refine */
    au8_final_src_sigmaX = ps_search_prms->pu8_part_src_sigmaX;
    au8_final_src_sigmaXSquared = ps_search_prms->pu8_part_src_sigmaXSquared;

    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
            {
                continue;
            }

            ps_err_prms->pu1_inp =
                ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
            ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_offset;
            ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
            ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                /* Compute sigmaX and sigmaX_Squared at 4x4 level for ref from ref_ptr */
                hme_compute_sigmaX_and_sigmaXSquared(
                    pu1_ref_ptr,
                    ref_buf_stride,
                    au4_4x4_ref_sigmaX,
                    au4_4x4_ref_sigmaXSquared,
                    4,
                    4,
                    16,
                    16,
                    1,
                    4);

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }

        {
            S32 i4_sad, i4_mv_cost, i4_tot_cost;
            S32 best_node_cost;
            S32 second_best_node_cost;
            ULWORD64 u8_temp_var, u8_temp_var1;
            ULWORD64 u8_ref_X_Square, u8_pure_dist, u8_src_var, u8_ref_var;

            {
                S16 mvdx1, mvdy1;
                S32 i4_search_idx = (S32)ps_result_prms->i1_ref_idx;
                search_results_t *ps_search_results = ps_result_prms->ps_search_results;
                S32 pred_lx = i4_search_idx;

                pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[pred_lx];
                pred_candt_nodes_t *ps_pred_nodes = &ps_pred_ctxt->as_pred_nodes[PART_2Nx2N];
                search_node_t *ps_pred_node_a = ps_pred_nodes->ps_mvp_node;

                S32 inp_shift = 2;
                S32 pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
                S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
                S32 lambda = ps_pred_ctxt->lambda;
                S32 rnd = 1 << (lambda_q_shift - 1);
                S32 mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
                S32 mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
                S32 ref_bits =
                    ps_pred_ctxt
                        ->ppu1_ref_bits_tlu[ps_pred_ctxt->pred_lx][ps_search_node->i1_ref_idx];

                COMPUTE_DIFF_MV(
                    mvdx1, mvdy1, ps_search_node, mv_p_x, mv_p_y, inp_shift, pred_shift);

                mvdx1 = ABS(mvdx1);
                mvdy1 = ABS(mvdy1);

                i4_mv_cost = hme_get_range(mvdx1) + hme_get_range(mvdy1) + (mvdx1 > 0) +
                             (mvdy1 > 0) + ref_bits + 2;

                i4_mv_cost *= lambda;
                i4_mv_cost += rnd;
                i4_mv_cost >>= lambda_q_shift;

                i4_mv_cost = CLIP_U16(i4_mv_cost);
            }

            for(i4_count = 0; i4_count < ps_mv_refine_ctxt->i4_num_valid_parts; i4_count++)
            {
                S32 i4_stim_injected_sad;
                S32 i4_stim_injected_cost;
                S32 i4_noise_term;
                unsigned long u4_shift_val;
                S32 i4_bits_req;

                S32 update_required = 0;
                S32 part_id = pi4_valid_part_ids[i4_count];
                S32 index = (ps_mv_refine_ctxt->i4_num_valid_parts > 8) ? part_id : i4_count;

                WORD32 i4_q_level = STIM_Q_FORMAT + ALPHA_Q_FORMAT;

                S32 i4_inv_wt = ps_wt_inp_prms->a_inv_wpred_wt[ps_search_node->i1_ref_idx];

                if(ps_search_prms->i4_alpha_stim_multiplier)
                {
                    /* Compute ref sigmaX and sigmaX_Squared values for valid partitions from previously computed ref 4x4 level values */
                    hme_compute_final_sigma_of_pu_from_base_blocks(
                        au4_4x4_ref_sigmaX,
                        au4_4x4_ref_sigmaXSquared,
                        au8_final_ref_sigmaX,
                        au8_final_ref_sigmaXSquared,
                        16,
                        4,
                        part_id,
                        4);

                    u8_ref_X_Square =
                        (au8_final_ref_sigmaX[part_id] * au8_final_ref_sigmaX[part_id]);
                    u8_ref_var = (au8_final_ref_sigmaXSquared[part_id] - u8_ref_X_Square);

                    /* Multiply un-normalized src_var with inv_wt if its not same as default wt */
                    /* and shift the resulting src_var if its more than 27 bits to avoid overflow */
                    /* The amount by which it is shifted is passed on to u4_shift_val and applied equally on ref_var */
                    u4_shift_val = ihevce_calc_stim_injected_variance(
                        au8_final_src_sigmaX,
                        au8_final_src_sigmaXSquared,
                        &u8_src_var,
                        i4_inv_wt,
                        ps_wt_inp_prms->ai4_shift_val[ps_search_node->i1_ref_idx],
                        ps_wt_inp_prms->wpred_log_wdc,
                        part_id);

                    u8_ref_var = u8_ref_var >> u4_shift_val;

                    /* Do the same check on ref_var to avoid overflow and apply similar shift on src_var */
                    GETRANGE64(i4_bits_req, u8_ref_var);

                    if(i4_bits_req > 27)
                    {
                        u8_ref_var = u8_ref_var >> (i4_bits_req - 27);
                        u8_src_var = u8_src_var >> (i4_bits_req - 27);
                    }

                    if(u8_src_var == u8_ref_var)
                    {
                        u8_temp_var = (1 << STIM_Q_FORMAT);
                    }
                    else
                    {
                        u8_temp_var = (2 * u8_src_var * u8_ref_var);
                        u8_temp_var = (u8_temp_var * (1 << STIM_Q_FORMAT));
                        u8_temp_var1 = (u8_src_var * u8_src_var) + (u8_ref_var * u8_ref_var);
                        u8_temp_var = (u8_temp_var + (u8_temp_var1 / 2));
                        u8_temp_var = (u8_temp_var / u8_temp_var1);
                    }

                    i4_noise_term = (UWORD32)u8_temp_var;

                    ASSERT(i4_noise_term >= 0);

                    i4_noise_term *= ps_search_prms->i4_alpha_stim_multiplier;
                }
                else
                {
                    i4_noise_term = 0;
                }
                u8_pure_dist = pi4_sad_grid[part_id];
                u8_pure_dist *= ((1 << (i4_q_level)) - (i4_noise_term));
                u8_pure_dist += (1 << ((i4_q_level)-1));
                i4_stim_injected_sad = (UWORD32)(u8_pure_dist >> (i4_q_level));

                i4_sad = CLIP3(pi4_sad_grid[part_id], 0, 0x7fff);
                i4_tot_cost = CLIP_S16(i4_sad + i4_mv_cost);
                i4_stim_injected_sad = CLIP3(i4_stim_injected_sad, 0, 0x7fff);
                i4_stim_injected_cost = CLIP_S16(i4_stim_injected_sad + i4_mv_cost);

                best_node_cost = CLIP_S16(ps_mv_refine_ctxt->i2_stim_injected_cost[0][index]);
                second_best_node_cost = SHRT_MAX;

                if(i4_stim_injected_cost < second_best_node_cost)
                {
                    update_required = 0;

                    if(i4_stim_injected_cost < best_node_cost)
                    {
                        update_required = 1;
                    }
                    else if(i4_stim_injected_cost == best_node_cost)
                    {
                        update_required = 0;
                    }

                    if(update_required == 2)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[1][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_stim_injected_cost[1][index] = i4_stim_injected_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[1][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[1][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[1][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[1][index] = ps_search_node->i1_ref_idx;
                    }
                    else if(update_required == 1)
                    {
                        ps_mv_refine_ctxt->i2_tot_cost[0][index] = i4_tot_cost;
                        ps_mv_refine_ctxt->i2_stim_injected_cost[0][index] = i4_stim_injected_cost;
                        ps_mv_refine_ctxt->i2_mv_cost[0][index] = i4_mv_cost;
                        ps_mv_refine_ctxt->i2_mv_x[0][index] = ps_search_node->s_mv.i2_mvx;
                        ps_mv_refine_ctxt->i2_mv_y[0][index] = ps_search_node->s_mv.i2_mvy;
                        ps_mv_refine_ctxt->i2_ref_idx[0][index] = ps_search_node->i1_ref_idx;
                    }
                }
            }
        }

        ps_search_node++;
    }

    {
        WORD32 i4_i;
        WORD32 part_id;
        search_node_t *ps_search_node = ps_search_prms->ps_search_nodes;
        for(i4_i = 0; i4_i < ps_mv_refine_ctxt->i4_num_valid_parts; i4_i++)
        {
            part_id = ps_mv_refine_ctxt->ai4_part_id[i4_i];
            if(ps_mv_refine_ctxt->i2_stim_injected_cost[0][part_id] >= MAX_SIGNED_16BIT_VAL)
            {
                ASSERT(ps_mv_refine_ctxt->i2_mv_cost[0][part_id] == MAX_SIGNED_16BIT_VAL);
                ASSERT(ps_mv_refine_ctxt->i2_mv_x[0][part_id] == 0);
                ASSERT(ps_mv_refine_ctxt->i2_mv_y[0][part_id] == 0);

                ps_mv_refine_ctxt->i2_ref_idx[0][part_id] = ps_search_node->i1_ref_idx;
            }
        }
    }
}

void hme_calc_sad_and_1_best_result_subpel(
    err_prms_t *ps_err_prms, result_upd_prms_t *ps_result_prms)
{
    S32 i4_candt;
    S32 i4_num_nodes;

    S32 *pi4_sad_grid = ps_err_prms->pi4_sad_grid;

    S32 cur_buf_stride = ps_err_prms->i4_inp_stride;
    WORD32 ref_buf_stride = ps_err_prms->i4_ref_stride;
    WORD32 cur_buf_stride_ls2 = (cur_buf_stride << 2);
    WORD32 ref_buf_stride_ls2 = (ref_buf_stride << 2);

    mv_refine_ctxt_t *ps_subpel_refine_ctxt;
    ps_subpel_refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
    i4_num_nodes = 1;

    /* Run through each of the candts in a loop */
    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES THE SAD AND UPDATES THE SAD GRID   */
        /**********************************************************************/
        {
            WORD32 b, c, d;
            UWORD8 *pu1_cur_ptr;
            UWORD8 *pu1_ref_ptr;
            UWORD16 au2_4x4_sad[NUM_4X4];

            pu1_cur_ptr = ps_err_prms->pu1_inp;
            pu1_ref_ptr = &ps_err_prms->pu1_ref[0];

            /* Loop to compute the SAD's */
            {
                memset(&au2_4x4_sad[0], 0, NUM_4X4 * sizeof(UWORD16));
                for(b = 0; b < NUM_4X4; b++)
                {
                    WORD32 t1 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * cur_buf_stride_ls2;
                    WORD32 t2 = (b % 4) * NUM_PIXELS_IN_ROW + (b >> 2) * ref_buf_stride_ls2;

                    for(c = 0; c < NUM_ROWS_IN_4X4; c++)
                    {
                        WORD32 z_cur = (cur_buf_stride)*c + t1;
                        WORD32 z_ref = (ref_buf_stride)*c + t2;
                        for(d = 0; d < NUM_PIXELS_IN_ROW; d++)
                        {
                            au2_4x4_sad[b] += (UWORD16)ABS((
                                ((S32)pu1_ref_ptr[(z_ref + d)]) - ((S32)pu1_cur_ptr[(z_cur + d)])));
                        }
                    }
                }

                pi4_sad_grid[PART_ID_NxN_TL] =
                    (au2_4x4_sad[0] + au2_4x4_sad[1] + au2_4x4_sad[4] + au2_4x4_sad[5]);
                pi4_sad_grid[PART_ID_NxN_TR] =
                    (au2_4x4_sad[2] + au2_4x4_sad[3] + au2_4x4_sad[6] + au2_4x4_sad[7]);
                pi4_sad_grid[PART_ID_NxN_BL] =
                    (au2_4x4_sad[8] + au2_4x4_sad[9] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_NxN_BR] =
                    (au2_4x4_sad[10] + au2_4x4_sad[11] + au2_4x4_sad[14] + au2_4x4_sad[15]);
                pi4_sad_grid[PART_ID_Nx2N_L] =
                    pi4_sad_grid[PART_ID_NxN_TL] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_Nx2N_R] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_BR];
                pi4_sad_grid[PART_ID_2NxN_T] =
                    pi4_sad_grid[PART_ID_NxN_TR] + pi4_sad_grid[PART_ID_NxN_TL];
                pi4_sad_grid[PART_ID_2NxN_B] =
                    pi4_sad_grid[PART_ID_NxN_BR] + pi4_sad_grid[PART_ID_NxN_BL];
                pi4_sad_grid[PART_ID_nLx2N_L] =
                    (au2_4x4_sad[8] + au2_4x4_sad[0] + au2_4x4_sad[12] + au2_4x4_sad[4]);
                pi4_sad_grid[PART_ID_nRx2N_R] =
                    (au2_4x4_sad[3] + au2_4x4_sad[7] + au2_4x4_sad[15] + au2_4x4_sad[11]);
                pi4_sad_grid[PART_ID_2NxnU_T] =
                    (au2_4x4_sad[1] + au2_4x4_sad[0] + au2_4x4_sad[2] + au2_4x4_sad[3]);
                pi4_sad_grid[PART_ID_2NxnD_B] =
                    (au2_4x4_sad[15] + au2_4x4_sad[14] + au2_4x4_sad[12] + au2_4x4_sad[13]);
                pi4_sad_grid[PART_ID_2Nx2N] =
                    pi4_sad_grid[PART_ID_2NxN_T] + pi4_sad_grid[PART_ID_2NxN_B];
                pi4_sad_grid[PART_ID_2NxnU_B] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnU_T];
                pi4_sad_grid[PART_ID_2NxnD_T] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_2NxnD_B];
                pi4_sad_grid[PART_ID_nRx2N_L] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nRx2N_R];
                pi4_sad_grid[PART_ID_nLx2N_R] =
                    pi4_sad_grid[PART_ID_2Nx2N] - pi4_sad_grid[PART_ID_nLx2N_L];
            }
        }
        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES UPDATES THE BEST RESULTS           */
        /**********************************************************************/
        {
            S32 i4_count = 0, i4_sad, i4_mv_cost, i4_tot_cost;
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

    {
        WORD32 i4_count = 0;
        for(i4_count = 0; i4_count < TOT_NUM_PARTS; i4_count++)
        {
            if(ps_subpel_refine_ctxt->i2_tot_cost[0][i4_count] >= MAX_SIGNED_16BIT_VAL)
            {
                ps_subpel_refine_ctxt->ai2_fullpel_satd[0][i4_count] = MAX_SIGNED_16BIT_VAL;
            }
        }
    }
}

/**
********************************************************************************
*  @fn     hme_calc_pt_sad_and_result_explicit(hme_search_prms_t *ps_search_prms,
*                                              wgt_pred_ctxt_t *ps_wt_inp_prms,
*                                              err_prms_t *ps_err_prms,
*                                              result_upd_prms_t *ps_result_prms,
*                                              U08 **ppu1_ref,
*                                              S32 i4_ref_stride)
*
*  @brief   Run thorugh the provided candidates and compute the point SAD and
*           cost and update the results in the order
*
*  @param[in]  ps_search_prms
*  @param[in]  ps_wt_inp_prms
*  @param[in]  ps_err_prms
*  @param[out] ps_result_prms
*  @param[in]  ppu1_ref
*  @param[in]  i4_ref_stride
*
*  @return   None
********************************************************************************
*/

void hme_calc_pt_sad_and_result_explicit(
    hme_search_prms_t *ps_search_prms,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    err_prms_t *ps_err_prms,
    result_upd_prms_t *ps_result_prms,
    U08 **ppu1_ref,
    S32 i4_ref_stride)
{
    WORD32 i4_grid_mask, i4_part_mask, i4_num_results, i4_candt, i4_num_nodes;
    WORD32 i4_inp_stride, i4_inp_off, i4_ref_offset;

    search_node_t *ps_search_node;
    BLK_SIZE_T e_blk_size;
    PF_SAD_FXN_T pf_sad_fxn;
    PF_RESULT_FXN_T pf_hme_result_fxn;

    i4_grid_mask = 0x1; /* Point SAD */

    /* Get the parameters required */
    i4_part_mask = ps_search_prms->i4_part_mask;
    e_blk_size = ps_search_prms->e_blk_size;
    i4_num_results = (S32)ps_search_prms->ps_search_results->u1_num_results_per_part;
    i4_num_nodes = ps_search_prms->i4_num_search_nodes;
    ps_search_node = ps_search_prms->ps_search_nodes;

    i4_inp_stride = ps_search_prms->i4_inp_stride;
    /* Move to the location of the search blk in inp buffer */
    i4_inp_off = ps_search_prms->i4_cu_x_off;
    i4_inp_off += ps_search_prms->i4_cu_y_off * i4_inp_stride;
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;

    pf_sad_fxn = hme_get_sad_fxn(e_blk_size, i4_grid_mask, i4_part_mask);
    /**********************************************************************/
    /* we have a sparsely populated SAD grid of size 9x17.                */
    /* the id of the results in the grid is shown                         */
    /*     5   2   6                                                      */
    /*     1   0   3                                                      */
    /*     7   4   8                                                      */
    /* The motivation for choosing a grid like this is that               */
    /* in case of no refinement, the central location is                  */
    /* the first entry in the grid                                        */
    /* Also for diamond, the 4 entries get considered first               */
    /* This is consistent with the diamond notation used in               */
    /* subpel refinement. To Check                                        */
    /* Update the results for the given search candt                      */
    /* returns the cost of the 2Nx2N partition                            */
    /**********************************************************************/

    /* Get the modified update result fun. with CLIP16 of cost to match   */
    /* with SIMD */
    pf_hme_result_fxn = hme_update_results_grid_pu_bestn_no_encode;

    for(i4_candt = 0; i4_candt < i4_num_nodes; i4_candt++)
    {
        if(ps_search_node->s_mv.i2_mvx == INTRA_MV)
            continue;

        /* initialize minimum cost for this candidate. As we search around */
        /* this candidate, this is used to check early exit, when in any   */
        /* given iteration, the center pt of the grid is lowest value      */
        ps_result_prms->i4_min_cost = MAX_32BIT_VAL;

        ps_err_prms->pu1_inp = ps_wt_inp_prms->apu1_wt_inp[ps_search_node->i1_ref_idx] + i4_inp_off;
        ps_err_prms->i4_grid_mask = i4_grid_mask;

        ps_err_prms->pu1_ref = ppu1_ref[ps_search_node->i1_ref_idx] + i4_ref_offset;
        ps_err_prms->pu1_ref += ps_search_node->s_mv.i2_mvx;
        ps_err_prms->pu1_ref += (ps_search_node->s_mv.i2_mvy * i4_ref_stride);

        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES THE SAD AND UPDATES THE SAD GRID   */
        /**********************************************************************/
        pf_sad_fxn(ps_err_prms);

        /**********************************************************************/
        /* CALL THE FUNCTION THAT COMPUTES UPDATES THE BEST RESULTS           */
        /**********************************************************************/
        ps_result_prms->i4_grid_mask = i4_grid_mask;
        ps_result_prms->ps_search_node_base = ps_search_node;
        pf_hme_result_fxn(ps_result_prms);

        ps_search_node++;
    }
}

/**
********************************************************************************
*  @fn     hme_set_mvp_node(search_results_t *ps_search_results,
*                           search_node_t *ps_candt_prj_coloc,
*                           S08 i1_ref_idx)
*
*  @brief   Set node used for motion vector predictor computation
*           Either TR or L is compared to projected colocated and
*           closest is decided as MVP
*
*  @param[in]  ps_search_results
*
*  @param[in]  ps_candt_prj_coloc
*
*  @param[in]  i1_ref_idx
*
*  @return   None
********************************************************************************
*/
void hme_set_mvp_node(
    search_results_t *ps_search_results,
    search_node_t *ps_candt_prj_coloc,
    U08 u1_pred_lx,
    U08 u1_default_ref_id)
{
    S32 i;
    pred_ctxt_t *ps_pred_ctxt = &ps_search_results->as_pred_ctxt[u1_pred_lx];
    pred_candt_nodes_t *ps_pred_nodes = ps_pred_ctxt->as_pred_nodes;
    search_node_t *ps_pred_node_a = NULL, *ps_pred_node_b = NULL;

    S32 inp_shift = 2;
    S32 pred_shift;
    S32 ref_bits;
    S32 mv_p_x, mv_p_y;
    S16 mvdx1, mvdx2, mvdy1, mvdy2;

    ref_bits = ps_pred_ctxt->ppu1_ref_bits_tlu[u1_pred_lx][u1_default_ref_id];

    /*************************************************************************/
    /* Priority to bottom left availability. Else we go to left. If both are */
    /* not available, then a remains null                                    */
    /*************************************************************************/
    if(ps_pred_nodes->ps_l->u1_is_avail)
    {
        ps_pred_node_a = ps_pred_nodes->ps_l;
    }

    if((!(ps_pred_ctxt->proj_used) && (ps_pred_nodes->ps_tr->u1_is_avail)))
    {
        ps_pred_node_b = ps_pred_nodes->ps_tr;
    }
    else
    {
        ps_pred_node_b = ps_pred_nodes->ps_coloc;
        ps_pred_node_b->s_mv = ps_pred_node_b->ps_mv[0];
    }

    if(ps_pred_node_a == NULL)
    {
        ps_pred_node_a = ps_pred_nodes->ps_coloc;
        ps_pred_node_a->s_mv = ps_pred_node_a->ps_mv[0];

        if(ps_pred_node_b == ps_pred_nodes->ps_coloc)
        {
            ps_pred_node_b = ps_pred_nodes->ps_zeromv;
            ps_pred_node_b->s_mv = ps_pred_node_b->ps_mv[0];
        }
    }

    if(ps_pred_node_a->i1_ref_idx != u1_default_ref_id)
    {
        SCALE_FOR_POC_DELTA(
            mv_p_x, mv_p_y, ps_pred_node_a, u1_default_ref_id, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_a->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_a->s_mv.i2_mvy;
    }
    pred_shift = ps_pred_node_a->u1_subpel_done ? 0 : 2;
    COMPUTE_MV_DIFFERENCE(mvdx1, mvdy1, ps_candt_prj_coloc, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx1 = ABS(mvdx1);
    mvdy1 = ABS(mvdy1);

    if(ps_pred_node_b->i1_ref_idx != u1_default_ref_id)
    {
        SCALE_FOR_POC_DELTA(
            mv_p_x, mv_p_y, ps_pred_node_b, u1_default_ref_id, ps_pred_ctxt->pi2_ref_scf);
    }
    else
    {
        mv_p_x = ps_pred_node_b->s_mv.i2_mvx;
        mv_p_y = ps_pred_node_b->s_mv.i2_mvy;
    }
    pred_shift = ps_pred_node_b->u1_subpel_done ? 0 : 2;
    COMPUTE_MV_DIFFERENCE(mvdx2, mvdy2, ps_candt_prj_coloc, mv_p_x, mv_p_y, inp_shift, pred_shift);
    mvdx2 = ABS(mvdx2);
    mvdy2 = ABS(mvdy2);

    if((mvdx1 + mvdy1) < (mvdx2 + mvdy2))
    {
        for(i = 0; i < TOT_NUM_PARTS; i++)
        {
            ps_pred_nodes[i].ps_mvp_node = ps_pred_node_a;
        }
    }
    else
    {
        for(i = 0; i < TOT_NUM_PARTS; i++)
        {
            ps_pred_nodes[i].ps_mvp_node = ps_pred_node_b;
        }
    }
}
