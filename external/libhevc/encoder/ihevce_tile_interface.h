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
* \file ihevce_tile_interface.h
*
* \brief
*   This file contains functions prototypes, constants, enumerations and
*   structures related to tile interface
*
* \date
*   03 july 2012
*
* \author
*   Ittiam
*
*
* List of Functions
*
*
******************************************************************************
*/

#ifndef _IHEVCE_TILE_INTERFACE_H_
#define _IHEVCE_TILE_INTERFACE_H_

/****************************************************************************/
/* Function Prototypes                                                      */
/****************************************************************************/
void ihevce_update_tile_params(
    ihevce_static_cfg_params_t *ps_static_cfg_prms,
    ihevce_tile_params_t *ps_tile_params,
    WORD32 i4_resolution_id);

WORD32 ihevce_tiles_get_num_mem_recs(void);

WORD32 ihevce_tiles_get_mem_recs(
    iv_mem_rec_t *ps_memtab,
    ihevce_static_cfg_params_t *ps_static_cfg_params,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD32 i4_resolution_id,
    WORD32 i4_mem_space);

void *ihevce_tiles_mem_init(
    iv_mem_rec_t *ps_memtab,
    ihevce_static_cfg_params_t *ps_static_cfg_prms,
    enc_ctxt_t *ps_enc_ctxt,
    WORD32 i4_resolution_id);

void update_last_coded_cu_qp(
    WORD8 *pi1_ctb_row_qp,
    WORD8 i1_entropy_coding_sync_enabled_flag,
    frm_ctb_ctxt_t *ps_frm_ctb_prms,
    WORD8 i1_frame_qp,
    WORD32 vert_ctr,
    WORD32 ctb_ctr,
    WORD8 *pi1_last_cu_qp);

#endif  //_IHEVCE_TILE_INTERFACE_H_
