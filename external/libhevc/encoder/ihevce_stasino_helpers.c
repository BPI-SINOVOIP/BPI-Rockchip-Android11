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
*  ihevce_stasino_helpers.c
*
* @brief
*
* @author
*  Ittiam
*
* @par List of Functions:
*
* @remarks
*  None
*
*******************************************************************************
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
#include "ihevce_stasino_helpers.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
*******************************************************************************
*
* @brief
*  This function calculates the variance of given data set.
*
* @par Description:
*  This function is mainly used to find the variance of the block of pixel values.
*  The block can be rectangular also. Single pass variance calculation
*  implementation.
*
* @param[in] p_input
*  The input buffer to calculate the variance.
*
* @param[out] pi4_mean
*  Pointer ot the mean of the datset
*
* @param[out] pi4_variance
*  Pointer tot he variabce of the data set
*
* @param[in] u1_is_hbd
*  1 if the data is in  high bit depth
*
* @param[in] stride
*  Stride for the input buffer
*
* @param[in] block_height
*  height of the pixel block
*
* @param[in] block_width
*  width of the pixel block
*
* @remarks
*  None
*
*******************************************************************************
*/
void ihevce_calc_variance(
    void *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width,
    UWORD8 u1_is_hbd,
    UWORD8 u1_disable_normalization)
{
    UWORD8 *pui1_buffer;  // pointer for 8 bit usecase
    WORD32 i, j;
    WORD32 total_elements;

    LWORD64 mean;
    ULWORD64 variance;
    ULWORD64 sum;
    ULWORD64 sq_sum;

    /* intialisation */
    total_elements = u1_block_height * u1_block_width;
    mean = 0;
    variance = 0;
    sum = 0;
    sq_sum = 0;

    /* handle the case of 8/10 bit depth separately */
    if(!u1_is_hbd)
    {
        pui1_buffer = (UWORD8 *)pv_input;

        /* loop over all the values in the block */
        for(i = 0; i < u1_block_height; i++)
        {
            /* loop over a row in the block */
            for(j = 0; j < u1_block_width; j++)
            {
                sum += pui1_buffer[i * i4_stride + j];
                sq_sum += (pui1_buffer[i * i4_stride + j] * pui1_buffer[i * i4_stride + j]);
            }
        }

        if(!u1_disable_normalization)
        {
            mean = sum / total_elements;
            variance =
                ((total_elements * sq_sum) - (sum * sum)) / (total_elements * (total_elements));
        }
        else
        {
            mean = sum;
            variance = ((total_elements * sq_sum) - (sum * sum));
        }
    }

    /* copy back the values to the output variables */
    *pi4_mean = mean;
    *pu4_variance = variance;
}

/**
*******************************************************************************
*
* @brief
*  This function calcluates the variance of given data set which is WORD16
*
* @par Description:
*  This function is mainly used to find the variance of the block of pixel values.
*  Single pass variance calculation implementation.
*
* @param[in] pv_input
*  The input buffer to calculate the variance.
*
*
* @param[in] stride
*  Stride for the input buffer
*
* @param[out] pi4_mean
*  Pointer ot the mean of the datset
*
* @param[out] pi4_variance
*  Pointer tot he variabce of the data set
*
* @param[in] block_height
*  height of the pixel block
*
* @param[in] block_width
*  width of the pixel block
*
*
* @remarks
*  None
*
*******************************************************************************/
void ihevce_calc_variance_signed(
    WORD16 *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width)
{
    WORD16 *pi2_buffer;  // poinbter for 10 bit use case

    WORD32 i, j;
    WORD32 total_elements;

    LWORD64 mean;
    LWORD64 variance;
    LWORD64 sum;
    LWORD64 sq_sum;

    /* intialisation */
    total_elements = u1_block_height * u1_block_width;
    mean = 0;
    variance = 0;
    sum = 0;
    sq_sum = 0;

    pi2_buffer = pv_input;

    for(i = 0; i < u1_block_height; i++)
    {
        for(j = 0; j < u1_block_width; j++)
        {
            sum += pi2_buffer[i * i4_stride + j];
            sq_sum += (pi2_buffer[i * i4_stride + j] * pi2_buffer[i * i4_stride + j]);
        }
    }

    mean = sum;  /// total_elements;
    variance = ((total_elements * sq_sum) - (sum * sum));  // / (total_elements * (total_elements) )

    /* copy back the values to the output variables */
    *pi4_mean = mean;
    *pu4_variance = variance;
}

