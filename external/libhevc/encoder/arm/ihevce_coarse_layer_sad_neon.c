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
*******************************************************************************
* @file
*  ihevce_coarse_layer_sad_neon.c
*
* @brief
*  Contains intrinsic definitions of functions for computing sad
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
#include "hme_globals.h"

#include "ihevce_me_instr_set_router.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

void hme_store_4x4_sads_high_speed_neon(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    range_prms_t *ps_mv_limit,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S16 *pi2_sads_4x4)
{
    uint8x8_t src2[4];
    uint8x16_t src;

    S32 i, j;

    /* Input and reference attributes */
    U08 *pu1_inp, *pu1_ref;
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref, *pu1_ref_coloc;

    S32 stepy, stepx, step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    /* Points to the range limits for mv */
    range_prms_t *ps_range_prms = ps_search_prms->aps_mv_range[0];

    /* Reference index to be searched */
    S32 i4_search_idx = ps_search_prms->i1_ref_idx;

    pu1_inp = ps_wt_inp_prms->apu1_wt_inp[i4_search_idx];
    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    pu1_inp += ps_search_prms->i4_cu_x_off;
    pu1_inp += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* we use either input of previously encoded pictures as reference       */
    /* in coarse layer                                                       */
    /*************************************************************************/
    i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
    ppu1_ref = ps_layer_ctxt->ppu1_list_inp;

    /* colocated position in reference picture */
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    pu1_ref_coloc = ppu1_ref[i4_search_idx] + i4_ref_offset;

    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_SPEED */
    step_shift_x = step_shift_y = 2;

    mv_x_offset = -(ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = -(ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    ASSERT(4 == stepx);

    /* load input */
    {
        S32 mv_x_sweep = ps_range_prms->i2_max_x - ps_range_prms->i2_min_x;
        uint32x2_t a[4];

        for(i = 0; i < 4; i++)
        {
            a[i] = vld1_dup_u32((uint32_t *)pu1_inp);
            pu1_inp += i4_inp_stride;
        }
        src2[0] = vreinterpret_u8_u32(a[0]);
        src2[1] = vreinterpret_u8_u32(a[1]);
        src2[2] = vreinterpret_u8_u32(a[2]);
        src2[3] = vreinterpret_u8_u32(a[3]);

        if((mv_x_sweep >> step_shift_x) & 1)
        {
            uint32x2x2_t l = vtrn_u32(a[0], a[1]);
            uint32x2x2_t m = vtrn_u32(a[2], a[3]);

            src = vcombine_u8(vreinterpret_u8_u32(l.val[0]), vreinterpret_u8_u32(m.val[0]));
        }
    }

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_range_prms->i2_min_y; mvy < ps_range_prms->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_range_prms->i2_min_x; mvx < ps_range_prms->i2_max_x;)
        {
            U16 *pu2_sad = (U16 *)&pi2_sads_4x4
                [((mvx >> step_shift_x) + mv_x_offset) +
                 ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range];

            pu1_ref = pu1_ref_coloc + mvx + (mvy * i4_ref_stride);
            if((mvx + (stepx * 4)) <= ps_range_prms->i2_max_x)  // 16x4
            {
                uint16x8_t abs_01 = vdupq_n_u16(0);
                uint16x8_t abs_23 = vdupq_n_u16(0);
                uint16x4_t tmp_a0, tmp_a1;

                for(j = 0; j < 4; j++)
                {
                    uint8x16_t ref = vld1q_u8(pu1_ref);

                    abs_01 = vabal_u8(abs_01, src2[j], vget_low_u8(ref));
                    abs_23 = vabal_u8(abs_23, src2[j], vget_high_u8(ref));
                    pu1_ref += i4_ref_stride;
                }
                tmp_a0 = vpadd_u16(vget_low_u16(abs_01), vget_high_u16(abs_01));
                tmp_a1 = vpadd_u16(vget_low_u16(abs_23), vget_high_u16(abs_23));
                abs_01 = vcombine_u16(tmp_a0, tmp_a1);
                tmp_a0 = vpadd_u16(vget_low_u16(abs_01), vget_high_u16(abs_01));
                vst1_u16(pu2_sad, tmp_a0);
                mvx += stepx * 4;
            }
            else if((mvx + (stepx * 2)) <= ps_range_prms->i2_max_x)  // 8x4
            {
                uint16x8_t abs_01 = vdupq_n_u16(0);
                uint16x4_t tmp_a;
                uint32x2_t tmp_b;

                for(j = 0; j < 4; j++)
                {
                    uint8x8_t ref = vld1_u8(pu1_ref);

                    abs_01 = vabal_u8(abs_01, src2[j], ref);
                    pu1_ref += i4_ref_stride;
                }
                tmp_a = vpadd_u16(vget_low_u16(abs_01), vget_high_u16(abs_01));
                tmp_b = vpaddl_u16(tmp_a);
                pu2_sad[0] = vget_lane_u32(tmp_b, 0);
                pu2_sad[1] = vget_lane_u32(tmp_b, 1);
                mvx += stepx * 2;
            }
            else if((mvx + stepx) <= ps_range_prms->i2_max_x)  // 4x4
            {
                const uint8x16_t ref = load_unaligned_u8q(pu1_ref, i4_ref_stride);
                uint16x8_t abs = vabdl_u8(vget_low_u8(src), vget_low_u8(ref));
                uint32x4_t b;
                uint64x2_t c;

                abs = vabal_u8(abs, vget_high_u8(src), vget_high_u8(ref));
                b = vpaddlq_u16(abs);
                c = vpaddlq_u32(b);
                *pu2_sad = vget_lane_u32(
                    vadd_u32(
                        vreinterpret_u32_u64(vget_low_u64(c)),
                        vreinterpret_u32_u64(vget_high_u64(c))),
                    0);
                mvx += stepx;
            }
        }
    }
}

