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
* \file ihevce_frame_process_utils.c
*
* \brief
*    This file contains definitions of top level functions related to frame
*    processing
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* List of Functions
*
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
#include "ihevc_debug.h"
#include "ihevc_macros.h"
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
#include "ihevc_common_tables.h"

#include "ihevce_defs.h"
#include "ihevce_hle_interface.h"
#include "ihevce_hle_q_func.h"
#include "ihevce_lap_enc_structs.h"
#include "ihevce_multi_thrd_structs.h"
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_checks.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_global_tables.h"
#include "ihevce_rc_enc_structs.h"
#include "ihevce_rc_interface.h"
#include "ihevce_frame_process_utils.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Globals                                                                   */
/*****************************************************************************/

/************** Version Number string *******************/
UWORD8 gau1_version_string[] = "i265-v4.13-218 Build ";

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
*
* @brief
*    API to return frame qp in constant qp mode based on init I frame qp,
*    slice type and current temporal layer.
*
*      I picture is given the same qp as the init qp configure in static params
*      P picture is set equal to I frame qp + 1
*      B picture is set equal to P frame qp + temporal layer
*
* @param[in] static_params_frame_qp
*   frame level qp set for I frames in create time params
*
* @param[in] slice_type
*   slice type for current frame (I/P/B)
*
* @param[in] temporal_id
*   temoporal layer ID of the current frame. This is associalted with B frame.
*   temporal layer ID. I and P frames have temporal_id set to 0.
*
* @param[in] min_qp
*   minimum qp to be allocated for this frame.
*
* @param[in] max_qp
*   maximum qp to be allocated for this frame
*
* @return
*    current frame qp
*
* @author
*  Ittiam
*
* @remarks
*  This is right place to plug in frame level RC call for current frame qp
*  allocation later when RC support is added
*
*****************************************************************************
*/
WORD32 ihevce_get_cur_frame_qp(
    WORD32 static_params_frame_qp,
    WORD32 slice_type,
    WORD32 temporal_id,
    WORD32 min_qp,
    WORD32 max_qp,
    rc_quant_t *ps_rc_quant_ctxt)
{
    WORD32 i4_curr_qp = static_params_frame_qp;

    /* sanity checks */
    ASSERT(max_qp >= min_qp);
    ASSERT((min_qp >= ps_rc_quant_ctxt->i2_min_qp) && (min_qp <= ps_rc_quant_ctxt->i2_max_qp));
    ASSERT(
        (static_params_frame_qp >= ps_rc_quant_ctxt->i2_min_qp) &&
        (static_params_frame_qp <= ps_rc_quant_ctxt->i2_max_qp));
    if(ISLICE == slice_type)
    {
        /* I frame qp is same as init qp in static params   */
        i4_curr_qp = static_params_frame_qp;
    }
    else if(PSLICE == slice_type)
    {
        /* P frame qp is I frame qp + 1                     */
        i4_curr_qp = static_params_frame_qp + 1;
    }
    else if(BSLICE == slice_type)
    {
        /* B frame qp is I frame qp + 1 + temporal layer id */
        i4_curr_qp = static_params_frame_qp + temporal_id + 1;
    }
    else
    {
        /* illegal slice type */
        ASSERT(0);
    }

    i4_curr_qp = CLIP3(i4_curr_qp, min_qp, max_qp);

    return (i4_curr_qp);
}