/**
*******************************************************************************
*
* @brief
*  This function calculates the variance of a chrominance plane for 420SP data
*
* @par Description:
*  This function is mainly used to find the variance of the block of pixel values.
*  The block can be rectangular also. Single pass variance calculation
*  implementation.
*
* @param[in] p_input
*  The input buffer to calculate the variance.
*
* @param[in] stride
*  Stride for the input buffer
*
* @param[out] pi4_mean
*  Pointer ot the mean of the datset
*
* @param[out] pi4_variance
*  Pointer tot he variabce of the data set
*
* @param[in] block_height
*  height of the pixel block
*
* @param[in] block_width
*  width of the pixel block
*
* @param[in] u1_is_hbd
*  1 if the data is in  high bit depth
*
* @param[in] e_chroma_plane
*  is U or V
*
* @remarks
*  None
*
*******************************************************************************
*/
void ihevce_calc_chroma_variance(
    void *pv_input,
    WORD32 i4_stride,
    WORD32 *pi4_mean,
    UWORD32 *pu4_variance,
    UWORD8 u1_block_height,
    UWORD8 u1_block_width,
    UWORD8 u1_is_hbd,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    UWORD8 *pui1_buffer;  // pointer for 8 bit usecase
    WORD32 i, j;
    WORD32 total_elements;

    LWORD64 mean;
    ULWORD64 variance;
    LWORD64 sum;
    LWORD64 sq_sum;

    /* intialisation */
    total_elements = u1_block_height * u1_block_width;
    mean = 0;
    variance = 0;
    sum = 0;
    sq_sum = 0;

    /* handle the case of 8/10 bit depth separately */
    if(!u1_is_hbd)
    {
        pui1_buffer = (UWORD8 *)pv_input;

        pui1_buffer += e_chroma_plane;

        /* loop over all the values in the block */
        for(i = 0; i < u1_block_height; i++)
        {
            /* loop over a row in the block */
            for(j = 0; j < u1_block_width; j++)
            {
                sum += pui1_buffer[i * i4_stride + j * 2];
                sq_sum += (pui1_buffer[i * i4_stride + j * 2] * pui1_buffer[i * i4_stride + j * 2]);
            }
        }

        mean = sum / total_elements;
        variance = ((total_elements * sq_sum) - (sum * sum)) / (total_elements * (total_elements));
    }

    /* copy back the values to the output variables */
    *pi4_mean = mean;
    *pu4_variance = variance;
}

LWORD64 ihevce_inject_stim_into_distortion(
    void *pv_src,
    WORD32 i4_src_stride,
    void *pv_pred,
    WORD32 i4_pred_stride,
    LWORD64 i8_distortion,
    WORD32 i4_alpha_stim_multiplier,
    UWORD8 u1_blk_size,
    UWORD8 u1_is_hbd,
    UWORD8 u1_enable_psyRDOPT,
    CHROMA_PLANE_ID_T e_chroma_plane)
{
    if(!u1_enable_psyRDOPT)
    {
        UWORD32 u4_src_variance;
        UWORD32 u4_pred_variance;
        WORD32 i4_mean;
        WORD32 i4_noise_term;

        if(NULL_PLANE == e_chroma_plane)
        {
            ihevce_calc_variance(
                pv_src,
                i4_src_stride,
                &i4_mean,
                &u4_src_variance,
                u1_blk_size,
                u1_blk_size,
                u1_is_hbd,
                0);

            ihevce_calc_variance(
                pv_pred,
                i4_pred_stride,
                &i4_mean,
                &u4_pred_variance,
                u1_blk_size,
                u1_blk_size,
                u1_is_hbd,
                0);
        }
        else
        {
            ihevce_calc_chroma_variance(
                pv_src,
                i4_src_stride,
                &i4_mean,
                &u4_src_variance,
                u1_blk_size,
                u1_blk_size,
                u1_is_hbd,
                e_chroma_plane);

            ihevce_calc_chroma_variance(
                pv_pred,
                i4_pred_stride,
                &i4_mean,
                &u4_pred_variance,
                u1_blk_size,
                u1_blk_size,
                u1_is_hbd,
                e_chroma_plane);
        }

        i4_noise_term =
            ihevce_compute_noise_term(i4_alpha_stim_multiplier, u4_src_variance, u4_pred_variance);

        MULTIPLY_STIM_WITH_DISTORTION(i8_distortion, i4_noise_term, STIM_Q_FORMAT, ALPHA_Q_FORMAT);

        return i8_distortion;
    }
    else
    {
        return i8_distortion;
    }
}

