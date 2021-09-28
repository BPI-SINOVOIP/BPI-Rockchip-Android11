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
* @file ihevce_cabac_rdo.c
*
* @brief
*  This file contains function definitions for rdopt cabac entropy modules
*
* @author
*  ittiam
*
* @List of Functions
*  ihevce_entropy_rdo_frame_init()
*  ihevce_entropy_rdo_ctb_init()
*  ihevce_entropy_rdo_encode_cu()
*  ihevce_cabac_rdo_encode_sao()
*  ihevce_update_best_sao_cabac_state()
*  ihevce_entropy_update_best_cu_states()
*  ihevce_entropy_rdo_encode_tu()
*  ihevce_entropy_rdo_encode_tu_rdoq()
*  ihevce_entropy_rdo_copy_states()
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
#include "ihevce_cabac_rdo.h"
#include "ihevce_trace.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
******************************************************************************
*
*  @brief Cabac rdopt frame level initialization.
*
*  @par   Description
*  Registers the sps,vps,pps,slice header pointers in rdopt enntropy contexts
*  and intializes cabac engine (init states) for each init cu and scratch cu
*  contexts
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*  pointer to rdopt entropy context (handle)
*
*  @param[in]   ps_slice_hdr
*  pointer to  current slice header
*
*  @param[in]   ps_sps
*  pointer to active SPS params
*
*  @param[in]   ps_pps
*  pointer to active PPS params
*
*  @param[in]   ps_vps
*  pointer to active VPS params
*
*  @param[in]   pu1_cu_skip_top_row
*  pointer to top row cu skip flags (registered at frame level)
*
*  @return      none
*
******************************************************************************
*/
void ihevce_entropy_rdo_frame_init(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    slice_header_t *ps_slice_hdr,
    pps_t *ps_pps,
    sps_t *ps_sps,
    vps_t *ps_vps,
    UWORD8 *pu1_cu_skip_top_row,
    rc_quant_t *ps_rc_quant_ctxt)
{
    WORD32 slice_qp = ps_slice_hdr->i1_slice_qp_delta + ps_pps->i1_pic_init_qp;

    /* Initialize the CTB size from sps parameters */
    WORD32 log2_ctb_size =
        ps_sps->i1_log2_min_coding_block_size + ps_sps->i1_log2_diff_max_min_coding_block_size;

    WORD32 cabac_init_idc;

    (void)ps_rc_quant_ctxt;
    /* sanity checks */
    ASSERT((log2_ctb_size >= 3) && (log2_ctb_size <= 6));
    ASSERT((slice_qp >= ps_rc_quant_ctxt->i2_min_qp) && (slice_qp <= ps_rc_quant_ctxt->i2_max_qp));

    /* register the sps,vps,pps, slice header pts in all cu entropy ctxts   */
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].ps_vps = ps_vps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].ps_sps = ps_sps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].ps_pps = ps_pps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].ps_slice_hdr = ps_slice_hdr;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].ps_vps = ps_vps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].ps_sps = ps_sps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].ps_pps = ps_pps;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].ps_slice_hdr = ps_slice_hdr;

    /* initialze the skip cu top row ptrs for all rdo entropy contexts      */
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].pu1_skip_cu_top = pu1_cu_skip_top_row;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].pu1_skip_cu_top = pu1_cu_skip_top_row;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i1_log2_ctb_size = log2_ctb_size;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i1_log2_ctb_size = log2_ctb_size;

    /* initialze the skip cu left flagd for all rdo entropy contexts       */
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].u4_skip_cu_left = 0;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].u4_skip_cu_left = 0;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i1_ctb_num_pcm_blks = 0;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i1_ctb_num_pcm_blks = 0;

    /* residue encoding should be enaled if ZERO_CBF eval is disabled */