/*!
******************************************************************************
* \if Function name : calc_block_ssim \endif
*
* \brief
*    Calc Block SSIM
*
* \return
*    None
*
* \author
*  Ittiam
*****************************************************************************
*/
unsigned int calc_block_ssim(
    unsigned char *pu1_ref,
    unsigned char *pu1_tst,
    unsigned char *pu1_win,
    WORD32 i4_horz_jump,
    unsigned short u2_ref_stride,
    unsigned short u2_tst_stride,
    unsigned char u1_win_size,
    unsigned char u1_win_q_shift)
{
    unsigned int u4_wtd_ref_mean, u4_wtd_tst_mean, u4_wtd_ref_sq, u4_wtd_tst_sq, u4_wtd_ref_tst;
    unsigned int u4_wtd_ref_mean_sq, u4_wtd_tst_mean_sq, u4_wtd_ref_tst_mean_prod;
    unsigned char u1_wt, u1_ref_smpl, u1_tst_smpl;
    unsigned short u2_wtd_ref_smpl, u2_wtd_tst_smpl, u2_win_q_rounding;
    int i4_row, i4_col;

    u4_wtd_ref_mean = 0;
    u4_wtd_tst_mean = 0;
    u4_wtd_ref_sq = 0;
    u4_wtd_tst_sq = 0;
    u4_wtd_ref_tst = 0;

    for(i4_row = 0; i4_row < u1_win_size; i4_row++)
    {
        for(i4_col = 0; i4_col < u1_win_size; i4_col++)
        {
            u1_wt = *pu1_win++;
            u1_ref_smpl = pu1_ref[i4_col * i4_horz_jump];
            u1_tst_smpl = pu1_tst[i4_col * i4_horz_jump];

            u2_wtd_ref_smpl = u1_wt * u1_ref_smpl;
            u2_wtd_tst_smpl = u1_wt * u1_tst_smpl;

            u4_wtd_ref_mean += u2_wtd_ref_smpl;
            u4_wtd_tst_mean += u2_wtd_tst_smpl;

            u4_wtd_ref_sq += u2_wtd_ref_smpl * u1_ref_smpl;
            u4_wtd_tst_sq += u2_wtd_tst_smpl * u1_tst_smpl;
            u4_wtd_ref_tst += u2_wtd_ref_smpl * u1_tst_smpl;
        }
        pu1_ref += u2_ref_stride;
        pu1_tst += u2_tst_stride;
    }

    {
        unsigned int u4_num, u4_den, u4_term1;

        u2_win_q_rounding = (1 << u1_win_q_shift) >> 1;
        u4_wtd_ref_mean += (u2_win_q_rounding >> 8);
        u4_wtd_tst_mean += (u2_win_q_rounding >> 8);

        /* Keep the mean terms within 16-bits before squaring */
        u4_wtd_ref_mean >>= (u1_win_q_shift - 8);
        u4_wtd_tst_mean >>= (u1_win_q_shift - 8);

        /* Bring down the square of sum terms to same Q format as the sum of square terms */
        u4_wtd_ref_mean_sq = (u4_wtd_ref_mean * u4_wtd_ref_mean + 16) >> (16 - u1_win_q_shift);
        u4_wtd_tst_mean_sq = (u4_wtd_tst_mean * u4_wtd_tst_mean + 16) >> (16 - u1_win_q_shift);
        u4_wtd_ref_tst_mean_prod = (u4_wtd_ref_mean * u4_wtd_tst_mean + 16) >>
                                   (16 - u1_win_q_shift);

        /* Compute self and cross variances */
        if(u4_wtd_ref_sq > u4_wtd_ref_mean_sq)
            u4_wtd_ref_sq -= u4_wtd_ref_mean_sq;
        else
            u4_wtd_ref_sq = 0;

        if(u4_wtd_tst_sq > u4_wtd_tst_mean_sq)
            u4_wtd_tst_sq -= u4_wtd_tst_mean_sq;
        else
            u4_wtd_tst_sq = 0;

        if(u4_wtd_ref_tst > u4_wtd_ref_tst_mean_prod)
            u4_wtd_ref_tst -= u4_wtd_ref_tst_mean_prod;
        else
            u4_wtd_ref_tst = 0;

        /* Keep the numerator in Q12 format before division */
        u4_num = ((u4_wtd_ref_tst_mean_prod << 1) + C1) << (12 - u1_win_q_shift);
        u4_den = ((u4_wtd_ref_mean_sq + u4_wtd_tst_mean_sq) + C1 + u2_win_q_rounding) >>
                 u1_win_q_shift;
        u4_term1 = (u4_num) / u4_den;

        u4_num = (u4_wtd_ref_tst << 1) + C2;
        u4_den = (u4_wtd_ref_sq + u4_wtd_tst_sq) + C2;
        /* If numerator takes less than 20-bits, product would not overflow; so no need to normalize */
        if(u4_num < 1048576)
        {
            return ((u4_num * u4_term1) / u4_den);
        }

        /* While the above should be done really with getRange calculation, for simplicity,
        the other cases go through a less accurate calculation */
        u4_num = (u4_num + u2_win_q_rounding) >> u1_win_q_shift;
        u4_den = (u4_den + u2_win_q_rounding) >> u1_win_q_shift;

        /* What is returned is SSIM in 1Q12 */
        return ((u4_term1 * u4_num) / u4_den);
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_fill_sei_payload \endif
*
* \brief
*    Fills SEI Payload
*
* \param[in]    ps_enc_ctxt
* Encoder Context
*
* \param[in]    ps_curr_inp
* Current Input pointer
*
* \param[in]    ps_curr_out
* Current Output pointer
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_fill_sei_payload(
    enc_ctxt_t *ps_enc_ctxt,
    ihevce_lap_enc_buf_t *ps_curr_inp,
    frm_proc_ent_cod_ctxt_t *ps_curr_out)
{
    UWORD32 *pu4_length, i4_cmd_len;
    UWORD32 *pu4_tag, i4_pic_type;
    UWORD8 *pu1_user_data;

    pu4_tag = ((UWORD32 *)(ps_curr_inp->s_input_buf.pv_synch_ctrl_bufs));
    ps_curr_out->u4_num_sei_payload = 0;
    i4_pic_type = ps_curr_inp->s_lap_out.i4_pic_type;
    (void)ps_enc_ctxt;
    while(1)
    {
        if(((*pu4_tag) & IHEVCE_COMMANDS_TAG_MASK) == IHEVCE_SYNCH_API_END_TAG)
            break;

        pu4_length = pu4_tag + 1;
        pu1_user_data = (UWORD8 *)(pu4_length + 1);
        i4_cmd_len = *pu4_length;

        if((*pu4_tag & IHEVCE_COMMANDS_TAG_MASK) == IHEVCE_SYNCH_API_REG_KEYFRAME_SEI_TAG)
        {
            if(i4_pic_type == IV_IDR_FRAME)
            {
                memcpy(
                    (void *)((ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload]
                                  .pu1_sei_payload)),
                    (void *)pu1_user_data,
                    i4_cmd_len);
                ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload].u4_payload_length =
                    (i4_cmd_len);
                ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload].u4_payload_type =
                    ((*pu4_tag & IHEVCE_PAYLOAD_TYPE_MASK) >> IHEVCE_PAYLOAD_TYPE_SHIFT);
                ps_curr_out->u4_num_sei_payload++;
            }
        }
        else if((*pu4_tag & IHEVCE_COMMANDS_TAG_MASK) == IHEVCE_SYNCH_API_REG_ALLFRAME_SEI_TAG)
        {
            memcpy(
                (void *)((
                    ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload].pu1_sei_payload)),
                (void *)pu1_user_data,
                i4_cmd_len);
            ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload].u4_payload_length =
                (i4_cmd_len);
            ps_curr_out->as_sei_payload[ps_curr_out->u4_num_sei_payload].u4_payload_type =
                ((*pu4_tag & IHEVCE_PAYLOAD_TYPE_MASK) >> IHEVCE_PAYLOAD_TYPE_SHIFT);
            ps_curr_out->u4_num_sei_payload++;
        }

        //The formula (((x-1)>>2)+1) gives us the ceiling of (x mod 4). Hence this will take the pointer to the next address boundary divisible by 4.
        //And then we add 2 bytes for the tag and the payload length.
        if(i4_cmd_len)
            pu4_tag += (((i4_cmd_len - 1) >> 2) + 1 + 2);
        else
            pu4_tag += 2;
    }
}

