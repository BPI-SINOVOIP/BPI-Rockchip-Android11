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
* @file ihevce_nbr_avail.c
*
* @brief
*    This file contains function definitions and look up tables for various
*    neigbour avail flags in HEVC encoder
*
* @author
*    Ittiam
*
* List of Functions
*   <TODO: TO BE ADDED>
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
#include "ihevce_multi_thrd_funcs.h"
#include "ihevce_me_common_defs.h"
#include "ihevce_had_satd.h"
#include "ihevce_error_codes.h"
#include "ihevce_bitstream.h"
#include "ihevce_cabac.h"
#include "ihevce_rdoq_macros.h"
#include "ihevce_function_selector.h"
#include "ihevce_enc_structs.h"
#include "ihevce_nbr_avail.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_set_ctb_nbr \endif
*
* \brief
*    This function sets the neighbour availability flags of ctb based on the
*    CTB position
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
void ihevce_set_ctb_nbr(
    nbr_avail_flags_t *ps_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 ctb_pos_x,
    WORD32 ctb_pos_y,
    frm_ctb_ctxt_t *ps_frm_ctb_prms)
{
    WORD32 ctr;
    WORD32 *pi4_cur_ctb_tile_id;
    WORD32 i4_curr_ctb_tile_id, i4_top_ctb_tile_id;
    WORD32 i4_left_ctb_tile_id, i4_right_ctb_tile_id;

    WORD32 ctb_size = ps_frm_ctb_prms->i4_ctb_size;
    WORD32 num_ctb_horz = ps_frm_ctb_prms->i4_num_ctbs_horz;
    WORD32 num_ctb_vert = ps_frm_ctb_prms->i4_num_ctbs_vert;
    WORD32 cu_aligned_pic_wd = ps_frm_ctb_prms->i4_cu_aligned_pic_wd;
    WORD32 cu_aligned_pic_ht = ps_frm_ctb_prms->i4_cu_aligned_pic_ht;
    UWORD8 *pu1_top_nbr_map = pu1_nbr_map - nbr_map_strd;
    UWORD8 *pu1_left_nbr_map = pu1_nbr_map - 1;
    UWORD8 *pu1_top_lt_nbr_map = pu1_top_nbr_map - 1;
    UWORD8 *pu1_top_rt_nbr_map = pu1_top_nbr_map + (ctb_size >> 2);
    WORD32 num_4x4_ctb_x = (ctb_size >> 2);
    WORD32 num_4x4_ctb_y = (ctb_size >> 2);

    /* Conditionally update num_4x4_ctb_x and num_4x4_ctb_y */
    if(ctb_pos_y == (num_ctb_vert - 1))
    {
        num_4x4_ctb_y = (cu_aligned_pic_ht - ((num_ctb_vert - 1) * ctb_size)) / 4;
    }

    if(ctb_pos_x == (num_ctb_horz - 1))
    {
        num_4x4_ctb_x = (cu_aligned_pic_wd - ((num_ctb_horz - 1) * ctb_size)) / 4;
    }

    /* Get Tile-ids of top, left and current CTBs */
    pi4_cur_ctb_tile_id = ps_frm_ctb_prms->pi4_tile_id_map +
                          ctb_pos_y * ps_frm_ctb_prms->i4_tile_id_ctb_map_stride + ctb_pos_x;

    i4_curr_ctb_tile_id = *pi4_cur_ctb_tile_id;
    i4_left_ctb_tile_id = *(pi4_cur_ctb_tile_id - 1);
    i4_right_ctb_tile_id = *(pi4_cur_ctb_tile_id + 1);
    i4_top_ctb_tile_id = *(pi4_cur_ctb_tile_id - ps_frm_ctb_prms->i4_tile_id_ctb_map_stride);

    /*********** Update Nbr availability in ps_nbr **********/

    ps_nbr->u1_left_avail = (i4_left_ctb_tile_id == i4_curr_ctb_tile_id);
    ps_nbr->u1_top_avail = (i4_top_ctb_tile_id == i4_curr_ctb_tile_id);
    ps_nbr->u1_top_lt_avail = (ps_nbr->u1_left_avail && ps_nbr->u1_top_avail);
    ps_nbr->u1_top_rt_avail = ps_nbr->u1_top_avail && (i4_right_ctb_tile_id == i4_curr_ctb_tile_id);
    ps_nbr->u1_bot_lt_avail = 0; /* at ctb level bottom left is always not available */

    /*********** Update Nbr availability in pu1_nbr_map **********/

    /* NOTE: entire Nbr availability map is by default set to 0 */
    *pu1_top_lt_nbr_map = ps_nbr->u1_top_lt_avail; /* Top-Left*/

    memset(pu1_top_nbr_map, ps_nbr->u1_top_avail, num_4x4_ctb_x); /* Top */

    for(ctr = 0; ctr < num_4x4_ctb_y; ctr++) /* Left */
    {
        *pu1_left_nbr_map = ps_nbr->u1_left_avail;
        pu1_left_nbr_map += nbr_map_strd;
    }

    if((num_ctb_horz - 2) == ctb_pos_x) /* Top-Right */
    {
        /* For the last but 1 ctb, if the last ctb is non-multiple of 64,
       then set the map accordingly */
        WORD32 last_ctb_x = cu_aligned_pic_wd - ((num_ctb_horz - 1) * ctb_size);

        num_4x4_ctb_x = MIN(last_ctb_x, MAX_TU_SIZE) / 4;
        memset(pu1_top_rt_nbr_map, ps_nbr->u1_top_rt_avail, num_4x4_ctb_x);
    }
    else
    {
        memset(pu1_top_rt_nbr_map, ps_nbr->u1_top_rt_avail, (MAX_TU_SIZE / 4));
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_nbr_intra \endif
*
* \brief
*    This function sets the neighbour availability flags of given unit
*    based on the position and size
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
WORD32 ihevce_get_nbr_intra(
    nbr_avail_flags_t *ps_cu_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size)
{
    WORD32 nbr_tem_flags = 0;
    WORD32 i;

    UWORD8 *pu1_bot_lt_map;
    UWORD8 *pu1_top_rt_map;
    UWORD8 *pu1_top_lt_map;
    UWORD8 *pu1_left_map;
    UWORD8 *pu1_top_map;

    /* map is stored at 4x4 level increment to point to current cu 4x4 */
    pu1_nbr_map += (unit_4x4_pos_x);
    pu1_nbr_map += (unit_4x4_pos_y)*nbr_map_strd;

    pu1_top_map = pu1_nbr_map - nbr_map_strd;
    pu1_top_lt_map = pu1_top_map - 1;
    pu1_left_map = (pu1_nbr_map - 1);
    /* use map to get top right availablility */
    pu1_top_rt_map = pu1_nbr_map - nbr_map_strd;
    pu1_top_rt_map += unit_4x4_size;

    /* use map to get bot left availablility */
    pu1_bot_lt_map = pu1_nbr_map - 1;
    pu1_bot_lt_map += unit_4x4_size * nbr_map_strd;

    /* Top flag */
    ps_cu_nbr->u1_top_avail = *pu1_top_map;

    /* left flag */
    ps_cu_nbr->u1_left_avail = *pu1_left_map;

    /* top left flag */
    ps_cu_nbr->u1_top_lt_avail = *pu1_top_lt_map;

    /* top right flag */
    ps_cu_nbr->u1_top_rt_avail = *pu1_top_rt_map;

    /* bottom left flag */
    ps_cu_nbr->u1_bot_lt_avail = (*pu1_bot_lt_map);

    /* Update the neighbor availiblity flag according to the nbr_map */
    nbr_tem_flags = 0;

    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_bot_lt_map) << (3 - i));
        pu1_bot_lt_map += (nbr_map_strd * 2);
    }

    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_left_map) << (7 - i));
        pu1_left_map += (nbr_map_strd * 2);
    }
    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_top_map) << (i + 8));
        pu1_top_map += 2;
    }
    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_top_rt_map) << (i + 12));
        pu1_top_rt_map += 2;
    }
    nbr_tem_flags |= (*pu1_top_lt_map << 16);

    return nbr_tem_flags;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_nbr_intra_mxn_tu \endif
