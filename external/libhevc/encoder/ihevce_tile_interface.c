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
* \file ihevce_tile_interface.c
*
* \brief
*    This file contains functions related to tile interface
*
* \date
*    24/10/2012
*
* \author
*    Ittiam
*
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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
#include "ihevce_tile_interface.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_update_tile_params \endif
*
* \brief
*    Updates the ps_tile_params structres based on the tile-position in frame.
*
*****************************************************************************
*/
void ihevce_update_tile_params(
    ihevce_static_cfg_params_t *ps_static_cfg_prms,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 i4_resolution_id)
{
    /* Total number of tiles in a frame */
    ihevce_app_tile_params_t *ps_app_tile_prms;
    WORD32 i4_num_tiles;
    WORD32 i4_cu_aligned_tgt_frame_ht,
        i4_cu_aligned_tgt_frame_wd;  //Frame width and height specific to target-resolution
    WORD32 i4_ctb_aligned_tgt_frame_ht,
        i4_ctb_aligned_tgt_frame_wd;  //Frame width and height specific to target-resolution
    WORD32 i4_x_y = 0;
    WORD32 i4_pos;
    WORD32 i4_i;

    WORD32 i4_curr_tile_id;
    WORD32 i4_max_log2_cu_size, i4_ctb_size;
    WORD32 i4_pic_wd_in_ctb;
    WORD32 i4_pic_ht_in_ctb;
    WORD32 min_cu_size;
    WORD32 i4_num_tile_cols = 1;
    WORD32 i4_num_tile_rows = 1;

    ps_app_tile_prms = &ps_static_cfg_prms->s_app_tile_params;

    i4_max_log2_cu_size = ps_static_cfg_prms->s_config_prms.i4_max_log2_cu_size;
    i4_ctb_size = 1 << i4_max_log2_cu_size;

    min_cu_size = 1 << ps_static_cfg_prms->s_config_prms.i4_min_log2_cu_size;

    /* Allign the frame width to min CU size */
    i4_cu_aligned_tgt_frame_wd =
        ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width +
        SET_CTB_ALIGN(
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width,
            min_cu_size);

    /* Allign the frame hieght to min CU size */
    i4_cu_aligned_tgt_frame_ht =
        ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height +
        SET_CTB_ALIGN(
            ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height,
            min_cu_size);

    if(1 == ps_app_tile_prms->i4_tiles_enabled_flag)
    {
        i4_num_tile_cols = ps_app_tile_prms->i4_num_tile_cols;
        i4_num_tile_rows = ps_app_tile_prms->i4_num_tile_rows;
    }

    i4_num_tiles = i4_num_tile_cols * i4_num_tile_rows;

    i4_ctb_aligned_tgt_frame_wd = i4_cu_aligned_tgt_frame_wd;
    i4_ctb_aligned_tgt_frame_wd += SET_CTB_ALIGN(i4_ctb_aligned_tgt_frame_wd, MAX_CTB_SIZE);
    i4_pic_wd_in_ctb = i4_ctb_aligned_tgt_frame_wd >> i4_max_log2_cu_size;

    i4_ctb_aligned_tgt_frame_ht = i4_cu_aligned_tgt_frame_ht;
    i4_ctb_aligned_tgt_frame_ht += SET_CTB_ALIGN(i4_ctb_aligned_tgt_frame_ht, MAX_CTB_SIZE);
    i4_pic_ht_in_ctb = i4_ctb_aligned_tgt_frame_ht >> i4_max_log2_cu_size;

    /* Update tile enable flag in each instance's tile struct */
    ps_tile_params->i4_tiles_enabled_flag = ps_app_tile_prms->i4_tiles_enabled_flag;

    ps_tile_params->i4_num_tile_cols = i4_num_tile_cols;
    ps_tile_params->i4_num_tile_rows = i4_num_tile_rows;

    i4_curr_tile_id = ps_tile_params->i4_curr_tile_id;

    /* num tiles in frame */
    ps_tile_params->i4_num_tiles = i4_num_tiles;

    ps_tile_params->i4_uniform_spacing_flag = ps_app_tile_prms->i4_uniform_spacing_flag;

    if(0 == ps_tile_params->i4_tiles_enabled_flag)
    {
        /* curr tile width and height */
        ps_tile_params->i4_curr_tile_width = i4_cu_aligned_tgt_frame_wd;
        ps_tile_params->i4_curr_tile_height = i4_cu_aligned_tgt_frame_ht;

        ps_tile_params->i4_first_ctb_x = 0;
        ps_tile_params->i4_first_ctb_y = 0;

        ps_tile_params->i4_first_sample_x = 0;
        ps_tile_params->i4_first_sample_y = 0;
    }
    else
    {
        if(0 == ps_app_tile_prms->i4_uniform_spacing_flag)
        {
            /* curr tile width */
            ps_tile_params->i4_curr_tile_width =
                ps_app_tile_prms->ai4_column_width[i4_curr_tile_id % i4_num_tile_cols];

            /* curr tile height */
            ps_tile_params->i4_curr_tile_height =
                ps_app_tile_prms->ai4_row_height[i4_curr_tile_id / i4_num_tile_cols];

            /* ctb_x and ctb_y of first ctb in tile */
            i4_pos = i4_curr_tile_id % i4_num_tile_cols;

            for(i4_i = 0; i4_i < i4_pos; i4_i++)
            {
                i4_x_y += ps_app_tile_prms->ai4_column_width[i4_i];
            }

            ps_tile_params->i4_first_sample_x = i4_x_y;
            ps_tile_params->i4_first_ctb_x = i4_x_y >> i4_max_log2_cu_size;

            i4_pos = i4_curr_tile_id / i4_num_tile_cols;

            i4_x_y = 0;

            for(i4_i = 0; i4_i < i4_pos; i4_i++)
            {
                i4_x_y += ps_app_tile_prms->ai4_row_height[i4_i];
            }

            ps_tile_params->i4_first_sample_y = i4_x_y;
            ps_tile_params->i4_first_ctb_y = i4_x_y >> i4_max_log2_cu_size;
        }
        else
        {
            /* below formula for tile width/height and start_x/start_y are derived from HM Decoder */
            WORD32 i4_start = 0;
            WORD32 i4_value = 0;
            /* curr tile width */
            for(i4_i = 0; i4_i < i4_num_tile_cols; i4_i++)
            {
                i4_value = ((i4_i + 1) * i4_pic_wd_in_ctb) / i4_num_tile_cols -
                           (i4_i * i4_pic_wd_in_ctb) / i4_num_tile_cols;

                if(i4_i == (i4_curr_tile_id % i4_num_tile_cols))
                {
                    ps_tile_params->i4_first_ctb_x = i4_start;
                    ps_tile_params->i4_first_sample_x = (i4_start << i4_max_log2_cu_size);
                    ps_tile_params->i4_curr_tile_width = (i4_value << i4_max_log2_cu_size);
                    if(i4_i == (i4_num_tile_cols - 1))
                    {
                        if(i4_cu_aligned_tgt_frame_wd % i4_ctb_size)
                        {
                            ps_tile_params->i4_curr_tile_width =
                                (ps_tile_params->i4_curr_tile_width - i4_ctb_size) +
                                (i4_cu_aligned_tgt_frame_wd % i4_ctb_size);
                        }
                    }
                    break;
                }
                i4_start += i4_value;
            }

            /* curr tile height */
            i4_start = 0;
            for(i4_i = 0; i4_i < i4_num_tile_rows; i4_i++)
            {
                i4_value = ((i4_i + 1) * i4_pic_ht_in_ctb) / i4_num_tile_rows -
                           (i4_i * i4_pic_ht_in_ctb) / i4_num_tile_rows;

                if(i4_i == (i4_curr_tile_id / i4_num_tile_cols))
                {
                    ps_tile_params->i4_first_ctb_y = i4_start;
                    ps_tile_params->i4_first_sample_y = (i4_start << i4_max_log2_cu_size);
                    ps_tile_params->i4_curr_tile_height = (i4_value << i4_max_log2_cu_size);
                    if(i4_i == (i4_num_tile_rows - 1))
                    {
                        if(i4_cu_aligned_tgt_frame_ht % i4_ctb_size)
                        {
                            ps_tile_params->i4_curr_tile_height =
                                (ps_tile_params->i4_curr_tile_height - i4_ctb_size) +
                                (i4_cu_aligned_tgt_frame_ht % i4_ctb_size);
                        }
                    }
                    break;
                }
                i4_start += i4_value;
            }
        }
    }

    /* Initiallize i4_curr_tile_wd_in_ctb_unit and i4_curr_tile_ht_in_ctb_unit */
    ps_tile_params->i4_curr_tile_wd_in_ctb_unit =
        ps_tile_params->i4_curr_tile_width +
        SET_CTB_ALIGN(ps_tile_params->i4_curr_tile_width, i4_ctb_size);

    ps_tile_params->i4_curr_tile_ht_in_ctb_unit =
        ps_tile_params->i4_curr_tile_height +
        SET_CTB_ALIGN(ps_tile_params->i4_curr_tile_height, i4_ctb_size);

    ps_tile_params->i4_curr_tile_wd_in_ctb_unit /= i4_ctb_size;
    ps_tile_params->i4_curr_tile_ht_in_ctb_unit /= i4_ctb_size;
}