UWORD8 ihevce_determine_cu_noise_based_on_8x8Blk_data(
    UWORD8 *pu1_is_8x8Blk_noisy, UWORD8 u1_cu_x_pos, UWORD8 u1_cu_y_pos, UWORD8 u1_cu_size)
{
    UWORD8 u1_num_noisy_children = 0;
    UWORD8 u1_start_index = (u1_cu_x_pos / 8) + u1_cu_y_pos;

    if(8 == u1_cu_size)
    {
        return pu1_is_8x8Blk_noisy[u1_start_index];
    }

    u1_num_noisy_children += ihevce_determine_cu_noise_based_on_8x8Blk_data(
        pu1_is_8x8Blk_noisy, u1_cu_x_pos, u1_cu_y_pos, u1_cu_size / 2);

    u1_num_noisy_children += ihevce_determine_cu_noise_based_on_8x8Blk_data(
        pu1_is_8x8Blk_noisy, u1_cu_x_pos + (u1_cu_size / 2), u1_cu_y_pos, u1_cu_size / 2);

    u1_num_noisy_children += ihevce_determine_cu_noise_based_on_8x8Blk_data(
        pu1_is_8x8Blk_noisy, u1_cu_x_pos, u1_cu_y_pos + (u1_cu_size / 2), u1_cu_size / 2);

    u1_num_noisy_children += ihevce_determine_cu_noise_based_on_8x8Blk_data(
        pu1_is_8x8Blk_noisy,
        u1_cu_x_pos + (u1_cu_size / 2),
        u1_cu_y_pos + (u1_cu_size / 2),
        u1_cu_size / 2);

    return (u1_num_noisy_children >= 2);
}