*
* \brief
*    This function sets the neighbour availability flags of given unit
*    based on the position and size
*
* \date
*    24/06/2014
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
WORD32 ihevce_get_nbr_intra_mxn_tu(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_horz,
    WORD32 unit_4x4_size_vert)
{
    WORD32 nbr_tem_flags = 0;
    WORD32 i;

    UWORD8 *pu1_bot_lt_map;
    UWORD8 *pu1_top_rt_map;
    UWORD8 *pu1_top_lt_map;
    UWORD8 *pu1_left_map;
    UWORD8 *pu1_top_map;

    /* map is stored at 4x4 level increment to point to current cu 4x4 */
    pu1_nbr_map += (unit_4x4_pos_x);
    pu1_nbr_map += (unit_4x4_pos_y)*nbr_map_strd;

    pu1_top_map = pu1_nbr_map - nbr_map_strd;
    pu1_top_lt_map = pu1_top_map - 1;
    pu1_left_map = (pu1_nbr_map - 1);
    /* use map to get top right availablility */
    pu1_top_rt_map = pu1_nbr_map - nbr_map_strd;
    pu1_top_rt_map += unit_4x4_size_horz;

    /* use map to get bot left availablility */
    pu1_bot_lt_map = pu1_nbr_map - 1;
    pu1_bot_lt_map += unit_4x4_size_vert * nbr_map_strd;

    /* Update the neighbor availiblity flag according to the nbr_map */
    nbr_tem_flags = 0;

    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_bot_lt_map) << (3 - i));
        pu1_bot_lt_map += (nbr_map_strd * 2);
    }

    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_left_map) << (7 - i));
        pu1_left_map += (nbr_map_strd * 2);
    }
    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_top_map) << (i + 8));
        pu1_top_map += 2;
    }
    for(i = 0; i < 4; i++)
    {
        nbr_tem_flags |= ((*pu1_top_rt_map) << (i + 12));
        pu1_top_rt_map += 2;
    }
    nbr_tem_flags |= (*pu1_top_lt_map << 16);

    return nbr_tem_flags;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_intra_chroma_tu_nbr \endif