/*!
******************************************************************************
* \if Function name : ihevce_tiles_get_num_mem_recs \endif
*
* \brief
*   Returns the total no. of memory records needed for tile encoding
*
* \param
*   None
*
* \return
*   total no. of memory required
*
* \author
*   Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_tiles_get_num_mem_recs(void)
{
    WORD32 i4_total_memtabs_req = 0;

    /*------------------------------------------------------------------*/
    /* Get number of memtabs                                            */
    /*------------------------------------------------------------------*/
    /* Memory for keeping all tile's parameters */
    i4_total_memtabs_req++;

    /* Memory for keeping frame level tile_id map */
    i4_total_memtabs_req++;

    return (i4_total_memtabs_req);
}

/*!
******************************************************************************
* \if Function name : ihevce_tiles_get_mem_recs \endif
*
* \brief
*   Fills each memory record attributes of tiles
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_tile_master_prms : master tile params
* \param[in] i4_mem_space : memspace in whihc memory request should be done
*
* \return
*   total no. of mem records filled
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_tiles_get_mem_recs(
    iv_mem_rec_t *ps_memtab,
    ihevce_static_cfg_params_t *ps_static_cfg_params,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 i4_resolution_id,
    WORD32 i4_mem_space)
{
    //WORD32  i4_frame_width, i4_frame_height;
    WORD32 i4_num_tiles;
    WORD32 i4_total_memtabs_filled = 0;
    WORD32 i4_num_tile_cols = 1;
    WORD32 i4_num_tile_rows = 1;
    WORD32 ctb_aligned_frame_width, ctb_aligned_frame_height;
    WORD32 u4_ctb_in_a_row, u4_ctb_rows_in_a_frame;

    ihevce_app_tile_params_t *ps_app_tile_params = &ps_static_cfg_params->s_app_tile_params;
    /*
    i4_frame_width  = ps_tile_master_prms->i4_frame_width;
    i4_frame_height = ps_tile_master_prms->i4_frame_height;*/

    if(1 == ps_app_tile_params->i4_tiles_enabled_flag)
    {
        i4_num_tile_cols = ps_app_tile_params->i4_num_tile_cols;
        i4_num_tile_rows = ps_app_tile_params->i4_num_tile_rows;
    }

    i4_num_tiles = i4_num_tile_cols * i4_num_tile_rows;

    /* -------- Memory for storing all tile params ---------*/
    ps_memtab[0].i4_size = sizeof(iv_mem_rec_t);
    ps_memtab[0].i4_mem_size = i4_num_tiles * sizeof(ihevce_tile_params_t);
    ps_memtab[0].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_memtab[0].i4_mem_alignment = 8;
    i4_total_memtabs_filled++;

    /* -------- Memory for CTB level tile-id map ---------*/
    ctb_aligned_frame_width =
        ps_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
    ctb_aligned_frame_height =
        ps_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height;

    /*making the width and height a multiple of CTB size*/
    ctb_aligned_frame_width += SET_CTB_ALIGN(
        ps_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width,
        MAX_CTB_SIZE);
    ctb_aligned_frame_height += SET_CTB_ALIGN(
        ps_static_cfg_params->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_height,
        MAX_CTB_SIZE);

    u4_ctb_in_a_row = (ctb_aligned_frame_width / MAX_CTB_SIZE);
    u4_ctb_rows_in_a_frame = (ctb_aligned_frame_height / MAX_CTB_SIZE);

    ps_frm_ctb_prms->i4_tile_id_ctb_map_stride = (ctb_aligned_frame_width / MAX_CTB_SIZE);

    /* Memory for a frame level memory to store tile-id corresponding to each CTB of frame*/
    /* (u4_ctb_in_a_row + 1): Keeping an extra column on the left. Tile Id's will be set to -1 in it */
    /* (u4_ctb_rows_in_a_frame + 1): Keeping an extra column on the top. Tile Id's will be set to -1 in it */
    /*  -1   -1  -1  -1  -1  -1  -1 ....... -1  -1
        -1    0   0   1   1   2   2 .......  M  -1
        -1    0   0   1   1   2   2 .......  M  -1
        ..   ..  ..  ..  ..  ..  .. .......  M  -1
        ..   ..  ..  ..  ..  ..  .. .......  M  -1
        -1  N   N   N+1 N+1 N+2 N+2 ....... N+M -1
    */
    ps_memtab[1].i4_size = sizeof(iv_mem_rec_t);
    ps_memtab[1].i4_mem_size =
        (1 + u4_ctb_in_a_row + 1) * (1 + u4_ctb_rows_in_a_frame) * sizeof(WORD32);
    ps_memtab[1].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_memtab[1].i4_mem_alignment = 8;
    i4_total_memtabs_filled++;

    return (i4_total_memtabs_filled);
}