#if((!RDOPT_ZERO_CBF_ENABLE) && (RDOPT_ENABLE))
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i4_enable_res_encode = 1;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i4_enable_res_encode = 1;
#else
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i4_enable_res_encode = 0;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i4_enable_res_encode = 0;
#endif

    /*************************************************************************/
    /* Note pu1_cbf_cb, pu1_cbf_cr initialization are done with array idx 1  */
    /* This is because these flags are accessed as pu1_cbf_cb[tfr_depth - 1] */
    /* without cheking for tfr_depth= 0                                      */
    /*************************************************************************/
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].apu1_cbf_cb[0] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cb[0][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].apu1_cbf_cb[0] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cb[0][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].apu1_cbf_cr[0] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cr[0][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].apu1_cbf_cr[0] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cr[0][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].apu1_cbf_cb[1] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cb[1][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].apu1_cbf_cb[1] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cb[1][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].apu1_cbf_cr[1] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cr[1][1];

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].apu1_cbf_cr[1] =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cr[1][1];

    memset(
        ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cb,
        0,
        (MAX_TFR_DEPTH + 1) * 2 * sizeof(UWORD8));

    memset(
        ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cb,
        0,
        (MAX_TFR_DEPTH + 1) * 2 * sizeof(UWORD8));

    memset(
        ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].au1_cbf_cr,
        0,
        (MAX_TFR_DEPTH + 1) * 2 * sizeof(UWORD8));

    memset(
        ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].au1_cbf_cr,
        0,
        (MAX_TFR_DEPTH + 1) * 2 * sizeof(UWORD8));

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

    /* all the entropy contexts in rdo initialized in bit compute mode      */
    ihevce_cabac_init(
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].s_cabac_ctxt,
        NULL, /* bitstream buffer not required in bits compute mode */
        CLIP3(slice_qp, 0, IHEVC_MAX_QP),
        cabac_init_idc,
        CABAC_MODE_COMPUTE_BITS);

    ihevce_cabac_init(
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].s_cabac_ctxt,
        NULL, /* bitstream buffer not required in bits compute mode */
        CLIP3(slice_qp, 0, IHEVC_MAX_QP),
        cabac_init_idc,
        CABAC_MODE_COMPUTE_BITS);

    /* initialize the entropy states in rdopt struct  */
    COPY_CABAC_STATES(
        &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].s_cabac_ctxt.au1_ctxt_models[0],
        sizeof(ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states));
}

/**
******************************************************************************
*
*  @brief Cabac rdopt ctb level initialization.
*
*  @par   Description
*  initialzes the ctb x and y co-ordinates for all the rdopt entropy contexts
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*  pointer to rdopt entropy context (handle)
*
*  @param[in]   ctb_x
*  current ctb x offset w.r.t frame start (ctb units)
*
*  @param[in]   ctb_y
*  current ctb y offset w.r.t frame start (ctb units)
*
*  @return      none
*
******************************************************************************
*/
void ihevce_entropy_rdo_ctb_init(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, WORD32 ctb_x, WORD32 ctb_y)
{
    /* initialze the ctb x and y co-ordinates for all the rdopt entropy contexts */
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i4_ctb_x = ctb_x;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i4_ctb_x = ctb_x;

    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].i4_ctb_y = ctb_y;
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].i4_ctb_y = ctb_y;
}