/*!
******************************************************************************
* \if Function name : ihevce_dyn_bitrate \endif
*
* \brief
*    Call back function to be called for changing the bitrate
*
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dyn_bitrate(void *pv_hle_ctxt, void *pv_dyn_bitrate_prms)
{
    ihevce_hle_ctxt_t *ps_hle_ctxt = (ihevce_hle_ctxt_t *)pv_hle_ctxt;
    ihevce_dyn_config_prms_t *ps_dyn_bitrate_prms = (ihevce_dyn_config_prms_t *)pv_dyn_bitrate_prms;
    enc_ctxt_t *ps_enc_ctxt =
        (enc_ctxt_t *)ps_hle_ctxt->apv_enc_hdl[ps_dyn_bitrate_prms->i4_tgt_res_id];
    ihevce_static_cfg_params_t *ps_static_cfg_params = ps_hle_ctxt->ps_static_cfg_prms;

    if(ps_enc_ctxt->ps_stat_prms->i4_log_dump_level > 0)
    {
        ps_static_cfg_params->s_sys_api.ihevce_printf(
            ps_static_cfg_params->s_sys_api.pv_cb_handle,
            "\n Average Bitrate changed to %d",
            ps_dyn_bitrate_prms->i4_new_tgt_bitrate);
        ps_static_cfg_params->s_sys_api.ihevce_printf(
            ps_static_cfg_params->s_sys_api.pv_cb_handle,
            "\n Peak    Bitrate changed to %d",
            ps_dyn_bitrate_prms->i4_new_peak_bitrate);
    }


    /* acquire mutex lock for rate control calls */
    osal_mutex_lock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);

    ihevce_rc_register_dyn_change_bitrate(
        ps_enc_ctxt->s_module_ctxt.apv_rc_ctxt[ps_dyn_bitrate_prms->i4_tgt_br_id],
        (LWORD64)ps_dyn_bitrate_prms->i4_new_tgt_bitrate,
        (LWORD64)ps_dyn_bitrate_prms->i4_new_peak_bitrate);

    /*unlock rate control context*/
    osal_mutex_unlock(ps_enc_ctxt->pv_rc_mutex_lock_hdl);
    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_validate_encoder_parameters \endif
*
* \brief
*    Call back function to be called for changing the bitrate
*
* \return
*    None
*
* \author
*  Ittiam
*****************************************************************************
*/
WORD32 ihevce_validate_encoder_parameters(ihevce_static_cfg_params_t *ps_static_cfg_prms)
{
    return (ihevce_hle_validate_static_params(ps_static_cfg_prms));
}

/*!
******************************************************************************
* \if Function name : ihevce_get_encoder_version \endif
*
* \brief
*    Call back function to be called for changing the bitrate
*
* \return
*    None
*
* \author
*  Ittiam
*****************************************************************************
*/
const char *ihevce_get_encoder_version()
{
    return ((const char *)gau1_version_string);
}