/*!
******************************************************************************
* \if Function name : ihevce_psy_rd_cost_croma \endif
*
* \brief
*    Calculates the psyco visual cost for RD opt. This is
*
* \param[in] pui4_source_satd
*   This is the pointer to the array of 8x8 satd of the corresponding source CTB. This is pre calculated.
* \param[in] *pui1_recon
*   This si the pointer to the pred data.
* \param[in] recon_stride
*   This si the pred stride
* \param[in] pic_type
*   Picture type.
* \param[in] layer_id
*   Indicates the temporal layer.
* \param[in] lambda
*   This is the weighting factor for the cost.
* \param[in] is_hbd
*   This is the high bit depth flag which indicates if the bit depth of the pixels is 10 bit or 8 bit.
* \param[in] sub_sampling_type
*   This is the chroma subsampling type. 11 - for 420 and 13 for 422
* \return
*    the cost for the psyRDopt
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_psy_rd_cost_croma(
    LWORD64 *pui4_source_satd,
    void *p_recon,
    WORD32 recon_stride_vert,
    WORD32 recond_stride_horz,
    WORD32 cu_size_luma,
    WORD32 pic_type,
    WORD32 layer_id,
    WORD32 lambda,
    WORD32 start_index,
    WORD32 is_hbd,
    WORD32 sub_sampling_type,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    /* declare local variables to store the SATD values for the pred  for the current block. */
    LWORD64 psy_rd_cost;
    UWORD32 lambda_mod;
    WORD32 psy_factor;

    /* declare local variables */
    WORD32 i;
    WORD32 cu_total_size;
    WORD32 num_comp_had_blocks;

    UWORD8 *pu1_l0_block;
    UWORD8 *pu1_l0_block_prev;
    UWORD8 *pu1_recon;
    WORD32 ht_offset;
    WORD32 wd_offset;
    WORD32 cu_ht;
    WORD32 cu_wd;

    WORD32 num_horz_blocks;

    WORD16 pi2_residue_had[64];
    /* this is used as a buffer with all values equal to 0. This is emulate the case with
       pred being zero in HAD fucntion */
    UWORD8 ai1_zeros_buffer[64];

    WORD32 had_block_size;
    LWORD64 source_satd;  // to hold source for current 8x8 block
    LWORD64 recon_satd;  // holds the current recon 8x8 satd

    WORD32 index_for_src_satd;

    (void)recond_stride_horz;
    (void)pic_type;
    (void)layer_id;
    if(!is_hbd)
    {
        pu1_recon = (UWORD8 *)p_recon;
    }

    /**** initialize the variables ****/
    had_block_size = 4;

    if(sub_sampling_type == 1)  // 420
    {
        cu_ht = cu_size_luma / 2;
        cu_wd = cu_size_luma / 2;
    }
    else
    {
        cu_ht = cu_size_luma;
        cu_wd = cu_size_luma / 2;
    }

    num_horz_blocks = 2 * cu_wd / had_block_size;  //ctb_width / had_block_size;
    ht_offset = -had_block_size;
    wd_offset = 0;  //-had_block_size;

    cu_total_size = cu_ht * cu_wd;
    num_comp_had_blocks = 2 * cu_total_size / (had_block_size * had_block_size);

    index_for_src_satd = start_index;

    for(i = 0; i < 64; i++)
    {
        ai1_zeros_buffer[i] = 0;
    }

    psy_factor = PSY_STRENGTH_CHROMA;
    psy_rd_cost = 0;
    lambda_mod = lambda * psy_factor;

    /************************************************************/
    /* loop over for every 4x4 blocks in the CU for Cb */
    for(i = 0; i < num_comp_had_blocks; i++)
    {
        if(i % num_horz_blocks == 0)
        {
            wd_offset = -had_block_size;
            ht_offset += had_block_size;
        }
        wd_offset += had_block_size;

        /* source satd for the current 8x8 block */
        source_satd = pui4_source_satd[index_for_src_satd];

        if(i % 2 != 0)
        {
            if(!is_hbd)
            {
                pu1_l0_block = pu1_l0_block_prev + 1;
            }
        }
        else
        {
            if(!is_hbd)
            {
                /* get memory pointers for each of L0 and L1 blocks whose hadamard has to be computed */
                pu1_l0_block = pu1_recon + recon_stride_vert * ht_offset + wd_offset;
                pu1_l0_block_prev = pu1_l0_block;
            }
        }

        if(had_block_size == 4)
        {
            if(!is_hbd)
            {
                recon_satd = ps_cmn_utils_optimised_function_list->pf_chroma_AC_HAD_4x4_8bit(
                    pu1_l0_block,
                    recon_stride_vert,
                    ai1_zeros_buffer,
                    had_block_size,
                    pi2_residue_had,
                    had_block_size);
            }

            /* get the additional cost function based on the absolute SATD diff of source and recon. */
            psy_rd_cost += (lambda_mod * llabs(source_satd - recon_satd));

            index_for_src_satd++;

            if((i % num_horz_blocks) == (num_horz_blocks - 1))
            {
                index_for_src_satd -= num_horz_blocks;
                index_for_src_satd +=
                    (MAX_CU_SIZE / 8); /* Assuming CTB size = 64 and blocksize = 8 */
            }

        }  // if had block size ==4
    }  // for loop for all 4x4 block in the cu

    psy_rd_cost = psy_rd_cost >> (Q_PSY_STRENGTH_CHROMA + LAMBDA_Q_SHIFT);
    /* reutrn the additional cost for the psy RD opt */
    return (psy_rd_cost);
}