/*!
******************************************************************************
* \if Function name : ihevce_tiles_mem_init \endif
*
* \brief
*   Initialization of shared buffer memories
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] ps_tile_master_prms : master tile params
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_tiles_mem_init(
    iv_mem_rec_t *ps_memtab,
    ihevce_static_cfg_params_t *ps_static_cfg_prms,
    enc_ctxt_t *ps_enc_ctxt,
    WORD32 i4_resolution_id)
{
    WORD32 i4_num_tiles, tile_ctr;
    WORD32 ctb_row_ctr, ctb_col_ctr, i;
    WORD32 tile_pos_x, tile_pos_y;
    WORD32 tile_wd_in_ctb, tile_ht_in_ctb;
    WORD32 *pi4_tile_id_map_temp, *pi4_tile_id_map_base;
    WORD32 frame_width_in_ctb;
    WORD32 i4_num_tile_cols = 1;
    WORD32 i4_num_tile_rows = 1;

    ihevce_tile_params_t *ps_tile_params_base;
    frm_ctb_ctxt_t *ps_frm_ctb_prms = &ps_enc_ctxt->s_frm_ctb_prms;

    if(1 == ps_static_cfg_prms->s_app_tile_params.i4_tiles_enabled_flag)
    {
        i4_num_tile_cols = ps_static_cfg_prms->s_app_tile_params.i4_num_tile_cols;
        i4_num_tile_rows = ps_static_cfg_prms->s_app_tile_params.i4_num_tile_rows;
    }

    frame_width_in_ctb =
        ps_static_cfg_prms->s_tgt_lyr_prms.as_tgt_params[i4_resolution_id].i4_width;
    frame_width_in_ctb += SET_CTB_ALIGN(frame_width_in_ctb, MAX_CTB_SIZE);
    frame_width_in_ctb /= MAX_CTB_SIZE;

    /* -------- Memory for storing all tile params ---------*/
    ps_tile_params_base = (ihevce_tile_params_t *)ps_memtab->pv_base;
    ps_memtab++;

    i4_num_tiles = i4_num_tile_cols * i4_num_tile_rows;

    for(tile_ctr = 0; tile_ctr < i4_num_tiles; tile_ctr++)
    {
        WORD32 i4_i;
        ihevce_tile_params_t *ps_tile_params = (ps_tile_params_base + tile_ctr);

        /* Setting default values */
        memset(ps_tile_params, 0, sizeof(ihevce_tile_params_t));

        ps_tile_params->i4_curr_tile_id = tile_ctr; /* tile id */

        /* update create time tile params in each encoder context */
        ihevce_update_tile_params(ps_static_cfg_prms, ps_tile_params, i4_resolution_id);

        if(0 == ps_static_cfg_prms->s_app_tile_params.i4_uniform_spacing_flag)
        {
            /* Storing column width array and row height array inro enc ctxt */
            for(i4_i = 0; i4_i < i4_num_tile_cols; i4_i++)
            {
                ps_enc_ctxt->ai4_column_width_array[i4_i] =
                    ps_static_cfg_prms->s_app_tile_params.ai4_column_width[i4_i];
            }
            for(i4_i = 0; i4_i < i4_num_tile_rows; i4_i++)
            {
                ps_enc_ctxt->ai4_row_height_array[i4_i] =
                    ps_static_cfg_prms->s_app_tile_params.ai4_row_height[i4_i];
            }
        }
    }

    /* -------- Memory for CTB level tile-id map ---------*/
    pi4_tile_id_map_base = (WORD32 *)ps_memtab->pv_base;

    // An extra col and row at top, left and right aroun frame level memory. Is set to -1.
    ps_frm_ctb_prms->i4_tile_id_ctb_map_stride = frame_width_in_ctb + 2;
    ps_frm_ctb_prms->pi4_tile_id_map =
        pi4_tile_id_map_base + ps_frm_ctb_prms->i4_tile_id_ctb_map_stride + 1;
    ps_memtab++;

    /* Filling -1 in top row */
    for(i = 0; i < ps_frm_ctb_prms->i4_tile_id_ctb_map_stride; i++)
    {
        pi4_tile_id_map_base[i] = -1;
    }

    /* Now creating tile-id map */
    for(tile_ctr = 0; tile_ctr < ps_tile_params_base->i4_num_tiles; tile_ctr++)
    {
        ihevce_tile_params_t *ps_tile_params = ps_tile_params_base + tile_ctr;

        tile_pos_x = ps_tile_params->i4_first_ctb_x;
        tile_pos_y = ps_tile_params->i4_first_ctb_y;
        tile_wd_in_ctb = ps_tile_params->i4_curr_tile_wd_in_ctb_unit;
        tile_ht_in_ctb = ps_tile_params->i4_curr_tile_ht_in_ctb_unit;

        pi4_tile_id_map_temp = ps_frm_ctb_prms->pi4_tile_id_map +
                               tile_pos_y * ps_frm_ctb_prms->i4_tile_id_ctb_map_stride + tile_pos_x;

        for(ctb_row_ctr = 0; (ctb_row_ctr < tile_ht_in_ctb); ctb_row_ctr++)
        {
            if(tile_pos_x == 0)
            { /* Filling -1 in left column */
                pi4_tile_id_map_temp[-1] = -1;
            }

            for(ctb_col_ctr = 0; (ctb_col_ctr < tile_wd_in_ctb); ctb_col_ctr++)
            {
                pi4_tile_id_map_temp[ctb_col_ctr] = tile_ctr;
            }

            if(frame_width_in_ctb == (tile_pos_x + tile_wd_in_ctb))
            { /* Filling -1 in right column */
                pi4_tile_id_map_temp[tile_wd_in_ctb] = -1;
            }

            pi4_tile_id_map_temp += ps_frm_ctb_prms->i4_tile_id_ctb_map_stride;
        }
    }

    return (void *)ps_tile_params_base;
}

