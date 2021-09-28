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
* \file ihevce_dep_mngr_interface.h
*
* \brief
*    This file contains infertace prototypes of Sync manager functions
*
* \date
*    13/12/2013
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_DEPENDENCY_MANAGER_INTERFACE_H_
#define _IHEVCE_DEPENDENCY_MANAGER_INTERFACE_H_

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

typedef enum
{
    DEP_MNGR_FRM_FRM_SYNC = 0, /*!< To be used for multi threads Frame-
                                      Frame level sync, where threads entering
                                      a particular frame processing stage at
                                      a particular index waits for all the threads
                                      to complete the that stage at the same index
                                      in the previous  iteration
                                      Ex: Wait for Encloop at Index i frame
                                      to complete before starting encloop of
                                      MAX_NUM_ENCLOOP + i frame at Index i
                                      (FRAME LEVEl SYNCS)*/

    DEP_MNGR_ROW_FRM_SYNC, /*!< To be used for multi threads Row-
                                      Frame level sync, where multiple threads
                                      entering a particular frame processing stage at
                                      a particular index waits for corresponding
                                      row to be completely processed in the
                                      dependent stage
                                      Ex: Multiple threads Wait in ME at a
                                      particular row X (of Frame I)
                                      until encloop of row X in Frame I
                                      is completed
                                      (REVERSE ME DEPENDENCY SYNC)*/

    DEP_MNGR_ROW_ROW_SYNC, /*!< To be used for multi threads Row-
                                      Row level sync, where a thread
                                      entering a particular frame processing stage at
                                      a particular index waits for corresponding
                                      row to be processed til dependent position
                                      in the dependent stage
                                      Ex: (ENC LOOP to ME FORWARD Sync)
                                          ( TOP RIGHT SYNC) */

    DEP_MNGR_MAP_SYNC

} DEP_MNGR_MODE_T;

typedef enum
{
    MAP_CTB_INIT = 0,
    MAP_CTB_RECON_DONE = 1,
    MAP_CTB_COMPLETE = 2,  //after hpel plane creation

} DEP_MNGR_MAP_CTB_STATUS_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

/* Create APIs */
WORD32 ihevce_dmgr_get_num_mem_recs(void);

WORD32 ihevce_dmgr_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    WORD32 dep_mngr_mode, /* should be part of DEP_MNGR_MODE_T*/
    WORD32 max_num_vert_units,
    WORD32 num_tile_cols,
    WORD32 num_threads,
    WORD32 i4_mem_space);

WORD32 ihevce_dmgr_map_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 num_units, WORD32 num_threads, WORD32 i4_mem_space);

void *ihevce_dmgr_init(
    iv_mem_rec_t *ps_mem_tab,
    void *pv_osal_handle,
    WORD32 dep_mngr_mode, /* should be part of DEP_MNGR_MODE_T*/
    WORD32 max_num_vert_units,
    WORD32 max_num_horz_units,
    WORD32 num_tile_cols,
    WORD32 num_threads,
    WORD32 sem_enable);

void *ihevce_dmgr_map_init(
    iv_mem_rec_t *ps_mem_tab,
    WORD32 max_num_vert_units,
    WORD32 max_num_horz_units,
    WORD32 sem_enable,
    WORD32 num_threads,
    WORD32 ai4_tile_xtra_ctb[4]);

void ihevce_dmgr_reg_sem_hdls(
    void *pv_dep_mngr_state, void **ppv_thread_sem_hdl, WORD32 num_threads);

/* Row-Row sync Process APIs*/
void ihevce_dmgr_rst_row_row_sync(void *pv_dep_mngr_state);

WORD32 ihevce_dmgr_chk_row_row_sync(
    void *pv_dep_mngr_state,
    WORD32 cur_offset,
    WORD32 dep_offset,
    WORD32 dep_row,
    WORD32 cur_tile_col,
    WORD32 thrd_id);

WORD32 ihevce_dmgr_set_row_row_sync(
    void *pv_dep_mngr_state, WORD32 cur_offset, WORD32 cur_row, WORD32 cur_tile_col);

/* Row-Frame sync Process APIs*/
void ihevce_dmgr_rst_row_frm_sync(void *pv_dep_mngr_state);

/* Frame-Frame sync Process APIs*/
void ihevce_dmgr_set_done_frm_frm_sync(void *pv_dep_mngr_state);

void ihevce_dmgr_set_prev_done_frm_frm_sync(void *pv_dep_mngr_state);

WORD32 ihevce_dmgr_chk_frm_frm_sync(void *pv_dep_mngr_state, WORD32 thrd_id);

WORD32 ihevce_dmgr_update_frm_frm_sync(void *pv_dep_mngr_state);

/* Map sync Process APIs*/
void ihevce_dmgr_map_rst_sync(void *pv_dep_mngr_state);

WORD32 ihevce_dmgr_map_chk_sync(
    void *pv_dep_mngr_state,
    WORD32 thrd_id,
    WORD32 offset_x,
    WORD32 offset_y,
    WORD32 i4_sr_ctb_x,
    WORD32 i4_sr_ctb_y);

WORD32 ihevce_dmgr_map_set_sync(
    void *pv_dep_mngr_state, WORD32 offset_x, WORD32 offset_y, WORD32 e_map_value);

/* Delete APIs */
void ihevce_dmgr_del(void *pv_dep_mngr_state);

#endif /* _IHEVCE_DEPENDENCY_MANAGER_INTERFACE_H_ */
