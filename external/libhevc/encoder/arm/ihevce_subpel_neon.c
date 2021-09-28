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
*  ihevce_subpel_neon.c
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
#include "hme_interface.h"
#include "hme_defs.h"

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
FT_CALC_SATD_AND_RESULT hme_evalsatd_update_1_best_result_pt_pu_16x16_neon;

WORD32 ihevce_had4_4x4_neon(
    UWORD8 *pu1_src,
    WORD32 src_strd,
    UWORD8 *pu1_pred,
    WORD32 pred_strd,
    WORD16 *pi2_dst4x4,
    WORD32 dst_strd,
    WORD32 *pi4_hsad,
    WORD32 hsad_stride,
    WORD32 i4_frm_qstep);

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

static void hme_4x4_qpel_interp_avg_neon(
    UWORD8 *pu1_src_a,
    UWORD8 *pu1_src_b,
    WORD32 src_a_strd,
    WORD32 src_b_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd)
{
    uint8x16_t src_a = load_unaligned_u8q(pu1_src_a, src_a_strd);
    uint8x16_t src_b = load_unaligned_u8q(pu1_src_b, src_b_strd);
    uint8x16_t dst = vrhaddq_u8(src_a, src_b);

    store_unaligned_u8q(pu1_dst, dst_strd, dst);
}

static void hme_8xn_qpel_interp_avg_neon(
    UWORD8 *pu1_src_a,
    UWORD8 *pu1_src_b,
    WORD32 src_a_strd,
    WORD32 src_b_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht)
{
    WORD32 i;

    for(i = 0; i < ht; i++)
    {
        uint8x8_t src_a = vld1_u8(pu1_src_a);
        uint8x8_t src_b = vld1_u8(pu1_src_b);
        uint8x8_t dst = vrhadd_u8(src_a, src_b);

        vst1_u8(pu1_dst, dst);
        pu1_src_a += src_a_strd;
        pu1_src_b += src_b_strd;
        pu1_dst += dst_strd;
    }
}

static void hme_16xn_qpel_interp_avg_neon(
    UWORD8 *pu1_src_a,
    UWORD8 *pu1_src_b,
    WORD32 src_a_strd,
    WORD32 src_b_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht)
{
    WORD32 i;

    for(i = 0; i < ht; i++)
    {
        uint8x16_t src_a = vld1q_u8(pu1_src_a);
        uint8x16_t src_b = vld1q_u8(pu1_src_b);
        uint8x16_t dst = vrhaddq_u8(src_a, src_b);

        vst1q_u8(pu1_dst, dst);
        pu1_src_a += src_a_strd;
        pu1_src_b += src_b_strd;
        pu1_dst += dst_strd;
    }
}

static void hme_32xn_qpel_interp_avg_neon(
    UWORD8 *pu1_src_a,
    UWORD8 *pu1_src_b,
    WORD32 src_a_strd,
    WORD32 src_b_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 ht)
{
    WORD32 i;

    for(i = 0; i < ht; i++)
    {
        uint8x16_t src_a_0 = vld1q_u8(pu1_src_a);
        uint8x16_t src_b_0 = vld1q_u8(pu1_src_b);
        uint8x16_t dst_0 = vrhaddq_u8(src_a_0, src_b_0);

        uint8x16_t src_a_1 = vld1q_u8(pu1_src_a + 16);
        uint8x16_t src_b_1 = vld1q_u8(pu1_src_b + 16);
        uint8x16_t dst_1 = vrhaddq_u8(src_a_1, src_b_1);

        vst1q_u8(pu1_dst, dst_0);
        vst1q_u8(pu1_dst + 16, dst_1);
        pu1_src_a += src_a_strd;
        pu1_src_b += src_b_strd;
        pu1_dst += dst_strd;
    }
}

