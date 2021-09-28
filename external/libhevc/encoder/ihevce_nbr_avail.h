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
* \file ihevce_nbr_avail.h
*
* \brief
*    This file contains function prototypes of neihbour acces related funcs
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_NBR_AVAIL_H_
#define _IHEVCE_NBR_AVAIL_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

void ihevce_set_ctb_nbr(
    nbr_avail_flags_t *ps_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 ctb_pos_x,
    WORD32 ctb_pos_y,
    frm_ctb_ctxt_t *ps_frm_ctb_prms);

WORD32 ihevce_get_nbr_intra(
    nbr_avail_flags_t *ps_cu_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size);

void ihevce_get_only_nbr_flag(
    nbr_avail_flags_t *ps_cu_nbr,
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_hz,
    WORD32 unit_4x4_size_vt);

void ihevce_set_nbr_map(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size,
    WORD32 val);
void ihevce_set_inter_nbr_map(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_hz,
    WORD32 unit_4x4_size_vt,
    WORD32 val);

WORD32 ihevce_get_nbr_intra_mxn_tu(
    UWORD8 *pu1_nbr_map,
    WORD32 nbr_map_strd,
    WORD32 unit_4x4_pos_x,
    WORD32 unit_4x4_pos_y,
    WORD32 unit_4x4_size_horz,
    WORD32 unit_4x4_size_vert);

WORD32 ihevce_get_intra_chroma_tu_nbr(
    WORD32 i4_luma_nbr_flags, WORD32 i4_subtu_idx, WORD32 i4_trans_size, UWORD8 u1_is_422);

#endif /* _IHEVCE_NBR_AVAIL_H_ */