/*!
******************************************************************************
* \if Function name : ihevce_psy_rd_cost \endif
*
* \brief
*    Calculates the psyco visual cost for RD opt. This is
*
* \param[in] pui4_source_satd
*   This is the pointer to the array of 8x8 satd of the corresponding source CTB. This is pre calculated.
* \param[in] *pui1_recon
*   This si the pointer to the pred data.
* \param[in] recon_stride
*   This si the pred stride
* \param[in] pic_type
*   Picture type.
* \param[in] layer_id
*   Indicates the temporal layer.
* \param[in] lambda
*   This is the weighting factor for the cost.
*
* \return
*    the cost for the psyRDopt
*
* \author
*  Ittiam
*
*****************************************************************************
*/
LWORD64 ihevce_psy_rd_cost(
    LWORD64 *pui4_source_satd,
    void *pv_recon,
    WORD32 recon_stride_vert,
    WORD32 recond_stride_horz,
    WORD32 cu_size,
    WORD32 pic_type,
    WORD32 layer_id,
    WORD32 lambda,
    WORD32 start_index,
    WORD32 is_hbd,
    UWORD32 u4_psy_strength,
    ihevce_cmn_opt_func_t *ps_cmn_utils_optimised_function_list)
{
    /* declare local variables to store the SATD values for the pred  for the current block. */
    LWORD64 psy_rd_cost;  // TODO : check if overflow is there.
    UWORD32 lambda_mod;
    WORD32 psy_factor;

    /* declare local variables */
    WORD32 i;
    WORD32 cu_total_size;
    WORD32 num_comp_had_blocks;

    UWORD8 *pu1_l0_block;
    UWORD8 *pu1_recon;

    WORD32 ht_offset;
    WORD32 wd_offset;
    WORD32 cu_ht;
    WORD32 cu_wd;

    WORD32 num_horz_blocks;

    //WORD16 pi2_residue_had[64];
    WORD16 pi2_residue_had_zscan[64];
    //WORD16 pi2_residue[64];
    /* this is used as a buffer with all values equal to 0. This is emulate the case with
       pred being zero in HAD fucntion */
    UWORD8 ai1_zeros_buffer[64];

    WORD32 had_block_size;
    LWORD64 source_satd;  // to hold source for current 8x8 block
    LWORD64 recon_satd;  // holds the current recon 8x8 satd

    WORD32 index_for_src_satd;

    (void)recond_stride_horz;
    (void)pic_type;
    (void)layer_id;
    /***** initialize the variables ****/
    had_block_size = 8;
    cu_ht = cu_size;
    cu_wd = cu_size;

    num_horz_blocks = cu_wd / had_block_size;  //ctb_width / had_block_size;

    ht_offset = -had_block_size;
    wd_offset = 0 - had_block_size;

    cu_total_size = cu_ht * cu_wd;
    num_comp_had_blocks = cu_total_size / (had_block_size * had_block_size);

    index_for_src_satd = start_index;

    for(i = 0; i < 64; i++)
    {
        ai1_zeros_buffer[i] = 0;
    }
    psy_factor = u4_psy_strength;  //PSY_STRENGTH;
    psy_rd_cost = 0;
    lambda_mod = lambda * psy_factor;

    if(!is_hbd)
    {
        pu1_recon = (UWORD8 *)pv_recon;
    }

    /**************************************************************/
    /* loop over for every 8x8 blocks in the CU */
    for(i = 0; i < num_comp_had_blocks; i++)
    {
        if(i % num_horz_blocks == 0)
        {
            wd_offset = -had_block_size;
            ht_offset += had_block_size;
        }
        wd_offset += had_block_size;

        /* source satd for the current 8x8 block */
        source_satd = pui4_source_satd[index_for_src_satd];

        if(had_block_size == 8)
        {
            //WORD32 index;
            //WORD32 u4_satd;
            //WORD32 dst_strd = 8;
            //WORD32 i4_frm_qstep = 0;
            //WORD32 early_cbf;
            if(!is_hbd)
            {
                /* get memory pointers for each of L0 and L1 blocks whose hadamard has to be computed */
                pu1_l0_block = pu1_recon + recon_stride_vert * ht_offset + wd_offset;

                recon_satd = ps_cmn_utils_optimised_function_list->pf_AC_HAD_8x8_8bit(
                    pu1_l0_block,
                    recon_stride_vert,
                    ai1_zeros_buffer,
                    had_block_size,
                    pi2_residue_had_zscan,
                    had_block_size);
            }

            /* get the additional cost function based on the absolute SATD diff of source and recon. */
            psy_rd_cost += (lambda_mod * llabs(source_satd - recon_satd));

            index_for_src_satd++;
            if((i % num_horz_blocks) == (num_horz_blocks - 1))
            {
                index_for_src_satd -= num_horz_blocks;
                index_for_src_satd +=
                    (MAX_CU_SIZE / 8); /* Assuming CTB size = 64 and blocksize = 8 */
            }
        }  // if
    }  // for loop
    psy_rd_cost = psy_rd_cost >> (Q_PSY_STRENGTH + LAMBDA_Q_SHIFT);

    /* reutrn the additional cost for the psy RD opt */
    return (psy_rd_cost);
}