/**
******************************************************************************
*
*  @brief Cabac rdopt cu encode function to compute luma bits for a given cu
*         only luma bits are used for rd optimization currently
*
*  @par   Description
*  use a scratch CU entropy context (indicated by rdopt_buf_idx) whose cabac
*  states are reset (to CU init state) and calls the cabac entropy coding
*  unit function to compute the total bits for current CU
*
*  A local CU structutre is prepared (in stack) as the structures that entropy
*  encode expects and the rdopt gets are different
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   ps_cu_prms
*   pointer to current CU params whose bits are computed
*
*  @param[in]   cu_pos_x
*   current CU x position w.r.t ctb (in 8x8 units)
*
*  @param[in]   cu_pos_y
*   current CU y position w.r.t ctb (in 8x8 units)
*
*  @param[in]   cu_size
*   current cu size (in pel units)
*
*  @param[in]   top_avail
*   top avaialability flag for current CU (required for encoding skip flag)
*
*  @param[in]   left_avail
*   left avaialability flag for current CU (required for encoding skip flag)
*
*  @param[in]   pv_ecd_coeff
*   Compressed coeff residue buffer (for luma)
*
*  @param[in]   rdopt_buf_idx
*   corresponds to the id of the scratch CU entropy context that needs to be
*   used for bit estimation
*
*  @param[out]   pi4_cu_rdopt_tex_bits
*   returns cbf bits if zer0 cbf eval flag is enabled otherwiese returns total
*   tex(including cbf bits)
*
*  @return      total bits required to encode the current CU
*
******************************************************************************
*/
WORD32 ihevce_entropy_rdo_encode_cu(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    enc_loop_cu_final_prms_t *ps_cu_prms,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 cu_size,
    WORD32 top_avail,
    WORD32 left_avail,
    void *pv_ecd_coeff,
    WORD32 *pi4_cu_rdopt_tex_bits)
{
    /* local cu structure for passing to entrop encode cu module */
    cu_enc_loop_out_t s_enc_cu;
    WORD32 rdopt_buf_idx = ps_rdopt_entropy_ctxt->i4_curr_buf_idx;

    entropy_context_t *ps_cur_cu_entropy =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[rdopt_buf_idx];

    WORD32 total_bits = 0;

    WORD32 log2_ctb_size = ps_cur_cu_entropy->i1_log2_ctb_size;
    WORD32 log2_cu_size;

    WORD32 cu_depth;

    /* sanity checks */
    ASSERT((rdopt_buf_idx == 0) || (rdopt_buf_idx == 1));
    ASSERT((cu_size >= 8) && (cu_size <= (1 << log2_ctb_size)));
    ASSERT((cu_pos_x >= 0) && (cu_pos_x <= (1 << (log2_ctb_size - 3))));
    ASSERT((cu_pos_y >= 0) && (cu_pos_y <= (1 << (log2_ctb_size - 3))));

    GETRANGE(log2_cu_size, cu_size);
    log2_cu_size -= 1;
    cu_depth = log2_ctb_size - log2_cu_size;

    {
        /**********************************************************/
        /* prepare local cu structure before calling cabac encode */
        /**********************************************************/

        /* default be canged to have orred val*/
        s_enc_cu.b1_no_residual_syntax_flag = 0;

        /* initialize cu posx, posy and size */
        s_enc_cu.b3_cu_pos_x = cu_pos_x;
        s_enc_cu.b3_cu_pos_y = cu_pos_y;
        s_enc_cu.b4_cu_size = (cu_size >> 3);

        /* PCM not supported */
        s_enc_cu.b1_pcm_flag = 0;
        s_enc_cu.b1_pred_mode_flag = ps_cu_prms->u1_intra_flag;
        s_enc_cu.b3_part_mode = ps_cu_prms->u1_part_mode;

        s_enc_cu.b1_skip_flag = ps_cu_prms->u1_skip_flag;
        s_enc_cu.b1_tq_bypass_flag = 0;
        s_enc_cu.pv_coeff = pv_ecd_coeff;

        /* store the number of TUs */
        s_enc_cu.u2_num_tus_in_cu = ps_cu_prms->u2_num_tus_in_cu;

        /* ---- intialize the PUs and TUs start ptrs for cur CU ----- */
        s_enc_cu.ps_pu = &ps_cu_prms->as_pu_enc_loop[0];
        s_enc_cu.ps_enc_tu = &ps_cu_prms->as_tu_enc_loop[0];

        /* Corner case : If Part is 2Nx2N and Merge has all TU with zero cbf */
        /* then it has to be coded as skip CU */
        if((SIZE_2Nx2N == ps_cu_prms->u1_part_mode) &&
           /*(1 == ps_cu_prms->u2_num_tus_in_cu) &&*/
           (1 == ps_cu_prms->as_pu_enc_loop[0].b1_merge_flag) && (0 == ps_cu_prms->u1_skip_flag) &&
           (0 == ps_cu_prms->u1_is_cu_coded))
        {
            s_enc_cu.b1_skip_flag = 1;
        }

        if(s_enc_cu.b1_pred_mode_flag == PRED_MODE_INTER)
        {
            s_enc_cu.b1_no_residual_syntax_flag = !ps_cu_prms->u1_is_cu_coded;
        }
        else /* b1_pred_mode_flag == PRED_MODE_INTRA */
        {
            /* copy prev_mode_flag, mpm_idx and rem_intra_pred_mode for each PU */
            memcpy(
                &s_enc_cu.as_prev_rem[0],
                &ps_cu_prms->as_intra_prev_rem[0],
                ps_cu_prms->u2_num_tus_in_cu * sizeof(intra_prev_rem_flags_t));

            s_enc_cu.b3_chroma_intra_pred_mode = ps_cu_prms->u1_chroma_intra_pred_mode;
        }
    }

    /* reset the total bits in cabac engine to zero */
    ps_cur_cu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 = 0;
    ps_cur_cu_entropy->s_cabac_ctxt.u4_texture_bits_estimated_q12 = 0;
    ps_cur_cu_entropy->s_cabac_ctxt.u4_cbf_bits_q12 = 0;
    ps_cur_cu_entropy->i1_encode_qp_delta = 0;

    /* Call the cabac encode function of current cu to compute bits */
    ihevce_cabac_encode_coding_unit(ps_cur_cu_entropy, &s_enc_cu, cu_depth, top_avail, left_avail);

    /* return total bits after rounding the fractional bits */
    total_bits =
        (ps_cur_cu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >>
        CABAC_FRAC_BITS_Q;
#if RDOPT_ZERO_CBF_ENABLE
    ASSERT(ps_cur_cu_entropy->s_cabac_ctxt.u4_texture_bits_estimated_q12 == 0);
#endif
    /* return total texture bits rounding the fractional bits */
    *pi4_cu_rdopt_tex_bits =
        (ps_cur_cu_entropy->s_cabac_ctxt.u4_cbf_bits_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >>
        CABAC_FRAC_BITS_Q;

    /*   (   ps_cur_cu_entropy->s_cabac_ctxt.u4_texture_bits_estimated_q12 +
                (1 << (CABAC_FRAC_BITS_Q - 1))
            ) >>  CABAC_FRAC_BITS_Q;*/

    return (total_bits);
}

/**
******************************************************************************
*
*  @brief Cabac rdo encode sao function to compute bits required for a given
*         ctb to be encoded with any sao type or no SAO.
*
*  @par   Description
*  use a scratch CU entropy context (indicated by rdopt_buf_idx) and init cabac
*  states are reset (to CU init state) and calls the cabac encode sao
*  function to compute the total bits for current CTB
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   ps_ctb_enc_loop_out
*   pointer to current enc loop CTB output structure
*
*  @return      total bits required to encode the current CTB
*
******************************************************************************
*/
WORD32 ihevce_cabac_rdo_encode_sao(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, ctb_enc_loop_out_t *ps_ctb_enc_loop_out)
{
    /* index to curr buf*/
    WORD32 rdopt_buf_idx = ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
    WORD32 total_bits = 0;
    entropy_context_t *ps_cur_ctb_entropy =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[rdopt_buf_idx];

    /* copy the intial entropy states from backuped buf to curr buf  */
    memcpy(
        &ps_cur_ctb_entropy->s_cabac_ctxt.au1_ctxt_models[0],
        &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
        sizeof(ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states));

    /* reset the total bits in cabac engine to zero */
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_bits_estimated_q12 = 0;
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_texture_bits_estimated_q12 = 0;
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_cbf_bits_q12 = 0;
    ps_cur_ctb_entropy->i1_encode_qp_delta = 0;
    //ps_cur_ctb_entropy->s_cabac_ctxt.u4_range = 0;

    ASSERT(ps_cur_ctb_entropy->s_cabac_ctxt.u4_range == 0);
    ihevce_cabac_encode_sao(ps_cur_ctb_entropy, ps_ctb_enc_loop_out);

    /* return total bits after rounding the fractional bits */
    total_bits =
        (ps_cur_ctb_entropy->s_cabac_ctxt.u4_bits_estimated_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >>
        CABAC_FRAC_BITS_Q;

    return (total_bits);
}

/**
******************************************************************************
*
*  @brief Updates best sao cabac state.
*
*  @par   Description
*         Copies the cabac states of best cand to init states buf for next ctb.
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   i4_best_buf_idx
*   Index to the buffer having the cabac states of best candidate
*
*  @return   Success/failure
*
******************************************************************************
*/
WORD32 ihevce_update_best_sao_cabac_state(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, WORD32 i4_best_buf_idx)
{
    /* local cu structure for passing to entrop encode cu module */
    WORD32 rdopt_buf_idx = i4_best_buf_idx;
    entropy_context_t *ps_cur_ctb_entropy =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[rdopt_buf_idx];

    /* copy the intial entropy states from best buf to intial states buf */
    memcpy(
        &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
        &ps_cur_ctb_entropy->s_cabac_ctxt.au1_ctxt_models[0],
        sizeof(ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states));

    /* reset the total bits in cabac engine to zero */
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_bits_estimated_q12 = 0;
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_texture_bits_estimated_q12 = 0;
    ps_cur_ctb_entropy->s_cabac_ctxt.u4_cbf_bits_q12 = 0;
    ps_cur_ctb_entropy->i1_encode_qp_delta = 0;

    return (1);
}

/**
******************************************************************************
*
*  @brief Cabac rdopt cu encode function to compute luma bits for a given cu
*         only luma bits are used for rd optimization currently
*
*  @par   Description
*  use a scratch CU entropy context (indicated by rdopt_buf_idx) whose cabac
*  states are reset (to CU init state) and calls the cabac entropy coding
*  unit function to compute the total bits for current CU
*
*  A local CU structutre is prepared (in stack) as the structures that entropy
*  encode expects and the rdopt gets are different
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   cu_pos_x
*   current CU x position w.r.t ctb (in 8x8 units)
*
*  @param[in]   cu_pos_y
*   current CU y position w.r.t ctb (in 8x8 units)
*
*  @param[in]   cu_size
*   current cu size (in pel units)
*
*  @param[in]   rdopt_best_cu_idx
*   id of the best CU entropy ctxt (rdopt winner candidate)
*
*  @return      total bits required to encode the current CU
*
******************************************************************************
*/
void ihevce_entropy_update_best_cu_states(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    WORD32 cu_pos_x,
    WORD32 cu_pos_y,
    WORD32 cu_size,
    WORD32 cu_skip_flag,
    WORD32 rdopt_best_cu_idx)
{
    entropy_context_t *ps_best_cu_entropy =
        &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[rdopt_best_cu_idx];

    /* CTB x co-ordinate w.r.t frame start           */
    WORD32 ctb_x0_frm = (ps_best_cu_entropy->i4_ctb_x << ps_best_cu_entropy->i1_log2_ctb_size);

    /* CU x co-ordinate w.r.t frame start           */
    WORD32 cu_x0_frm = cu_pos_x + ctb_x0_frm;

    /* bit postion from where top skip flag is extracted; 1bit per 8 pel   */
    WORD32 x_pos = ((cu_x0_frm >> 3) & 0x7);

    /* bit postion from where left skip flag is extracted; 1bit per 8 pel  */
    WORD32 y_pos = ((cu_pos_y >> 3) & 0x7);

    /* top and left skip flags computed based on nbr availability */
    UWORD8 *pu1_top_skip_flags = ps_best_cu_entropy->pu1_skip_cu_top + (cu_x0_frm >> 6);

    UWORD32 u4_skip_left_flags = ps_best_cu_entropy->u4_skip_cu_left;

    ps_best_cu_entropy = &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[rdopt_best_cu_idx];

    /* copy the entropy states from best rdopt cu states to init states  */
    COPY_CABAC_STATES(
        &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
        &ps_best_cu_entropy->s_cabac_ctxt.au1_ctxt_models[0],
        sizeof(ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states));

    /* replicate skip flag in left and top row cu skip flags */
    if(cu_skip_flag)
    {
        SET_BITS(pu1_top_skip_flags[0], x_pos, (cu_size >> 3));
        SET_BITS(u4_skip_left_flags, y_pos, (cu_size >> 3));
    }
    else
    {
        CLEAR_BITS(pu1_top_skip_flags[0], x_pos, (cu_size >> 3));
        CLEAR_BITS(u4_skip_left_flags, y_pos, (cu_size >> 3));
    }

    /* copy the left skip flags in both the rdopt contexts */
    ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[0].u4_skip_cu_left =
        ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[1].u4_skip_cu_left = u4_skip_left_flags;
}

/**
******************************************************************************
*
*  @brief Cabac rdopt tu encode function to compute luma bits for a given tu
*         only luma bits are used for rd optimization currently
*
*  @par   Description
*  use a scratch CU entropy context (indicated by rdopt_buf_idx) whose cabac
*  states are reset (to CU init state for first tu) and calls the cabac residue
*  coding function to compute the total bits for current TU
*
*  Note : TU includes only residual coding bits and does not include
*         tu split, cbf and qp delta encoding bits for a TU
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   pv_ecd_coeff
*   Compressed coeff residue buffer (for luma)
*
*  @param[in]   transform_size
*   current tu size in pel units
*
*  @param[in]   is_luma
*   indicates if it is luma or chrom TU (required for residue encode)
*
*  @return      total bits required to encode the current TU
*
******************************************************************************
*/
WORD32 ihevce_entropy_rdo_encode_tu(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    void *pv_ecd_coeff,
    WORD32 transform_size,
    WORD32 is_luma,
    WORD32 perform_sbh)
{
    WORD32 log2_tfr_size;
    WORD32 total_bits = 0;
    WORD32 curr_buf_idx = ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
    entropy_context_t *ps_cur_tu_entropy;

    ps_cur_tu_entropy = &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[curr_buf_idx];

    ASSERT((transform_size >= 4) && (transform_size <= 32));

    /* transform size to log2transform size */
    GETRANGE(log2_tfr_size, transform_size);
    log2_tfr_size -= 1;

    /* reset the total bits in cabac engine to zero */
    ps_cur_tu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 = 0;
    ps_cur_tu_entropy->i1_encode_qp_delta = 0;

    /* Call the cabac residue encode function to compute TU bits */
    ihevce_cabac_residue_encode_rdopt(
        ps_cur_tu_entropy, pv_ecd_coeff, log2_tfr_size, is_luma, perform_sbh);

    /* return total bits after rounding the fractional bits */
    total_bits =
        (ps_cur_tu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >>
        CABAC_FRAC_BITS_Q;

    return (total_bits);
}

/**
******************************************************************************
*
*  @brief Cabac rdopt tu encode function to compute bits for a given tu. Actual
*  RDOQ algorithm is performed by the ihevce_cabac_residue_encode_rdoq function
*  called by this function.
*
*  @par   Description
*  use a scratch CU entropy context (indicated by rdopt_buf_idx) whose cabac
*  states are reset (to CU init state for first tu) and calls the cabac residue
*  coding function to compute the total bits for current TU
*
*  Note : TU includes only residual coding bits and does not include
*         tu split, cbf and qp delta encoding bits for a TU
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*   pointer to rdopt entropy context (handle)
*
*  @param[in]   pv_ecd_coeff
*   Compressed coeff residue buffer
*
*  @param[in]   transform_size
*   current tu size in pel units
*
*  @param[in]   first_tu_of_cu
*   indicates if the tu is the first unit of cu (required for initializing
*   cabac ctxts)
*
*  @param[in]   rdopt_buf_idx
*   corresponds to the id of the rdopt CU entropy context that needs to be
*   used for bit estimation
*
*  @param[in]   is_luma
*   indicates if it is luma or chrom TU (required for residue encode)
*
*  @param[in]   intra_nxn_mode
*   indicates if it is luma or chrom TU (required for residue encode)
*
*  @param[inout] ps_rdoq_ctxt
*  pointer to rdoq context structure
*
*  @param[inout] pi4_coded_tu_dist
*  Pointer to the variable which will contain the transform domain distortion
*  of the entire TU, when any of the coeffs in the TU are coded
*
*  @param[inout] pi4_not_coded_tu_dist
*  Pointer to the variable which will contain the transform domain distortion
*  of the enture TU, when all the coeffs in the TU are coded
*
*  @return      total bits required to encode the current TU
*
******************************************************************************
*/
WORD32 ihevce_entropy_rdo_encode_tu_rdoq(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt,
    void *pv_ecd_coeff,
    WORD32 transform_size,
    WORD32 is_luma,
    rdoq_sbh_ctxt_t *ps_rdoq_ctxt,
    LWORD64 *pi8_coded_tu_dist,
    LWORD64 *pi8_not_coded_tu_dist,
    WORD32 perform_sbh)
{
    WORD32 log2_tfr_size;
    WORD32 total_bits = 0;
    WORD32 curr_buf_idx = ps_rdopt_entropy_ctxt->i4_curr_buf_idx;
    entropy_context_t *ps_cur_tu_entropy;

    ps_cur_tu_entropy = &ps_rdopt_entropy_ctxt->as_cu_entropy_ctxt[curr_buf_idx];

    ASSERT((transform_size >= 4) && (transform_size <= 32));

    /* transform size to log2transform size */
    GETRANGE(log2_tfr_size, transform_size);
    log2_tfr_size -= 1;

    /* reset the total bits in cabac engine to zero */
    ps_cur_tu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 = 0;
    ps_cur_tu_entropy->i1_encode_qp_delta = 0;

    /* Call the cabac residue encode function to compute TU bits */
    ihevce_cabac_residue_encode_rdoq(
        ps_cur_tu_entropy,
        pv_ecd_coeff,
        log2_tfr_size,
        is_luma,
        (void *)ps_rdoq_ctxt,
        pi8_coded_tu_dist,
        pi8_not_coded_tu_dist,
        perform_sbh);

    /* return total bits after rounding the fractional bits */
    total_bits =
        (ps_cur_tu_entropy->s_cabac_ctxt.u4_bits_estimated_q12 + (1 << (CABAC_FRAC_BITS_Q - 1))) >>
        CABAC_FRAC_BITS_Q;

    return (total_bits);
}

/**
******************************************************************************
*
*  @brief Cabac rdopt copy functions for copying states (which will be used later)
*
*  @par   Description
*  Does the HEVC style of entropy sync by copying the state to/from rdo context
*  from/to row level cabac states at start of row/2nd ctb of row
*
*  Caller needs to make sure UPDATE_ENT_SYNC_RDO_STATE is used for first ctb of
*  every row (leaving first row of slice) and STORE_ENT_SYNC_RDO_STATE is used for
*  storing the cabac states at the end of 2nd ctb of a row.
*
*  @param[inout]   ps_rdopt_entropy_ctxt
*  pointer to rdopt entropy context (handle)
*
*  @param[in]   pu1_entropy_sync_states
*  pointer to entropy sync cabac states
*
*  @param[in]   copy_mode
*  mode of copying cabac states. Shall be either UPDATE_ENT_SYNC_RDO_STATE and
*  STORE_ENT_SYNC_RDO_STATE
*
******************************************************************************
*/
void ihevce_entropy_rdo_copy_states(
    rdopt_entropy_ctxt_t *ps_rdopt_entropy_ctxt, UWORD8 *pu1_entropy_sync_states, WORD32 copy_mode)
{
    /* sanity checks */
    ASSERT((copy_mode == STORE_ENT_SYNC_RDO_STATE) || (copy_mode == UPDATE_ENT_SYNC_RDO_STATE));

    if(STORE_ENT_SYNC_RDO_STATE == copy_mode)
    {
        COPY_CABAC_STATES(
            pu1_entropy_sync_states,
            &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
            IHEVC_CAB_CTXT_END);
    }
    else if(UPDATE_ENT_SYNC_RDO_STATE == copy_mode)
    {
        COPY_CABAC_STATES(
            &ps_rdopt_entropy_ctxt->au1_init_cabac_ctxt_states[0],
            pu1_entropy_sync_states,
            IHEVC_CAB_CTXT_END);
    }
}