void hme_store_4x4_sads_high_quality_neon(
    hme_search_prms_t *ps_search_prms,
    layer_ctxt_t *ps_layer_ctxt,
    range_prms_t *ps_mv_limit,
    wgt_pred_ctxt_t *ps_wt_inp_prms,
    S16 *pi2_sads_4x4)
{
    uint8x8_t src2[4];
    uint8x16_t src;

    S32 i, j;

    /* Input and reference attributes */
    U08 *pu1_inp, *pu1_ref;
    S32 i4_inp_stride, i4_ref_stride, i4_ref_offset;

    /* The reference is actually an array of ptrs since there are several    */
    /* reference id. So an array gets passed form calling function           */
    U08 **ppu1_ref, *pu1_ref_coloc;

    S32 stepy, stepx, step_shift_x, step_shift_y;
    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    /* Points to the range limits for mv */
    range_prms_t *ps_range_prms = ps_search_prms->aps_mv_range[0];

    /* Reference index to be searched */
    S32 i4_search_idx = ps_search_prms->i1_ref_idx;

    pu1_inp = ps_wt_inp_prms->apu1_wt_inp[i4_search_idx];
    i4_inp_stride = ps_search_prms->i4_inp_stride;

    /* Move to the location of the search blk in inp buffer */
    pu1_inp += ps_search_prms->i4_cu_x_off;
    pu1_inp += ps_search_prms->i4_cu_y_off * i4_inp_stride;

    /*************************************************************************/
    /* we use either input of previously encoded pictures as reference       */
    /* in coarse layer                                                       */
    /*************************************************************************/
    i4_ref_stride = ps_layer_ctxt->i4_inp_stride;
    ppu1_ref = ps_layer_ctxt->ppu1_list_inp;

    /* colocated position in reference picture */
    i4_ref_offset = (i4_ref_stride * ps_search_prms->i4_y_off) + ps_search_prms->i4_x_off;
    pu1_ref_coloc = ppu1_ref[i4_search_idx] + i4_ref_offset;

    stepx = stepy = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_QUALITY */
    step_shift_x = step_shift_y = 1;

    mv_x_offset = -(ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = -(ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    /* load input */
    {
        S32 mv_x_sweep = ps_range_prms->i2_max_x - ps_range_prms->i2_min_x;
        uint32x2_t a[4];

        for(i = 0; i < 4; i++)
        {
            a[i] = vld1_dup_u32((uint32_t *)pu1_inp);
            pu1_inp += i4_inp_stride;
        }
        src2[0] = vreinterpret_u8_u32(a[0]);
        src2[1] = vreinterpret_u8_u32(a[1]);
        src2[2] = vreinterpret_u8_u32(a[2]);
        src2[3] = vreinterpret_u8_u32(a[3]);

        if((mv_x_sweep >> 2) & 1)
        {
            uint32x2x2_t l = vtrn_u32(a[0], a[1]);
            uint32x2x2_t m = vtrn_u32(a[2], a[3]);

            src = vcombine_u8(vreinterpret_u8_u32(l.val[0]), vreinterpret_u8_u32(m.val[0]));
        }
    }

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_range_prms->i2_min_y; mvy < ps_range_prms->i2_max_y; mvy += stepy)
    {
        for(mvx = ps_range_prms->i2_min_x; mvx < ps_range_prms->i2_max_x;)
        {
            U16 *pu2_sad = (U16 *)&pi2_sads_4x4
                [((mvx >> step_shift_x) + mv_x_offset) +
                 ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range];

            pu1_ref = pu1_ref_coloc + mvx + (mvy * i4_ref_stride);
            if((mvx + (stepx * 8)) <= ps_range_prms->i2_max_x)  // 16x4
            {
                uint16x8_t abs_a_01 = vdupq_n_u16(0);
                uint16x8_t abs_a_23 = vdupq_n_u16(0);
                uint16x8_t abs_b_01 = vdupq_n_u16(0);
                uint16x8_t abs_b_23 = vdupq_n_u16(0);
                uint16x4_t tmp_b0, tmp_b1;
                uint16x4x2_t tmp_a;

                for(j = 0; j < 4; j++)
                {
                    uint8x16_t ref_a = vld1q_u8(pu1_ref);
                    uint8x16_t ref_b = vld1q_u8(pu1_ref + 2);

                    abs_a_01 = vabal_u8(abs_a_01, src2[j], vget_low_u8(ref_a));
                    abs_a_23 = vabal_u8(abs_a_23, src2[j], vget_high_u8(ref_a));
                    abs_b_01 = vabal_u8(abs_b_01, src2[j], vget_low_u8(ref_b));
                    abs_b_23 = vabal_u8(abs_b_23, src2[j], vget_high_u8(ref_b));
                    pu1_ref += i4_ref_stride;
                }
                tmp_a.val[0] = vpadd_u16(vget_low_u16(abs_a_01), vget_high_u16(abs_a_01));
                tmp_a.val[1] = vpadd_u16(vget_low_u16(abs_a_23), vget_high_u16(abs_a_23));
                abs_a_01 = vcombine_u16(tmp_a.val[0], tmp_a.val[1]);
                tmp_a.val[0] = vpadd_u16(vget_low_u16(abs_a_01), vget_high_u16(abs_a_01));
                tmp_b0 = vpadd_u16(vget_low_u16(abs_b_01), vget_high_u16(abs_b_01));
                tmp_b1 = vpadd_u16(vget_low_u16(abs_b_23), vget_high_u16(abs_b_23));
                abs_b_01 = vcombine_u16(tmp_b0, tmp_b1);
                tmp_a.val[1] = vpadd_u16(vget_low_u16(abs_b_01), vget_high_u16(abs_b_01));
                vst2_u16(pu2_sad, tmp_a);
                mvx += stepx * 8;
            }
            else if((mvx + (stepx * 4)) <= ps_range_prms->i2_max_x)  // 8x4
            {
                uint16x8_t abs_a_01 = vdupq_n_u16(0);
                uint16x8_t abs_b_01 = vdupq_n_u16(0);
                uint16x4_t tmp_a, tmp_b;

                for(j = 0; j < 4; j++)
                {
                    uint8x8_t ref_a = vld1_u8(pu1_ref);
                    uint8x8_t ref_b = vld1_u8(pu1_ref + 2);

                    abs_a_01 = vabal_u8(abs_a_01, src2[j], ref_a);
                    abs_b_01 = vabal_u8(abs_b_01, src2[j], ref_b);
                    pu1_ref += i4_ref_stride;
                }
                tmp_a = vpadd_u16(vget_low_u16(abs_a_01), vget_high_u16(abs_a_01));
                tmp_b = vpadd_u16(vget_low_u16(abs_b_01), vget_high_u16(abs_b_01));
                tmp_a = vpadd_u16(tmp_a, tmp_b);

                pu2_sad[0] = vget_lane_u16(tmp_a, 0);
                pu2_sad[1] = vget_lane_u16(tmp_a, 2);
                pu2_sad[2] = vget_lane_u16(tmp_a, 1);
                pu2_sad[3] = vget_lane_u16(tmp_a, 3);
                mvx += stepx * 4;
            }
            else if((mvx + stepx * 2) <= ps_range_prms->i2_max_x)  // 4x4
            {
                uint8x16_t ref = load_unaligned_u8q(pu1_ref, i4_ref_stride);
                uint16x8_t abs = vabdl_u8(vget_low_u8(src), vget_low_u8(ref));
                uint32x4_t b;
                uint64x2_t c;

                abs = vabal_u8(abs, vget_high_u8(src), vget_high_u8(ref));
                b = vpaddlq_u16(abs);
                c = vpaddlq_u32(b);
                *pu2_sad = vget_lane_u32(
                    vadd_u32(
                        vreinterpret_u32_u64(vget_low_u64(c)),
                        vreinterpret_u32_u64(vget_high_u64(c))),
                    0);

                ref = load_unaligned_u8q(pu1_ref + 2, i4_ref_stride);
                abs = vabdl_u8(vget_low_u8(src), vget_low_u8(ref));
                abs = vabal_u8(abs, vget_high_u8(src), vget_high_u8(ref));
                b = vpaddlq_u16(abs);
                c = vpaddlq_u32(b);
                pu2_sad[1] = vget_lane_u32(
                    vadd_u32(
                        vreinterpret_u32_u64(vget_low_u64(c)),
                        vreinterpret_u32_u64(vget_high_u64(c))),
                    0);
                mvx += stepx * 2;
            }
            else
            {
                assert(0);
            }
        }
    }
}

#define BEST_COST(i)                                                                               \
    if(sad_array[0][i] < min_cost_4x8)                                                             \
    {                                                                                              \
        best_mv_x_4x8 = mvx + i * stepx;                                                           \
        best_mv_y_4x8 = mvy;                                                                       \
        min_cost_4x8 = sad_array[0][i];                                                            \
    }                                                                                              \
    if(sad_array[1][i] < min_cost_8x4)                                                             \
    {                                                                                              \
        best_mv_x_8x4 = mvx + i * stepx;                                                           \
        best_mv_y_8x4 = mvy;                                                                       \
        min_cost_8x4 = sad_array[1][i];                                                            \
    }

void hme_combine_4x4_sads_and_compute_cost_high_speed_neon(
    S08 i1_ref_idx,
    range_prms_t *ps_mv_range,
    range_prms_t *ps_mv_limit,
    hme_mv_t *ps_best_mv_4x8,
    hme_mv_t *ps_best_mv_8x4,
    pred_ctxt_t *ps_pred_ctxt,
    PF_MV_COST_FXN pf_mv_cost_compute,
    S16 *pi2_sads_4x4_current,
    S16 *pi2_sads_4x4_east,
    S16 *pi2_sads_4x4_south)
{
    S32 best_mv_y_4x8, best_mv_x_4x8, best_mv_y_8x4, best_mv_x_8x4;

    S32 stepy = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    S32 stepx = HME_COARSE_STEP_SIZE_HIGH_SPEED;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_SPEED */
    S32 step_shift_x = 2;
    S32 step_shift_y = 2;

    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    S32 lambda = ps_pred_ctxt->lambda;
    S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
    S32 rnd = 1 << (lambda_q_shift - 1);

    S32 min_cost_4x8 = MAX_32BIT_VAL;
    S32 min_cost_8x4 = MAX_32BIT_VAL;

    S32 i;

    const uint16x8_t v_ref_idx = vdupq_n_u16(i1_ref_idx);
    const uint32x4_t v_lambda = vdupq_n_u32(lambda);
    const uint32x4_t v_rnd_factor = vdupq_n_u32(rnd);
    const int32x4_t v_lambda_q_shift = vdupq_n_s32(-lambda_q_shift);

    mv_x_offset = (-ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = (-ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    ASSERT(MAX_MVX_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_x));
    ASSERT(MAX_MVY_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_y));

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_mv_range->i2_min_y; mvy < ps_mv_range->i2_max_y; mvy += stepy)
    {
        /* LUT: (2 * hme_get_range(mv_y) - 1) + ((!mv_y) ? 0 : 1) */
        uint16x8_t mvy_wt = vld1q_u16((U16 *)&gi2_mvy_range[ABS(mvy)][0]);

        /* mvy wt + ref_idx */
        mvy_wt = vaddq_u16(mvy_wt, v_ref_idx);

        for(mvx = ps_mv_range->i2_min_x; mvx < ps_mv_range->i2_max_x;)
        {
            S32 sad_pos = ((mvx >> step_shift_x) + mv_x_offset) +
                          ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range;

            if(mvx + (8 * stepx) <= ps_mv_range->i2_max_x)  // 8x4
            {
                uint16x8_t curr = vld1q_u16((U16 *)pi2_sads_4x4_current + sad_pos);
                uint16x8_t south = vld1q_u16((U16 *)pi2_sads_4x4_south + sad_pos);
                uint16x8_t east = vld1q_u16((U16 *)pi2_sads_4x4_east + sad_pos);
                uint16x8_t sad_4x8 = vaddq_u16(curr, south);
                uint16x8_t sad_8x4 = vaddq_u16(curr, east);
                /* LUT: (2 * hme_get_range(mv_x) - 1) + ((!mv_x) ? 0 : 1) */
                uint16x8_t mv_wt =
                    vld1q_u16((U16 *)&gi2_mvx_range[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0]);
                uint32x4_t total_cost_0, total_cost_1;
                uint16x8_t total_cost;
                U16 sad_array[2][8];

                /* mv weight + ref_idx */
                mv_wt = vaddq_u16(mv_wt, mvy_wt);

                total_cost_0 = vmulq_u32(v_lambda, vmovl_u16(vget_low_u16(mv_wt)));
                total_cost_1 = vmulq_u32(v_lambda, vmovl_u16(vget_high_u16(mv_wt)));

                total_cost_0 = vaddq_u32(total_cost_0, v_rnd_factor);
                total_cost_1 = vaddq_u32(total_cost_1, v_rnd_factor);

                total_cost_0 = vshlq_u32(total_cost_0, v_lambda_q_shift);
                total_cost_1 = vshlq_u32(total_cost_1, v_lambda_q_shift);

                total_cost = vcombine_u16(vmovn_u32(total_cost_0), vmovn_u32(total_cost_1));

                sad_4x8 = vaddq_u16(total_cost, sad_4x8);
                sad_8x4 = vaddq_u16(total_cost, sad_8x4);

                vst1q_u16(sad_array[0], sad_4x8);
                vst1q_u16(sad_array[1], sad_8x4);

                for(i = 0; i < 8; i++)
                {
                    BEST_COST(i);
                }
                mvx += stepx * 8;
            }
            else if(mvx + (4 * stepx) <= ps_mv_range->i2_max_x)  // 4x4
            {
                uint16x4_t curr = vld1_u16((U16 *)pi2_sads_4x4_current + sad_pos);
                uint16x4_t south = vld1_u16((U16 *)pi2_sads_4x4_south + sad_pos);
                uint16x4_t east = vld1_u16((U16 *)pi2_sads_4x4_east + sad_pos);
                uint16x4_t sad_4x8 = vadd_u16(curr, south);
                uint16x4_t sad_8x4 = vadd_u16(curr, east);
                /* LUT: (2 * hme_get_range(mv_x) - 1) + ((!mv_x) ? 0 : 1) */
                uint16x4_t mv_wt =
                    vld1_u16((U16 *)&gi2_mvx_range[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0]);
                uint32x4_t total_cost;
                U16 sad_array[2][4];

                /* mv weight + ref_idx */
                mv_wt = vadd_u16(mv_wt, vget_low_u16(mvy_wt));

                total_cost = vmulq_u32(v_lambda, vmovl_u16(mv_wt));
                total_cost = vaddq_u32(total_cost, v_rnd_factor);
                total_cost = vshlq_u32(total_cost, v_lambda_q_shift);

                sad_4x8 = vadd_u16(vmovn_u32(total_cost), sad_4x8);
                sad_8x4 = vadd_u16(vmovn_u32(total_cost), sad_8x4);

                vst1_u16(sad_array[0], sad_4x8);
                vst1_u16(sad_array[1], sad_8x4);

                for(i = 0; i < 4; i++)
                {
                    BEST_COST(i);
                }

                mvx += stepx * 4;
            }
            else
            {
                S16 sad_array[2][1];
                S32 mv_cost;

                /* Get SAD by adding SAD for current and neighbour S  */
                sad_array[0][0] = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_south[sad_pos];
                sad_array[1][0] = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_east[sad_pos];

                mv_cost = gi2_mvy_range[ABS(mvy)][0] +
                          gi2_mvx_range[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0] + i1_ref_idx;

                mv_cost = (mv_cost * lambda + rnd) >> lambda_q_shift;

                sad_array[0][0] += mv_cost;
                sad_array[1][0] += mv_cost;

                BEST_COST(0);
                mvx += stepx;
            }
        }
    }

    ps_best_mv_4x8->i2_mv_x = best_mv_x_4x8;
    ps_best_mv_4x8->i2_mv_y = best_mv_y_4x8;

    ps_best_mv_8x4->i2_mv_x = best_mv_x_8x4;
    ps_best_mv_8x4->i2_mv_y = best_mv_y_8x4;
}