/*!
******************************************************************************
* \if Function name : update_last_coded_cu_qp \endif
*
* \brief Update i1_last_cu_qp based on CTB's position in tile
*
* \param[in]  pi1_top_last_cu_qp
*             Pointer to the CTB row's Qp storage
* \param[in]  i1_entropy_coding_sync_enabled_flag
*             flag to indicate rate control mode
* \param[in]  ps_frm_ctb_prms
*             Frame ctb parameters
* \param[in]  i1_frame_qp
*             Frame qp
* \param[in]  vert_ctr
*             first CTB row of frame
* \param[in]  ctb_ctr
*             ct row count
* \param[out]  pi1_last_cu_qp
*             Qp of the last CU of previous CTB row
*
* \return
*  None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void update_last_coded_cu_qp(
    WORD8 *pi1_top_last_cu_qp,
    WORD8 i1_entropy_coding_sync_enabled_flag,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD8 i1_frame_qp,
    WORD32 vert_ctr,
    WORD32 ctb_ctr,
    WORD8 *pi1_last_cu_qp)
{
    WORD32 i4_curr_ctb_tile_id, i4_left_ctb_tile_id, i4_top_ctb_tile_id;
    WORD32 *pi4_tile_id_map_temp;

    pi4_tile_id_map_temp = ps_frm_ctb_prms->pi4_tile_id_map +
                           vert_ctr * ps_frm_ctb_prms->i4_tile_id_ctb_map_stride + ctb_ctr;

    i4_curr_ctb_tile_id = *(pi4_tile_id_map_temp);
    i4_left_ctb_tile_id = *(pi4_tile_id_map_temp - 1);
    i4_top_ctb_tile_id = *(pi4_tile_id_map_temp - ps_frm_ctb_prms->i4_tile_id_ctb_map_stride);

    if(i4_curr_ctb_tile_id == i4_left_ctb_tile_id)
    {
        return;
    }
    else if(i4_curr_ctb_tile_id != i4_top_ctb_tile_id)
    { /* First CTB of tile */
        *pi1_last_cu_qp = i1_frame_qp;
    }
    else
    { /* First CTB of CTB-row */
        if(1 == i1_entropy_coding_sync_enabled_flag)
        {
            *pi1_last_cu_qp = i1_frame_qp;
        }
        else
        {
            *pi1_last_cu_qp = *(pi1_top_last_cu_qp);
        }
    }
}