*
* \brief
*    This function sets the neighbour availability flags of a chroma
*    subTU based on luma availability and chroma format
*
* \date
*    04/07/2014
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
WORD32 ihevce_get_intra_chroma_tu_nbr(
    WORD32 i4_luma_nbr_flags, WORD32 i4_subtu_idx, WORD32 i4_trans_size, UWORD8 u1_is_422)
{
    /* TOP LEFT | TOP-RIGHT |  TOP   | LEFT    | BOTTOM LEFT*/
    /* (1 bit)     (4 bits)  (4 bits) (4 bits)  (4 bits)  */
    /* With reference to the above bit arrangement - */
    /* BL0 - Bit 3 */
    /* BL1 - Bit 2 */
    /* BL2 - Bit 1 */
    /* BL3 - Bit 0 */
    /* L0 - Bit 7 */
    /* L1 - Bit 6 */
    /* L2 - Bit 5 */
    /* L3 - Bit 4 */
    /* T0 - Bit 8 */
    /* T1 - Bit 9 */
    /* T2 - Bit 10 */
    /* T3 - Bit 11 */
    /* TR0 - Bit 12 */
    /* TR1 - Bit 13 */
    /* TR2 - Bit 14 */
    /* TR3 - Bit 15 */
    if(u1_is_422)
    {
        if(0 == i4_subtu_idx)
        {
            /* If left is available for luma, then */
            if(i4_luma_nbr_flags & 0xf0)
            {
                switch(i4_trans_size)
                {
                case 4:
                {
                    /* BL0 - 1 */
                    /* BL1-3 - Luma_BL0-2 */
                    /*i4_luma_nbr_flags |= (i4_luma_nbr_flags & 0xe) >> 1;*/
                    i4_luma_nbr_flags |= 0x8;

                    /* L0-1 - 11 */
                    /* L2-3 - Luma_L2-3 */
                    i4_luma_nbr_flags |= 0xc0;

                    break;
                }
                case 8:
                {
                    /* BL0-1 - 11 */
                    /* BL1-3 - Luma_BL0-1 */
                    /*i4_luma_nbr_flags |= (i4_luma_nbr_flags & 0xc) >> 2;*/
                    i4_luma_nbr_flags |= 0xc;

                    /* L0-3 - 1111 */
                    i4_luma_nbr_flags |= 0xf0;

                    break;
                }
                case 16:
                {
                    /* BL0-3 - 1111 */
                    i4_luma_nbr_flags |= 0xf;

                    /* L0-3 - 1111 */
                    i4_luma_nbr_flags |= 0xf0;

                    break;
                }
                }
            }
        }
        else
        {
            /* Top right is always unavailable */
            /* Top is always available */

            i4_luma_nbr_flags &= (0xffff0fff);

            /* Top left is marked as available if */
            /* luma left is available */
            if(i4_luma_nbr_flags & 0xf0)
            {
                i4_luma_nbr_flags |= (1 << 16);
            }

            switch(i4_trans_size)
            {
            case 4:
            {
                /* T0 - 1 */
                /* T1-3 - 000 */
                i4_luma_nbr_flags |= 0x100;
                i4_luma_nbr_flags &= 0xfffff1ff;

                if(i4_luma_nbr_flags & 0xf0)
                {
                    i4_luma_nbr_flags |= 0x80;
                }

                if(i4_luma_nbr_flags & 0x8)
                {
                    i4_luma_nbr_flags |= 0x8;
                }

                break;
            }
            case 8:
            {
                /* T0-1 - 11 */
                /* T2-3 - 00 */
                i4_luma_nbr_flags |= 0x300;
                i4_luma_nbr_flags &= 0xfffff3ff;

                if(i4_luma_nbr_flags & 0xf0)
                {
                    i4_luma_nbr_flags |= 0xc0;
                }

                if((i4_luma_nbr_flags & 0xc) == 0x8)
                {
                    i4_luma_nbr_flags |= 0xc;
                }
                else if((i4_luma_nbr_flags & 0xc) == 0xc)
                {
                    i4_luma_nbr_flags |= 0xf;
                }
                else if((i4_luma_nbr_flags & 0xf) == 0xe)
                {
                    i4_luma_nbr_flags |= 0xf;
                }

                break;
            }
            case 16:
            {
                /* T0-3 - 1111 */
                i4_luma_nbr_flags |= 0xf00;

                if(i4_luma_nbr_flags & 0xf0)
                {
                    i4_luma_nbr_flags |= 0xf0;
                }

                if((i4_luma_nbr_flags & 0xf) == 0x8)
                {
                    i4_luma_nbr_flags |= 0xc;
                }
                else if((i4_luma_nbr_flags & 0xf) == 0xc)
                {
                    i4_luma_nbr_flags |= 0xf;
                }
                else if((i4_luma_nbr_flags & 0xf) == 0xe)
                {
                    i4_luma_nbr_flags |= 0xf;
                }

                break;
            }
            }
        }
    }

    return i4_luma_nbr_flags;
}

