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
* \file ihevce_enc_sbh_funcs.c
*
* \brief
*    This file contains utility functions for sbh
*
* \date
*    31/08/2012
*
* \author
*    Ittiam
*
* List of Functions
*    ihevce_sign_data_hiding()
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
#include "ihevc_trans_tables.h"
#include "ihevc_trans_macros.h"

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
#include "ihevce_global_tables.h"
#include "ihevce_enc_sbh_utils.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief
*  This function find the coefficient that needs to be modified for SBH
*  for each sub block, if required
*
* @par Description:
*  Checks the validity for applying SBH
*
* @param[inout] ps_rdoq_sbh_params
*  All the necessary parameters for SBH
*
* @returns  None
*
* @remarks  None
*
********************************************************************************
*/
void ihevce_sign_data_hiding(rdoq_sbh_ctxt_t *ps_rdoq_sbh_params)
{
    WORD32 i, trans_unit_idx;
    UWORD8 *pu1_trans_table = NULL;
    UWORD8 *pu1_csb_table;
    WORD32 shift_value, mask_value;
    WORD32 blk_row, blk_col;

    WORD32 x_pos, y_pos;
    WORD16 i2_quant_coeff;
    WORD32 best_pos = -1;

    WORD16 *pi2_quant_coeffs = ps_rdoq_sbh_params->pi2_quant_coeffs;
    WORD16 *pi2_iquant_data = ps_rdoq_sbh_params->pi2_iquant_coeffs;
    WORD16 *pi2_tr_coeffs = ps_rdoq_sbh_params->pi2_trans_values;
    WORD32 *pi4_subBlock2csbfId_map = ps_rdoq_sbh_params->pi4_subBlock2csbfId_map;
    WORD16 *pi2_dequant_coeff = ps_rdoq_sbh_params->pi2_dequant_coeff;
    UWORD8 *pu1_csbf_buf = ps_rdoq_sbh_params->pu1_csbf_buf;
    WORD32 dst_iq_strd = ps_rdoq_sbh_params->i4_iq_data_strd;
    WORD32 dst_q_strd = ps_rdoq_sbh_params->i4_q_data_strd;

    WORD32 scan_idx = ps_rdoq_sbh_params->i4_scan_idx;
    WORD32 qp_div = ps_rdoq_sbh_params->i4_qp_div;
    WORD32 trans_size = ps_rdoq_sbh_params->i4_trans_size;
    WORD32 qp_rem = ps_rdoq_sbh_params->i2_qp_rem;
    LWORD64 ssd_cost = ps_rdoq_sbh_params->i8_ssd_cost;

    WORD32 last_cg = -1;

    WORD32 log2_size, bit_depth, shift_iq;

    GETRANGE(log2_size, trans_size);
    log2_size -= 1;
    bit_depth = ps_rdoq_sbh_params->i4_bit_depth;
    shift_iq = bit_depth + log2_size - 5;

    /* Select proper order for your transform unit and csb based on scan_idx*/
    /* and the trans_size */

    /* scan order inside a csb */
    pu1_csb_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);

    /* GETRANGE will give the log_2 of trans_size to shift_value */
    GETRANGE(shift_value, trans_size);
    shift_value = shift_value - 3; /* for finding. row no. from scan index */
    mask_value = (trans_size / 4) - 1; /*for finding the col. no. from scan index*/
    switch(trans_size)
    {
    case 32:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_8x8[scan_idx][0]);
        break;
    case 16:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_4x4[scan_idx][0]);
        break;
    case 8:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_2x2[scan_idx][0]);
        break;
    case 4:
        pu1_trans_table = (UWORD8 *)&(g_u1_scan_table_1x1[0]);
        break;
    default:
        ASSERT(0);
        break;
    }
    for(trans_unit_idx = (trans_size * trans_size / 16) - 1; trans_unit_idx >= 0; trans_unit_idx--)
    {
        WORD32 last_scan_pos = -1, first_scan_pos = 16, sign_first_coeff, sum_abs_level = 0,
               quant_coeff_first;

        if(pu1_csbf_buf[pi4_subBlock2csbfId_map[pu1_trans_table[trans_unit_idx]]])
        {
            /* row of csb */
            blk_row = (pu1_trans_table[trans_unit_idx] >> shift_value) * 4;
            /* col of csb */
            blk_col = (pu1_trans_table[trans_unit_idx] & mask_value) * 4;

            if(last_cg == -1)
            {
                last_cg = 1;
            }

            for(i = 15; i >= 0; i--)
            {
                x_pos = (pu1_csb_table[i] & 0x3) + blk_col;
                y_pos = (pu1_csb_table[i] >> 2) + blk_row;

                i2_quant_coeff = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];

                if(i2_quant_coeff)
                {
                    first_scan_pos = i;
                    if(-1 == last_scan_pos)
                    {
                        last_scan_pos = i;
                    }

                    sum_abs_level += abs(i2_quant_coeff);
                }
            }

            if((last_scan_pos - first_scan_pos) >= 4)
            {
                x_pos = (pu1_csb_table[first_scan_pos] & 0x3) + blk_col;
                y_pos = (pu1_csb_table[first_scan_pos] >> 2) + blk_row;

                quant_coeff_first = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];

                sign_first_coeff = (quant_coeff_first > 0) ? 0 : 1;

                if(sign_first_coeff != (sum_abs_level & 0x1))
                {
                    WORD32 q_err;
                    WORD32 min_cost = MAX_INT;
                    WORD32 final_change = 0, cur_cost = 0, cur_change = 0;
                    WORD16 i2_tr_coeff;
                    WORD16 i2_iquant_coeff;

                    for(i = (last_cg == 1) ? last_scan_pos : 15; i >= 0; i--)
                    {
                        x_pos = (pu1_csb_table[i] & 0x3) + blk_col;
                        y_pos = (pu1_csb_table[i] >> 2) + blk_row;

                        i2_quant_coeff = pi2_quant_coeffs[x_pos + (y_pos * trans_size)];
                        i2_tr_coeff = pi2_tr_coeffs[x_pos + (y_pos * trans_size)];
                        i2_iquant_coeff = pi2_iquant_data[x_pos + (y_pos * dst_iq_strd)];

                        q_err = abs(i2_tr_coeff) - abs(i2_iquant_coeff);

                        if(i2_quant_coeff != 0)
                        {
                            cur_cost = -1 * SIGN(q_err) * q_err;

                            if(q_err <= 0)
                            {
                                if(i == first_scan_pos && abs(i2_quant_coeff) == 1)
                                {
                                    cur_cost = MAX_INT;
                                }
                            }
                        }
                        else
                        {
                            cur_cost = -q_err;
                            if(i < first_scan_pos)
                            {
                                WORD32 sign_bit = (i2_tr_coeff >= 0 ? 0 : 1);

                                if(sign_first_coeff != sign_bit)
                                {
                                    cur_cost = MAX_INT;
                                }
                            }
                        }

                        cur_change = (i2_quant_coeff == 0) ? 1 : (q_err > 0 ? 1 : -1);

                        if(cur_cost < min_cost)
                        {
                            min_cost = cur_cost;
                            final_change = cur_change;
                            best_pos = i;
                        }
                    }
                    if((i2_quant_coeff == 32767) || (i2_quant_coeff == -32768))
                    {
                        final_change = -1;
                    }

                    x_pos = (pu1_csb_table[best_pos] & 0x3) + blk_col;
                    y_pos = (pu1_csb_table[best_pos] >> 2) + blk_row;
                    i2_iquant_coeff = pi2_iquant_data[x_pos + (y_pos * dst_iq_strd)];
                    i2_tr_coeff = pi2_tr_coeffs[x_pos + (y_pos * trans_size)];

                    if(i2_tr_coeff >= 0)
                    {
                        pi2_quant_coeffs[x_pos + (y_pos * trans_size)] += final_change;
                    }
                    else
                    {
                        pi2_quant_coeffs[x_pos + (y_pos * trans_size)] -= final_change;
                    }

                    {
                        WORD32 i4_err1, i4_err2;

                        /*  Inverse Quantization    */
                        IQUANT(
                            pi2_iquant_data[y_pos * dst_iq_strd + x_pos],
                            pi2_quant_coeffs[y_pos * dst_q_strd + x_pos],
                            pi2_dequant_coeff[y_pos * trans_size + x_pos] *
                                g_ihevc_iquant_scales[qp_rem],
                            shift_iq,
                            qp_div);

                        i4_err1 = (i2_tr_coeff - i2_iquant_coeff);
                        i4_err1 = i4_err1 * i4_err1;
                        ssd_cost = ssd_cost - i4_err1;
                        i4_err2 = (i2_tr_coeff - pi2_iquant_data[y_pos * dst_iq_strd + x_pos]);
                        i4_err2 = i4_err2 * i4_err2;
                        ssd_cost = ssd_cost + i4_err2;
                    }
                }
            }
            if(last_cg == 1)
            {
                last_cg = 0;
            }
        }
    }

    ps_rdoq_sbh_params->i8_ssd_cost = ssd_cost;
}