void hme_combine_4x4_sads_and_compute_cost_high_quality_neon(
    S08 i1_ref_idx,
    range_prms_t *ps_mv_range,
    range_prms_t *ps_mv_limit,
    hme_mv_t *ps_best_mv_4x8,
    hme_mv_t *ps_best_mv_8x4,
    pred_ctxt_t *ps_pred_ctxt,
    PF_MV_COST_FXN pf_mv_cost_compute,
    S16 *pi2_sads_4x4_current,
    S16 *pi2_sads_4x4_east,
    S16 *pi2_sads_4x4_south)
{
    S32 best_mv_y_4x8, best_mv_x_4x8, best_mv_y_8x4, best_mv_x_8x4;

    S32 stepy = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    S32 stepx = HME_COARSE_STEP_SIZE_HIGH_QUALITY;
    /*TODO: Calculate Step shift from the #define HME_COARSE_STEP_SIZE_HIGH_SPEED */
    S32 step_shift_x = 1;
    S32 step_shift_y = 1;

    S32 mvx, mvy, mv_x_offset, mv_y_offset, mv_x_range, mv_y_range;

    S32 lambda = ps_pred_ctxt->lambda;
    S32 lambda_q_shift = ps_pred_ctxt->lambda_q_shift;
    S32 rnd = 1 << (lambda_q_shift - 1);

    S32 min_cost_4x8 = MAX_32BIT_VAL;
    S32 min_cost_8x4 = MAX_32BIT_VAL;

    S32 i;

    const uint16x8_t v_ref_idx = vdupq_n_u16(i1_ref_idx);
    const uint32x4_t v_lambda = vdupq_n_u32(lambda);
    const uint32x4_t v_rnd_factor = vdupq_n_u32(rnd);
    const int32x4_t v_lambda_q_shift = vdupq_n_s32(-lambda_q_shift);

    mv_x_offset = (-ps_mv_limit->i2_min_x >> step_shift_x);
    mv_y_offset = (-ps_mv_limit->i2_min_y >> step_shift_y);
    mv_x_range = (-ps_mv_limit->i2_min_x + ps_mv_limit->i2_max_x) >> step_shift_x;
    mv_y_range = (-ps_mv_limit->i2_min_y + ps_mv_limit->i2_max_y) >> step_shift_y;

    ASSERT(MAX_MVX_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_x));
    ASSERT(MAX_MVY_SUPPORTED_IN_COARSE_LAYER >= ABS(ps_mv_range->i2_max_y));

    /* Run 2loops to sweep over the reference area */
    for(mvy = ps_mv_range->i2_min_y; mvy < ps_mv_range->i2_max_y; mvy += stepy)
    {
        /* LUT: (2 * hme_get_range(mv_y) - 1) + ((!mv_y) ? 0 : 1) */
        uint16x8_t mvy_wt = vld1q_u16((U16 *)&gi2_mvy_range[ABS(mvy)][0]);

        /* mvy wt + ref_idx */
        mvy_wt = vaddq_u16(mvy_wt, v_ref_idx);

        for(mvx = ps_mv_range->i2_min_x; mvx < ps_mv_range->i2_max_x;)
        {
            S32 sad_pos = ((mvx >> step_shift_x) + mv_x_offset) +
                          ((mvy >> step_shift_y) + mv_y_offset) * mv_x_range;

            if(mvx + (8 * stepx) <= ps_mv_range->i2_max_x)  // 8x4
            {
                uint16x8_t curr = vld1q_u16((U16 *)pi2_sads_4x4_current + sad_pos);
                uint16x8_t south = vld1q_u16((U16 *)pi2_sads_4x4_south + sad_pos);
                uint16x8_t east = vld1q_u16((U16 *)pi2_sads_4x4_east + sad_pos);
                uint16x8_t sad_4x8 = vaddq_u16(curr, south);
                uint16x8_t sad_8x4 = vaddq_u16(curr, east);
                /* LUT: (2 * hme_get_range(mv_x) - 1) + ((!mv_x) ? 0 : 1) */
                uint16x8_t mv_wt = vld1q_u16(
                    (U16 *)&gi2_mvx_range_high_quality[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0]);
                uint32x4_t total_cost_0, total_cost_1;
                uint16x8_t total_cost;
                U16 sad_array[2][8];

                /* mv weight + ref_idx */
                mv_wt = vaddq_u16(mv_wt, mvy_wt);

                total_cost_0 = vmulq_u32(v_lambda, vmovl_u16(vget_low_u16(mv_wt)));
                total_cost_1 = vmulq_u32(v_lambda, vmovl_u16(vget_high_u16(mv_wt)));

                total_cost_0 = vaddq_u32(total_cost_0, v_rnd_factor);
                total_cost_1 = vaddq_u32(total_cost_1, v_rnd_factor);

                total_cost_0 = vshlq_u32(total_cost_0, v_lambda_q_shift);
                total_cost_1 = vshlq_u32(total_cost_1, v_lambda_q_shift);

                total_cost = vcombine_u16(vmovn_u32(total_cost_0), vmovn_u32(total_cost_1));

                sad_4x8 = vaddq_u16(total_cost, sad_4x8);
                sad_8x4 = vaddq_u16(total_cost, sad_8x4);

                vst1q_u16(sad_array[0], sad_4x8);
                vst1q_u16(sad_array[1], sad_8x4);

                for(i = 0; i < 8; i++)
                {
                    BEST_COST(i);
                }
                mvx += stepx * 8;
            }
            else if(mvx + (4 * stepx) <= ps_mv_range->i2_max_x)  // 4x4
            {
                uint16x4_t curr = vld1_u16((U16 *)pi2_sads_4x4_current + sad_pos);
                uint16x4_t south = vld1_u16((U16 *)pi2_sads_4x4_south + sad_pos);
                uint16x4_t east = vld1_u16((U16 *)pi2_sads_4x4_east + sad_pos);
                uint16x4_t sad_4x8 = vadd_u16(curr, south);
                uint16x4_t sad_8x4 = vadd_u16(curr, east);
                /* LUT: (2 * hme_get_range(mv_x) - 1) + ((!mv_x) ? 0 : 1) */
                uint16x4_t mv_wt = vld1_u16(
                    (U16 *)&gi2_mvx_range_high_quality[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0]);
                uint32x4_t total_cost;
                U16 sad_array[2][4];

                /* mv weight + ref_idx */
                mv_wt = vadd_u16(mv_wt, vget_low_u16(mvy_wt));

                total_cost = vmulq_u32(v_lambda, vmovl_u16(mv_wt));
                total_cost = vaddq_u32(total_cost, v_rnd_factor);
                total_cost = vshlq_u32(total_cost, v_lambda_q_shift);

                sad_4x8 = vadd_u16(vmovn_u32(total_cost), sad_4x8);
                sad_8x4 = vadd_u16(vmovn_u32(total_cost), sad_8x4);

                vst1_u16(sad_array[0], sad_4x8);
                vst1_u16(sad_array[1], sad_8x4);

                for(i = 0; i < 4; i++)
                {
                    BEST_COST(i);
                }

                mvx += stepx * 4;
            }
            else
            {
                S16 sad_array[2][1];
                S32 mv_cost;

                /* Get SAD by adding SAD for current and neighbour S  */
                sad_array[0][0] = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_south[sad_pos];
                sad_array[1][0] = pi2_sads_4x4_current[sad_pos] + pi2_sads_4x4_east[sad_pos];

                mv_cost = gi2_mvy_range[ABS(mvy)][0] +
                          gi2_mvx_range_high_quality[mvx + MAX_MVX_SUPPORTED_IN_COARSE_LAYER][0] +
                          i1_ref_idx;

                mv_cost = (mv_cost * lambda + rnd) >> lambda_q_shift;

                sad_array[0][0] += mv_cost;
                sad_array[1][0] += mv_cost;

                BEST_COST(0);
                mvx += stepx;
            }
        }
    }

    ps_best_mv_4x8->i2_mv_x = best_mv_x_4x8;
    ps_best_mv_4x8->i2_mv_y = best_mv_y_4x8;

    ps_best_mv_8x4->i2_mv_x = best_mv_x_8x4;
    ps_best_mv_8x4->i2_mv_y = best_mv_y_8x4;
}