/*!
******************************************************************************
* \if Function name : ihevce_get_only_nbr_flag \endif
*
* \brief
*    This function sets the neighbour availability flags of given unit
*    based on the position, unit width and unit height
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
void ihevce_get_only_nbr_flag(
    nbr_avail_flags_t *ps_cu_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_hz,
    WORD32 unit_4x4_size_vt)
{
    /* map is stored at 4x4 level increment to point to current cu 4x4 */
    pu1_nbr_map += (unit_4x4_pos_x);
    pu1_nbr_map += (unit_4x4_pos_y)*nbr_map_strd;

    /* Top flag */
    ps_cu_nbr->u1_top_avail = *(pu1_nbr_map - nbr_map_strd);

    /* left flag */
    ps_cu_nbr->u1_left_avail = *(pu1_nbr_map - 1);

    /* top left flag */
    ps_cu_nbr->u1_top_lt_avail = *(pu1_nbr_map - nbr_map_strd - 1);

    /* top right flag */
    {
        UWORD8 *pu1_top_rt_map;
        /* use map to get top right availablility */
        pu1_top_rt_map = pu1_nbr_map - nbr_map_strd;
        pu1_top_rt_map += unit_4x4_size_hz;

        /* store the availbility */
        ps_cu_nbr->u1_top_rt_avail = *pu1_top_rt_map;
    }

    /* bottom left flag */
    {
        UWORD8 *pu1_bot_lt_map;

        /* use map to get bot left availablility */
        pu1_bot_lt_map = pu1_nbr_map - 1;
        pu1_bot_lt_map += unit_4x4_size_vt * nbr_map_strd;

        /* store the availbility */
        ps_cu_nbr->u1_bot_lt_avail = *pu1_bot_lt_map;
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_set_nbr_map \endif
*
* \brief
*    This function sets the neighbour availability flags of given value
*    based on the position and size
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*    none
*
******************************************************************************
*/
void ihevce_set_nbr_map(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size,
    WORD32 val)
{
    WORD32 i;
    /* map is stored at 4x4 level increment to point to current cu 4x4 */
    pu1_nbr_map += (unit_4x4_pos_x);
    pu1_nbr_map += (unit_4x4_pos_y)*nbr_map_strd;

    /* loops to set the flags for given size */
    for(i = 0; i < unit_4x4_size; i++)
    {
        memset(pu1_nbr_map, val, sizeof(UWORD8) * unit_4x4_size);
        /* row level updates */
        pu1_nbr_map += nbr_map_strd;
    }

    return;
}
/*!
******************************************************************************
* \if Function name : ihevce_set_inter_nbr_map \endif
*
* \brief
*    This function sets the neighbour availability flags of given value
*    based on the position and horizontal width and vertical height
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
* \return
*  none
* List of Functions
*
*
******************************************************************************
*/
void ihevce_set_inter_nbr_map(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_hz,
    WORD32 unit_4x4_size_vt,
    WORD32 val)
{
    WORD32 i;
    /* map is stored at 4x4 level increment to point to current cu 4x4 */
    pu1_nbr_map += (unit_4x4_pos_x);
    pu1_nbr_map += (unit_4x4_pos_y)*nbr_map_strd;
    {
        /* loops to set the flags for given size */
        for(i = 0; i < unit_4x4_size_vt; i++)
        {
            memset(pu1_nbr_map, val, sizeof(UWORD8) * unit_4x4_size_hz);
            /* row level updates */
            pu1_nbr_map += nbr_map_strd;
        }
    }

    return;
}