unsigned long ihevce_calc_stim_injected_variance(
    ULWORD64 *pu8_sigmaX,
    ULWORD64 *pu8_sigmaXSquared,
    ULWORD64 *u8_var,
    WORD32 i4_inv_wpred_wt,
    WORD32 i4_inv_wt_shift_val,
    WORD32 i4_wpred_log_wdc,
    WORD32 i4_part_id)
{
    ULWORD64 u8_X_Square, u8_temp_var;
    WORD32 i4_bits_req;

    const WORD32 i4_default_src_wt = ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;

    u8_X_Square = (pu8_sigmaX[i4_part_id] * pu8_sigmaX[i4_part_id]);
    u8_temp_var = pu8_sigmaXSquared[i4_part_id] - u8_X_Square;

    if(i4_inv_wpred_wt != i4_default_src_wt)
    {
        i4_inv_wpred_wt = i4_inv_wpred_wt >> i4_inv_wt_shift_val;

        u8_temp_var = SHR_NEG(
            (u8_temp_var * i4_inv_wpred_wt * i4_inv_wpred_wt),
            (30 - (2 * i4_inv_wt_shift_val) - i4_wpred_log_wdc * 2));
    }

    GETRANGE64(i4_bits_req, u8_temp_var);

    if(i4_bits_req > 27)
    {
        *u8_var = u8_temp_var >> (i4_bits_req - 27);
        return (i4_bits_req - 27);
    }
    else
    {
        *u8_var = u8_temp_var;
        return 0;
    }
}

unsigned long ihevce_calc_variance_for_diff_weights(
    ULWORD64 *pu8_sigmaX,
    ULWORD64 *pu8_sigmaXSquared,
    ULWORD64 *u8_var,
    WORD32 *pi4_inv_wt,
    WORD32 *pi4_inv_wt_shift_val,
    pu_result_t *ps_result,
    WORD32 i4_wpred_log_wdc,
    PART_ID_T *pe_part_id,
    UWORD8 u1_cu_size,
    UWORD8 u1_num_parts,
    UWORD8 u1_is_for_src)
{
    WORD32 i4_k;
    UWORD32 u4_wd, u4_ht;
    UWORD8 u1_num_base_blks;
    UWORD32 u4_num_pixels_in_part;
    UWORD8 u1_index;
    WORD32 i4_bits_req;

    UWORD8 u1_base_blk_size = 4;
    UWORD32 u4_tot_num_pixels = u1_cu_size * u1_cu_size;
    ULWORD64 u8_temp_sigmaX[MAX_NUM_INTER_PARTS] = { 0, 0 };
    ULWORD64 u8_temp_sigmaXsquared[MAX_NUM_INTER_PARTS] = { 0, 0 };
    ULWORD64 u8_z;

    const WORD32 i4_default_src_wt = ((1 << 15) + (WGHT_DEFAULT >> 1)) / WGHT_DEFAULT;

    for(i4_k = 0; i4_k < u1_num_parts; i4_k++)
    {
        u4_wd = ps_result[i4_k].pu.b4_wd + 1;
        u4_ht = ps_result[i4_k].pu.b4_ht + 1;
        u1_num_base_blks = u4_wd * u4_ht;
        u4_num_pixels_in_part = u1_num_base_blks * u1_base_blk_size * u1_base_blk_size;

        if(u1_is_for_src)
        {
            u1_index = pe_part_id[i4_k];
        }
        else
        {
            u1_index = i4_k;
        }

        u8_temp_sigmaXsquared[i4_k] = pu8_sigmaXSquared[u1_index] / u4_num_pixels_in_part;
        u8_temp_sigmaX[i4_k] = pu8_sigmaX[u1_index];

        if(u1_is_for_src)
        {
            if(pi4_inv_wt[i4_k] != i4_default_src_wt)
            {
                pi4_inv_wt[i4_k] = pi4_inv_wt[i4_k] >> pi4_inv_wt_shift_val[i4_k];
                u8_temp_sigmaX[i4_k] = SHR_NEG(
                    (u8_temp_sigmaX[i4_k] * pi4_inv_wt[i4_k]),
                    (15 - pi4_inv_wt_shift_val[i4_k] - i4_wpred_log_wdc));
                u8_temp_sigmaXsquared[i4_k] = SHR_NEG(
                    (u8_temp_sigmaXsquared[i4_k] * pi4_inv_wt[i4_k] * pi4_inv_wt[i4_k]),
                    (30 - (2 * pi4_inv_wt_shift_val[i4_k]) - i4_wpred_log_wdc * 2));
            }
        }
    }

    u8_z = (u4_tot_num_pixels * (u8_temp_sigmaXsquared[0] + u8_temp_sigmaXsquared[1])) -
           ((u8_temp_sigmaX[0] + u8_temp_sigmaX[1]) * (u8_temp_sigmaX[0] + u8_temp_sigmaX[1]));

    GETRANGE64(i4_bits_req, u8_z);

    if(i4_bits_req > 27)
    {
        *u8_var = u8_z >> (i4_bits_req - 27);
        return (i4_bits_req - 27);
    }
    else
    {
        *u8_var = u8_z;
        return 0;
    }
}