static void hme_4mx4n_qpel_interp_avg_neon(
    UWORD8 *pu1_src_a,
    UWORD8 *pu1_src_b,
    WORD32 src_a_strd,
    WORD32 src_b_strd,
    UWORD8 *pu1_dst,
    WORD32 dst_strd,
    WORD32 blk_wd,
    WORD32 blk_ht)
{
    WORD32 i, j;

    assert(blk_wd % 4 == 0);
    assert(blk_ht % 4 == 0);

    for(i = 0; i < blk_ht; i += 4)
    {
        for(j = 0; j < blk_wd;)
        {
            WORD32 wd = blk_wd - j;

            if(wd >= 32)
            {
                hme_32xn_qpel_interp_avg_neon(
                    pu1_src_a + j, pu1_src_b + j, src_a_strd, src_b_strd, pu1_dst + j, dst_strd, 4);
                j += 32;
            }
            else if(wd >= 16)
            {
                hme_16xn_qpel_interp_avg_neon(
                    pu1_src_a + j, pu1_src_b + j, src_a_strd, src_b_strd, pu1_dst + j, dst_strd, 4);
                j += 16;
            }
            else if(wd >= 8)
            {
                hme_8xn_qpel_interp_avg_neon(
                    pu1_src_a + j, pu1_src_b + j, src_a_strd, src_b_strd, pu1_dst + j, dst_strd, 4);
                j += 8;
            }
            else
            {
                hme_4x4_qpel_interp_avg_neon(
                    pu1_src_a + j, pu1_src_b + j, src_a_strd, src_b_strd, pu1_dst + j, dst_strd);
                j += 4;
            }
        }
        pu1_src_a += (4 * src_a_strd);
        pu1_src_b += (4 * src_b_strd);
        pu1_dst += (4 * dst_strd);
    }
}

void hme_qpel_interp_avg_neon(interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, S32 i4_buf_id)
{
    U08 *pu1_src1, *pu1_src2, *pu1_dst;
    qpel_input_buf_cfg_t *ps_inp_cfg;
    S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
    S32 i4_ref_stride = ps_prms->i4_ref_stride;

    i4_mv_x_frac = i4_mv_x & 3;
    i4_mv_y_frac = i4_mv_y & 3;

    i4_offset = (i4_mv_x >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

    /* Derive the descriptor that has all offset and size info */
    ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

    if(ps_inp_cfg->i1_buf_id1 == ps_inp_cfg->i1_buf_id2)
    {
        /* This is case for fxfy/hxfy/fxhy/hxhy */
        ps_prms->pu1_final_out = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
        ps_prms->pu1_final_out += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
        ps_prms->pu1_final_out += (ps_inp_cfg->i1_buf_yoff1 * ps_prms->i4_ref_stride);
        ps_prms->i4_final_out_stride = i4_ref_stride;

        return;
    }

    pu1_src1 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
    pu1_src1 += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
    pu1_src1 += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);

    pu1_src2 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id2];
    pu1_src2 += ps_inp_cfg->i1_buf_xoff2 + i4_offset;
    pu1_src2 += (ps_inp_cfg->i1_buf_yoff2 * i4_ref_stride);

    pu1_dst = ps_prms->apu1_interp_out[i4_buf_id];

    hme_4mx4n_qpel_interp_avg_neon(
        pu1_src1,
        pu1_src2,
        ps_prms->i4_ref_stride,
        ps_prms->i4_ref_stride,
        pu1_dst,
        ps_prms->i4_out_stride,
        ps_prms->i4_blk_wd,
        ps_prms->i4_blk_ht);
    ps_prms->pu1_final_out = pu1_dst;
    ps_prms->i4_final_out_stride = ps_prms->i4_out_stride;
}

// TODO: Can this function and above function be unified
void hme_qpel_interp_avg_1pt_neon(
    interp_prms_t *ps_prms,
    S32 i4_mv_x,
    S32 i4_mv_y,
    S32 i4_buf_id,
    U08 **ppu1_final,
    S32 *pi4_final_stride)
{
    U08 *pu1_src1, *pu1_src2, *pu1_dst;
    qpel_input_buf_cfg_t *ps_inp_cfg;
    S32 i4_mv_x_frac, i4_mv_y_frac, i4_offset;
    S32 i4_ref_stride = ps_prms->i4_ref_stride;

    i4_mv_x_frac = i4_mv_x & 3;
    i4_mv_y_frac = i4_mv_y & 3;

    i4_offset = (i4_mv_x >> 2) + (i4_mv_y >> 2) * i4_ref_stride;

    /* Derive the descriptor that has all offset and size info */
    ps_inp_cfg = &gas_qpel_inp_buf_cfg[i4_mv_y_frac][i4_mv_x_frac];

    pu1_src1 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id1];
    pu1_src1 += ps_inp_cfg->i1_buf_xoff1 + i4_offset;
    pu1_src1 += (ps_inp_cfg->i1_buf_yoff1 * i4_ref_stride);

    pu1_src2 = ps_prms->ppu1_ref[ps_inp_cfg->i1_buf_id2];
    pu1_src2 += ps_inp_cfg->i1_buf_xoff2 + i4_offset;
    pu1_src2 += (ps_inp_cfg->i1_buf_yoff2 * i4_ref_stride);

    pu1_dst = ps_prms->apu1_interp_out[i4_buf_id];

    hme_4mx4n_qpel_interp_avg_neon(
        pu1_src1,
        pu1_src2,
        ps_prms->i4_ref_stride,
        ps_prms->i4_ref_stride,
        pu1_dst,
        ps_prms->i4_out_stride,
        ps_prms->i4_blk_wd,
        ps_prms->i4_blk_ht);
    ppu1_final[i4_buf_id] = pu1_dst;
    pi4_final_stride[i4_buf_id] = ps_prms->i4_out_stride;
}

void hme_qpel_interp_avg_2pt_vert_with_reuse_neon(
    interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, U08 **ppu1_final, S32 *pi4_final_stride)
{
    hme_qpel_interp_avg_1pt_neon(ps_prms, i4_mv_x, i4_mv_y + 1, 3, ppu1_final, pi4_final_stride);

    hme_qpel_interp_avg_1pt_neon(ps_prms, i4_mv_x, i4_mv_y - 1, 1, ppu1_final, pi4_final_stride);
}

void hme_qpel_interp_avg_2pt_horz_with_reuse_neon(
    interp_prms_t *ps_prms, S32 i4_mv_x, S32 i4_mv_y, U08 **ppu1_final, S32 *pi4_final_stride)
{
    hme_qpel_interp_avg_1pt_neon(ps_prms, i4_mv_x + 1, i4_mv_y, 2, ppu1_final, pi4_final_stride);

    hme_qpel_interp_avg_1pt_neon(ps_prms, i4_mv_x - 1, i4_mv_y, 0, ppu1_final, pi4_final_stride);
}

void hme_evalsatd_update_1_best_result_pt_pu_16x16_neon(
    err_prms_t *ps_prms, result_upd_prms_t *ps_result_prms)
{
    mv_refine_ctxt_t *refine_ctxt = ps_result_prms->ps_subpel_refine_ctxt;
    S32 *pi4_sad_grid = ps_prms->pi4_sad_grid;
    S32 *pi4_valid_part_ids = &refine_ctxt->ai4_part_id[0];

    S32 ai4_satd_4x4[16];
    S32 ai4_satd_8x8[4];

    U08 *pu1_inp = ps_prms->pu1_inp;
    U08 *pu1_ref = ps_prms->pu1_ref;

    S32 inp_stride = ps_prms->i4_inp_stride;
    S32 ref_stride = ps_prms->i4_ref_stride;

    S32 i;

    /* Call recursive 16x16 HAD module; updates satds for 4x4, 8x8 and 16x16 */
    for(i = 0; i < 4; i++)
    {
        U08 *pu1_src = pu1_inp + (i & 0x1) * 8 + (i >> 1) * inp_stride * 8;
        U08 *pu1_pred = pu1_ref + (i & 0x1) * 8 + (i >> 1) * ref_stride * 8;
        S16 idx = (i & 0x1) * 2 + (i >> 1) * 8;

        ai4_satd_8x8[i] = ihevce_had4_4x4_neon(
            pu1_src, inp_stride, pu1_pred, ref_stride, NULL, 0, &ai4_satd_4x4[idx], 4, 0);
    }

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
}
