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
* \file ihevce_dep_mngr.c
*
* \brief
*    This file contains all the functions related to Sync manager
*
* \date
*    12/12/2013
*
* \author
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
#include "ihevc_debug.h"
#include "ihevc_macros.h"
#include "ihevc_platform_macros.h"

#include "ihevce_api.h"
#include "ihevce_dep_mngr_interface.h"
#include "ihevce_dep_mngr_private.h"

#include "cast_types.h"
#include "osal.h"
#include "osal_defaults.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_get_num_mem_recs \endif
*
* \brief
*    Number of memory records are returned for Dependency manager.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_dmgr_get_num_mem_recs()
{
    return (NUM_DEP_MNGR_MEM_RECS);
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for Dependency manager.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] dep_mngr_mode : Mode of operation of dependency manager
* \param[in] max_num_vert_units : Maximum nunber of units to be processed
* \param[in] num_tile_cols : Number of column tiles for which encoder is working
* \param[in] num_threads : Number of threads among which sync will be established
* \param[in] i4_mem_space : memspace in which memory request should be done
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_dmgr_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab,
    WORD32 dep_mngr_mode,
    WORD32 max_num_vert_units,
    WORD32 num_tile_cols,
    WORD32 num_threads,
    WORD32 i4_mem_space)
{
    WORD32 num_vert_units;
    WORD32 num_wait_thrd_ids;

    /* Dependency manager state structure */
    ps_mem_tab[DEP_MNGR_CTXT].i4_mem_size = sizeof(dep_mngr_state_t);
    ps_mem_tab[DEP_MNGR_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_CTXT].i4_mem_alignment = 8;

    /* SANITY CHECK */
    ASSERT(
        (DEP_MNGR_FRM_FRM_SYNC == dep_mngr_mode) || (DEP_MNGR_ROW_FRM_SYNC == dep_mngr_mode) ||
        (DEP_MNGR_ROW_ROW_SYNC == dep_mngr_mode));

    /* Default value */
    if(num_tile_cols < 1)
    {
        num_tile_cols = 1;
    }

    /**************** Get Processed status Memory Requirements *********************/
    if(DEP_MNGR_FRM_FRM_SYNC == dep_mngr_mode)
    {
        /* for frame to frame sync
           2 words are used for holding num units processed prev
           2 words are used for holding num units processed curr
           */
        num_vert_units = (2 + 2) * num_threads;
    }
    else
    {
        /* for both frm-row and row-row num vertical units in frame is allocated */
        /* (* num_tile_cols) as each column tile can separately update and check */
        num_vert_units = max_num_vert_units * num_tile_cols;
    }

    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].i4_mem_size = (sizeof(WORD32) * num_vert_units);
    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].i4_mem_alignment = 8;

    /**************** Get Wait thread ids Memory Requirements *********************/
    if(DEP_MNGR_FRM_FRM_SYNC == dep_mngr_mode)
    {
        /* for frame to frame sync number of threads worth memory is allocated */
        num_wait_thrd_ids = num_threads;
    }
    else if(DEP_MNGR_ROW_ROW_SYNC == dep_mngr_mode)
    {
        /* for row to row sync number of vertical rows worth memory is allocated */
        num_wait_thrd_ids = max_num_vert_units;
    }
    else
    {
        /* for row to frame sync number of threads * number of vertical rows worth memory is allocated */
        num_wait_thrd_ids = max_num_vert_units * num_threads;
    }

    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].i4_mem_size = (sizeof(WORD32) * num_wait_thrd_ids);
    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].i4_mem_alignment = 8;

    /**************** Get Semaphore Requirements *********************/
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].i4_mem_size = (sizeof(void *) * num_threads);
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].i4_mem_alignment = 8;

    return (NUM_DEP_MNGR_MEM_RECS);
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_map_get_mem_recs \endif
*
* \brief
*    Memory requirements are returned for Dependency manager.
*
* \param[in,out]  ps_mem_tab : pointer to memory descriptors table
* \param[in] num_units : Number of units in the map
* \param[in] num_threads : Number of threads among which sync will be established
* \param[in] i4_mem_space : memspace in which memory request should be done
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
WORD32 ihevce_dmgr_map_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 num_units, WORD32 num_threads, WORD32 i4_mem_space)
{
    /* Dependency manager state structure */
    ps_mem_tab[DEP_MNGR_CTXT].i4_mem_size = sizeof(dep_mngr_state_t);
    ps_mem_tab[DEP_MNGR_CTXT].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_CTXT].i4_mem_alignment = 8;

    /**************** Get Processed status Memory Requirements *********************/
    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].i4_mem_size = (sizeof(WORD8) * num_units);
    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].i4_mem_alignment = 8;

    /**************** Get Wait thread ids Memory Requirements *********************/
    /* Map-mode: semaphore post is unconditionally done on all threads */
    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].i4_mem_size = (sizeof(WORD32) * num_threads);
    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].i4_mem_alignment = 8;

    /**************** Get Semaphore Requirements *********************/
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].i4_mem_size = (sizeof(void *) * num_threads);
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].e_mem_type = (IV_MEM_TYPE_T)i4_mem_space;
    ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].i4_mem_alignment = 8;

    return (NUM_DEP_MNGR_MEM_RECS);
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_rst_frm_frm_sync \endif
*
* \brief
*    Resets the values stored to init value
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_rst_frm_frm_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 thrds;
    ULWORD64 *pu8_num_units_proc_prev;
    ULWORD64 *pu8_num_units_proc_curr;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Reset the num units processed by each thread */
    pu8_num_units_proc_curr = (ULWORD64 *)ps_dep_mngr_state->pv_units_prcsd_in_row;
    pu8_num_units_proc_prev = pu8_num_units_proc_curr + ps_dep_mngr_state->i4_num_thrds;

    /* Reset the values thread ids waiting */
    for(thrds = 0; thrds < ps_dep_mngr_state->i4_num_thrds; thrds++)
    {
        pu8_num_units_proc_prev[thrds] = 0;
        pu8_num_units_proc_curr[thrds] = 0;
        ps_dep_mngr_state->pi4_wait_thrd_id[thrds] = -1;
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_rst_row_frm_sync \endif
*
* \brief
*    Resets the values stored to init value
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_rst_row_frm_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 ctr, thrds;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Reset the values of number of units processed in a row */
    for(ctr = 0; ctr < ps_dep_mngr_state->i4_num_vert_units; ctr++)
    {
        ((WORD32 *)ps_dep_mngr_state->pv_units_prcsd_in_row)[ctr] = 0;
    }

    /* Reset the values thread ids waiting on each row  */
    for(ctr = 0; ctr < ps_dep_mngr_state->i4_num_vert_units; ctr++)
    {
        for(thrds = 0; thrds < ps_dep_mngr_state->i4_num_thrds; thrds++)
        {
            ps_dep_mngr_state->pi4_wait_thrd_id[thrds + (ps_dep_mngr_state->i4_num_thrds * ctr)] =
                -1;
        }
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_map_rst_sync \endif
*
* \brief
*    Resets the values stored to init value
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_map_rst_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD8 *pi1_ptr;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    pi1_ptr = (WORD8 *)ps_dep_mngr_state->pv_units_prcsd_in_row -
              ps_dep_mngr_state->ai4_tile_xtra_ctb[0] * ps_dep_mngr_state->i4_num_horz_units -
              ps_dep_mngr_state->ai4_tile_xtra_ctb[1];

    memset(
        pi1_ptr,
        MAP_CTB_INIT,
        ps_dep_mngr_state->i4_num_vert_units * ps_dep_mngr_state->i4_num_horz_units *
            sizeof(WORD8));

    //ps_dep_mngr_state->i4_frame_map_complete = 0;

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_rst_row_row_sync \endif
*
* \brief
*    Resets the values stored to init value
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_rst_row_row_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 ctr;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Reset the values of number of units processed in a row */
    for(ctr = 0; ctr < (ps_dep_mngr_state->i4_num_vert_units * ps_dep_mngr_state->i4_num_tile_cols);
        ctr++)
    {
        ((WORD32 *)ps_dep_mngr_state->pv_units_prcsd_in_row)[ctr] = 0;
    }

    /* Reset the values thread ids waiting on each row  */
    for(ctr = 0; ctr < ps_dep_mngr_state->i4_num_vert_units; ctr++)
    {
        ps_dep_mngr_state->pi4_wait_thrd_id[ctr] = -1;
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_init \endif
*
* \brief
*    Intialization for Dependency manager state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] pv_osal_handle : osal handle
* \param[in] dep_mngr_mode : Mode of operation of dependency manager
* \param[in] max_num_vert_units : Maximum nunber of units to be processed (Frame Data)
* \param[in] max_num_horz_units : Maximun Number of Horizontal units to be processed (Frame Data)
* \param[in] num_tile_cols : Number of column tiles for which encoder is working
* \param[in] sem_enable : Whether you want to enable semaphore or not
             1 : Sem. Enabled, 0 : Spin lock enabled (do-while)
* \param[in] num_threads : Number of threads among which sync will be established
* \param[in] i4_mem_space : memspace in which memory request should be do
*
* \return
*    Handle to context
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_dmgr_init(
    iv_mem_rec_t *ps_mem_tab,
    void *pv_osal_handle,
    WORD32 dep_mngr_mode,
    WORD32 max_num_vert_units,
    WORD32 max_num_horz_units,
    WORD32 num_tile_cols,
    WORD32 num_threads,
    WORD32 sem_enable)
{
    dep_mngr_state_t *ps_dep_mngr_state;

    (void)pv_osal_handle;
    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)ps_mem_tab[DEP_MNGR_CTXT].pv_base;

    /* dep manager memory init */
    ps_dep_mngr_state->ppv_thrd_sem_handles = (void **)ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].pv_base;
    ps_dep_mngr_state->pi4_wait_thrd_id = (WORD32 *)ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].pv_base;
    ps_dep_mngr_state->pv_units_prcsd_in_row = ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].pv_base;

    /* SANITY CHECK */
    ASSERT(NULL != pv_osal_handle);
    ASSERT(
        (DEP_MNGR_FRM_FRM_SYNC == dep_mngr_mode) || (DEP_MNGR_ROW_FRM_SYNC == dep_mngr_mode) ||
        (DEP_MNGR_ROW_ROW_SYNC == dep_mngr_mode));

    /* Default value */
    if(num_tile_cols < 1)
    {
        num_tile_cols = 1;
    }

    /* reset the state structure variables */
    ps_dep_mngr_state->i4_num_horz_units = max_num_horz_units;
    ps_dep_mngr_state->i4_num_vert_units = max_num_vert_units;
    ps_dep_mngr_state->i1_sem_enable = sem_enable;
    ps_dep_mngr_state->i4_dep_mngr_mode = dep_mngr_mode;
    ps_dep_mngr_state->i4_num_thrds = num_threads;
    ps_dep_mngr_state->i4_num_tile_cols = num_tile_cols;

    /* call the reset function baed on mode */
    if(DEP_MNGR_FRM_FRM_SYNC == dep_mngr_mode)
    {
        ihevce_dmgr_rst_frm_frm_sync((void *)ps_dep_mngr_state);
    }
    else if(DEP_MNGR_ROW_ROW_SYNC == dep_mngr_mode)
    {
        ihevce_dmgr_rst_row_row_sync((void *)ps_dep_mngr_state);
    }
    else
    {
        ihevce_dmgr_rst_row_frm_sync((void *)ps_dep_mngr_state);
    }

    return ((void *)ps_dep_mngr_state);
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_map_init \endif
*
* \brief
*    Intialization for Dependency manager state structure .
*
* \param[in] ps_mem_tab : pointer to memory descriptors table
* \param[in] max_num_vert_units : Maximum nunber of units to be processed
* \param[in] max_num_horz_units : Maximun Number of Horizontal units to be processed
* \param[in] sem_enable : Whether you want to enable semaphore or not
             1 : Sem. Enabled, 0 : Spin lock enabled (do-while)
* \param[in] num_threads : Number of threads among which sync will be established
* \param[in] ai4_tile_xtra_ctb : Array containing the number of CTBs which are
*            are present in the Search Range outside the tile in dist-client mode.
*            In standalone mode this array should be zero.
*
* \return
*    Handle to context
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void *ihevce_dmgr_map_init(
    iv_mem_rec_t *ps_mem_tab,
    WORD32 max_num_vert_units,
    WORD32 max_num_horz_units,
    WORD32 sem_enable,
    WORD32 num_threads,
    WORD32 ai4_tile_xtra_ctb[4])
{
    WORD32 ctr;
    dep_mngr_state_t *ps_dep_mngr_state;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)ps_mem_tab[DEP_MNGR_CTXT].pv_base;

    ps_dep_mngr_state->ai4_tile_xtra_ctb[0] = ai4_tile_xtra_ctb[0];
    ps_dep_mngr_state->ai4_tile_xtra_ctb[1] = ai4_tile_xtra_ctb[1];
    ps_dep_mngr_state->ai4_tile_xtra_ctb[2] = ai4_tile_xtra_ctb[2];
    ps_dep_mngr_state->ai4_tile_xtra_ctb[3] = ai4_tile_xtra_ctb[3];

    /* dep manager memory init */
    ps_dep_mngr_state->pv_units_prcsd_in_row = ps_mem_tab[DEP_MNGR_UNITS_PRCSD_MEM].pv_base;
    ps_dep_mngr_state->pi4_wait_thrd_id = (WORD32 *)ps_mem_tab[DEP_MNGR_WAIT_THRD_ID_MEM].pv_base;
    ps_dep_mngr_state->ppv_thrd_sem_handles = (void **)ps_mem_tab[DEP_MNGR_SEM_HANDLE_MEM].pv_base;

    /* Pointing to first CTB of tile */
    ps_dep_mngr_state->pv_units_prcsd_in_row =
        (void*)((WORD8*)ps_dep_mngr_state->pv_units_prcsd_in_row +
        ps_dep_mngr_state->ai4_tile_xtra_ctb[1] +
        max_num_horz_units * ps_dep_mngr_state->ai4_tile_xtra_ctb[0]);

    /* Map-mode: semaphore post is unconditionally done on all threads. Hence
    store these one time IDs. The use of pi4_wait_thrd_id itself can be removed
    altogether for map-mode, but keeping it for the sake of laziness  */
    for(ctr = 0; ctr < num_threads; ctr++)
    {
        ps_dep_mngr_state->pi4_wait_thrd_id[ctr] = ctr;
    }

    /* reset the state structure variables */
    ps_dep_mngr_state->i4_num_horz_units = max_num_horz_units;
    ps_dep_mngr_state->i4_num_vert_units = max_num_vert_units;
    ps_dep_mngr_state->i1_sem_enable = sem_enable;
    ps_dep_mngr_state->i4_dep_mngr_mode = DEP_MNGR_MAP_SYNC;
    ps_dep_mngr_state->i4_num_thrds = num_threads;

    /* call the reset function baed on mode */
    ihevce_dmgr_map_rst_sync((void *)ps_dep_mngr_state);

    return ((void *)ps_dep_mngr_state);
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_del \endif
*
* \brief
*    Delete the Dependency manager state structure.
* Note : Destroys the mutex only. System has to free the allocated memory
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_del(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;

    /* dep manager state structure */
    (void)ps_dep_mngr_state;
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_register_sem_hdls \endif
*
* \brief
*    Register sem handles of threads wihci are part of dependency group
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in] ppv_thread_sem_hdl : arry of pointer to all the sem handles
* \param[in] num_threads : Number of threads part of this dependency group
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_reg_sem_hdls(void *pv_dep_mngr_state, void **ppv_thread_sem_hdl, WORD32 num_threads)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 ctr;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    ASSERT(num_threads <= ps_dep_mngr_state->i4_num_thrds);

    for(ctr = 0; ctr < num_threads; ctr++)
    {
        ps_dep_mngr_state->ppv_thrd_sem_handles[ctr] = ppv_thread_sem_hdl[ctr];
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_set_prev_done_frm_frm_sync \endif
*
* \brief
*    Set the values to dependency not resolved state
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_set_prev_done_frm_frm_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 thrds;
    ULWORD64 *pu8_num_units_proc_curr;
    ULWORD64 *pu8_num_units_proc_prev;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Reset the values num threads entering processing state  */
    pu8_num_units_proc_curr = (ULWORD64 *)ps_dep_mngr_state->pv_units_prcsd_in_row;
    pu8_num_units_proc_prev =
        (ULWORD64 *)(pu8_num_units_proc_curr + ps_dep_mngr_state->i4_num_thrds);

    /* Reset the values thread ids waiting */
    for(thrds = 0; thrds < ps_dep_mngr_state->i4_num_thrds; thrds++)
    {
        pu8_num_units_proc_prev[thrds] = 1;
        ps_dep_mngr_state->pi4_wait_thrd_id[thrds] = -1;
    }

    return;
}

/*!
******************************************************************************
* \if Function name : ihevce_dmgr_set_done_frm_frm_sync \endif
*
* \brief
*    Set the values to dependency met state
*
* \param[in,out]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    None
*
* \author
*  Ittiam
*
*****************************************************************************
*/
void ihevce_dmgr_set_done_frm_frm_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 thrds;
    ULWORD64 *pu8_num_units_proc_curr;

    /* dep manager state structure */
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Reset the values num threads entering processing state  */
    pu8_num_units_proc_curr = (ULWORD64 *)ps_dep_mngr_state->pv_units_prcsd_in_row;

    /* Reset the values thread ids waiting */
    for(thrds = 0; thrds < ps_dep_mngr_state->i4_num_thrds; thrds++)
    {
        pu8_num_units_proc_curr[thrds] = 1;
        ps_dep_mngr_state->pi4_wait_thrd_id[thrds] = -1;
    }

    return;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_chk_row_row_sync \endif
*
* \brief
*    This function checks whether the dependency is met to proceed with
*    processing. If condition is not met, it should go to a sem_wait state,
*    else start processing.
*
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in]  cur_offset    : Current offset of the dep. variable
* \param[in]  dep_offset    : Offset from the current value to meet the dep.
* \param[in]  dep_row       : The position of the Ref.
* \param[in]  cur_tile_col  : The current column tile number (not tile id)
*   Assuming the dependency is within the tile only (Acroos tiles won't work now)
* \param[in]  thrd_id       : Thread id of the current thread checking for dependency
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_chk_row_row_sync(
    void *pv_dep_mngr_state,
    WORD32 cur_offset,
    WORD32 dep_offset,
    WORD32 dep_row,
    WORD32 cur_tile_col,
    WORD32 thrd_id)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    volatile WORD32 *pi4_ref_value;
    WORD32 ref_value;

    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Sanity Check */
    ASSERT(dep_row >= 0);
    ASSERT(dep_row < ps_dep_mngr_state->i4_num_vert_units);
    ASSERT(cur_tile_col >= 0);
    ASSERT(cur_tile_col < ps_dep_mngr_state->i4_num_tile_cols);

    pi4_ref_value = ((volatile WORD32 *)(ps_dep_mngr_state->pv_units_prcsd_in_row)) +
                    (cur_tile_col * ps_dep_mngr_state->i4_num_vert_units) + dep_row;

    /* Sanity Check */
    ASSERT((cur_offset + dep_offset) <= ps_dep_mngr_state->i4_num_horz_units);

    /* Check whether Dep. is met */
    while(1)
    {
        ref_value = *pi4_ref_value;

        if(ref_value >= (cur_offset + dep_offset))
            break;

        if(1 == ps_dep_mngr_state->i1_sem_enable)
        {
            void *pv_sem_handle;
            WORD32 ret_val;

            (void)ret_val;
            pv_sem_handle = ps_dep_mngr_state->ppv_thrd_sem_handles[thrd_id];

            /* register the thread id before going to pend state */
            ps_dep_mngr_state->pi4_wait_thrd_id[dep_row] = thrd_id;

            /* go to the pend state */
            ret_val = osal_sem_wait(pv_sem_handle);
            //ASSERT(0 == ret_val);
        }
    }

    return 0;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_set_row_row_sync \endif
*
* \brief
*    This function sets the dependency and wakes up the proper semaphores
*    to start processing.
*
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in]  cur_offset     : Current offset processed
* \param[in]  cur_row       : The cur. vertical position
* \param[in]  cur_tile_col  : The current column tile number (not tile id)
*   Assuming the dependency is within the tile only (Acroos tiles won't work now)
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_set_row_row_sync(
    void *pv_dep_mngr_state, WORD32 cur_offset, WORD32 cur_row, WORD32 cur_tile_col)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD32 *pi4_units_prcsd;
    void *pv_sem_handle;
    WORD32 ret_val;

    (void)ret_val;
    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Sanity Check */
    ASSERT(cur_offset >= 0);
    ASSERT(cur_offset <= ps_dep_mngr_state->i4_num_horz_units);
    ASSERT(cur_row <= ps_dep_mngr_state->i4_num_vert_units);
    ASSERT(cur_tile_col >= 0);
    ASSERT(cur_tile_col < ps_dep_mngr_state->i4_num_tile_cols);

    DATA_SYNC();

    pi4_units_prcsd = ((WORD32 *)(ps_dep_mngr_state->pv_units_prcsd_in_row)) +
                      (cur_tile_col * ps_dep_mngr_state->i4_num_vert_units) + cur_row;

    /* Update the number of units processed */
    *pi4_units_prcsd = cur_offset;

    if(1 == ps_dep_mngr_state->i1_sem_enable)
    {
        WORD32 wait_thrd_id;

        wait_thrd_id = ps_dep_mngr_state->pi4_wait_thrd_id[cur_row];

        /* Post on threads waiting on the current row */
        if(-1 != wait_thrd_id)
        {
            pv_sem_handle = ps_dep_mngr_state->ppv_thrd_sem_handles[wait_thrd_id];
            /* Post on the semaphore */
            ret_val = osal_sem_post(pv_sem_handle);
            //ASSERT(0 == ret_val);

            ps_dep_mngr_state->pi4_wait_thrd_id[cur_row] = -1;
        }

        /* towards end of row all threads are posted (to avoid any corner cases) */
        if(cur_offset == ps_dep_mngr_state->i4_num_horz_units)
        {
            WORD32 ctr;

            for(ctr = 0; ctr < ps_dep_mngr_state->i4_num_thrds; ctr++)
            {
                ret_val = osal_sem_post(ps_dep_mngr_state->ppv_thrd_sem_handles[ctr]);
                //ASSERT(0 == ret_val);
            }
        }
    }

    return 0;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_chk_frm_frm_sync \endif
*
* \brief
*    This function checks whether the dependency is met to proceed with
*    processing. If condition is not met, it should go to a sem_wait state,
*    else start processing.
*    For Barrier case, the thread will wait till all threads have completed
*    the processing on the previosu instance of same stage
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in]  thrd_id       : Thread id checking for dependency
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_chk_frm_frm_sync(void *pv_dep_mngr_state, WORD32 thrd_id)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    void *pv_sem_handle;
    volatile ULWORD64 *pu8_num_units_proc_prev;
    volatile ULWORD64 *pu8_num_units_proc_curr;
    ULWORD64 prev_value;
    ULWORD64 curr_value;

    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;
    pv_sem_handle = ps_dep_mngr_state->ppv_thrd_sem_handles[thrd_id];

    pu8_num_units_proc_curr = (volatile ULWORD64 *)ps_dep_mngr_state->pv_units_prcsd_in_row;
    pu8_num_units_proc_prev =
        (volatile ULWORD64 *)(pu8_num_units_proc_curr + ps_dep_mngr_state->i4_num_thrds);

    /* Check whether Dep. is met */
    while(1)
    {
        WORD32 ret_val;

        (void)ret_val;
        curr_value = pu8_num_units_proc_curr[thrd_id];
        prev_value = pu8_num_units_proc_prev[thrd_id];

        if(curr_value == (prev_value + 1))
        {
            break;
        }
        else
        {
            /* register the thread id before going to pend state */
            ps_dep_mngr_state->pi4_wait_thrd_id[thrd_id] = thrd_id;

            /* go to the pend state */
            ret_val = osal_sem_wait(pv_sem_handle);
            //ASSERT(0 == ret_val);
        }
    }

    /* store curr value to prev for next iteration */
    pu8_num_units_proc_prev[thrd_id] = pu8_num_units_proc_curr[thrd_id];

    return 0;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_update_frm_frm_sync \endif
*
* \brief
*    This function sets the dependency and wakes up the proper semaphores
*    to start processing.
*    For barrier case, if the dep. is met, all waiting threads should be waked up
*
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_update_frm_frm_sync(void *pv_dep_mngr_state)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    void *pv_sem_handle;
    volatile ULWORD64 *pu8_num_units_proc_curr;
    WORD32 ctr;

    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    pu8_num_units_proc_curr = (volatile ULWORD64 *)ps_dep_mngr_state->pv_units_prcsd_in_row;

    /* Post on All vertical waiting threads semaphores & update the cur unit proc */
    for(ctr = 0; ctr < ps_dep_mngr_state->i4_num_thrds; ctr++)
    {
        WORD32 ret_val;
        WORD32 wait_thrd_id;

        (void)ret_val;
        /* increment the curr unit counter for all threads */
        pu8_num_units_proc_curr[ctr] = pu8_num_units_proc_curr[ctr] + 1;

        wait_thrd_id = ctr;
        //wait_thrd_id    = ps_dep_mngr_state->pi4_wait_thrd_id[ctr];

        if(-1 != wait_thrd_id)
        {
            pv_sem_handle = ps_dep_mngr_state->ppv_thrd_sem_handles[wait_thrd_id];
            /* Post on the semaphore */
            ret_val = osal_sem_post(pv_sem_handle);
            //ASSERT(0 == ret_val);

            ps_dep_mngr_state->pi4_wait_thrd_id[ctr] = -1;
        }
    }

    return 0;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_map_chk \endif
*
* \brief
*   This function checks whether all entries in the dependency map are set
*
* \param[in]  pu1_start    : Pointer to the start of the search area
* \param[in]  i4_num_ctb_x : Size   of search area
* \param[in]  i4_num_ctb_y : Size   of search area
* \param[in]  i4_stride    : Stride of search area
*
* \return
*    1 on Success otherwise 0
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32
    ihevce_dmgr_map_chk(WORD8 *pu1_start, WORD32 i4_num_ctb_x, WORD32 i4_num_ctb_y, WORD32 i4_stride)
{
    WORD8 *pi1_ctb = pu1_start;
    WORD32 row, col;
    WORD32 map_ready_flag = MAP_CTB_COMPLETE;

    for(row = 0; row < i4_num_ctb_y; row++)
    {
        for(col = 0; col < i4_num_ctb_x; col++)
        {
            map_ready_flag &= pi1_ctb[col];
        }
        pi1_ctb += i4_stride;
    }

    /* NOTE: early exit in the above loop can taken if map_ready_flag
    is found to be zero somewhere at the start itself */
    return (map_ready_flag == MAP_CTB_COMPLETE);
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_map_chk_sync \endif
*
* \brief
*   This function checks whether the dependency is met by searching in a
*   rectangular area. If condition is not met, it should go to a sem_wait state,
*    else start processing.
*
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in]  thrd_id     : Thread id of the current thread checking for dependency
* \param[in]  offset_x    : Offset of current CTB in Tile in ctb-unit
* \param[in]  offset_y    : Offset of current CTB in Tile in ctb-unit
* \param[in]  i4_sr_ctb_x : Search Range in ctb-unit
* \param[in]  i4_sr_ctb_y : Search Range in ctb-unit
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_map_chk_sync(
    void *pv_dep_mngr_state,
    WORD32 thrd_id,
    WORD32 offset_x,
    WORD32 offset_y,
    WORD32 i4_sr_ctb_x,
    WORD32 i4_sr_ctb_y)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    volatile WORD8 *pi1_ctb;
    WORD8 *pi1_tile_start;
    WORD32 i4_avail_left, i4_avail_right, i4_avail_top, i4_avail_bot;
    WORD32 i4_num_ctb_x, i4_num_ctb_y;
    WORD32 i4_stride;
    WORD32 i4_tile_wd, i4_tile_ht;  //in ctb units

    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    i4_tile_wd = ps_dep_mngr_state->i4_num_horz_units - ps_dep_mngr_state->ai4_tile_xtra_ctb[1] -
                 ps_dep_mngr_state->ai4_tile_xtra_ctb[2];

    i4_tile_ht = ps_dep_mngr_state->i4_num_vert_units - ps_dep_mngr_state->ai4_tile_xtra_ctb[0] -
                 ps_dep_mngr_state->ai4_tile_xtra_ctb[3];

    i4_stride = ps_dep_mngr_state->i4_num_horz_units;

    /* Sanity Checks, confirm if ctb offsets are within tiles */
    ASSERT(offset_x >= 0);
    ASSERT(offset_y >= 0);
    ASSERT(offset_x < i4_tile_wd);
    ASSERT(offset_y < i4_tile_ht);

    pi1_tile_start = (WORD8 *)ps_dep_mngr_state->pv_units_prcsd_in_row;
    pi1_ctb = (volatile WORD8 *)pi1_tile_start;

    if(ps_dep_mngr_state->ai4_tile_xtra_ctb[0])
    {
        i4_avail_top = i4_sr_ctb_y;
    }
    else
    {
        i4_avail_top = MIN(i4_sr_ctb_y, offset_y);
    }

    if(ps_dep_mngr_state->ai4_tile_xtra_ctb[1])
    {
        i4_avail_left = i4_sr_ctb_x;
    }
    else
    {
        i4_avail_left = MIN(i4_sr_ctb_x, offset_x);
    }

    if(ps_dep_mngr_state->ai4_tile_xtra_ctb[2])
    {
        i4_avail_right = i4_sr_ctb_x;
    }
    else
    {
        i4_avail_right = MIN(i4_sr_ctb_x, (i4_tile_wd - offset_x - 1));
    }

    if(ps_dep_mngr_state->ai4_tile_xtra_ctb[3])
    {
        i4_avail_bot = i4_sr_ctb_y;
    }
    else
    {
        i4_avail_bot = MIN(i4_sr_ctb_y, (i4_tile_ht - offset_y - 1));
    }

    i4_num_ctb_x = (i4_avail_left + 1 + i4_avail_right);
    i4_num_ctb_y = (i4_avail_top + 1 + i4_avail_bot);

    /* Point to the start of the search-area */
    pi1_ctb += ((offset_y - i4_avail_top) * i4_stride + (offset_x - i4_avail_left));

    /* Check whether Dep. is met */
    while(1)
    {
        if(1 == ihevce_dmgr_map_chk((WORD8 *)pi1_ctb, i4_num_ctb_x, i4_num_ctb_y, i4_stride))
        {
            break;
        }
        else
        {
            if(1 == ps_dep_mngr_state->i1_sem_enable)
            {
                osal_sem_wait(ps_dep_mngr_state->ppv_thrd_sem_handles[thrd_id]);
            }
        }
    }

    return 0;
}

/*!
**************************************************************************
* \if Function name : ihevce_dmgr_map_set_sync \endif
*
* \brief
*    This function sets the dependency and wakes up the proper semaphores
*    to start processing.
*
* \param[in]  pv_dep_mngr_state  : Pointer to Sync Manager handle.
* \param[in]  offset_x           : Offset of current CTB in Tile(ctb unit)
* \param[in]  offset_y           : Offset of current CTB in Tile(ctb unit)
*
* \return
*    0 on Success and -1 on error
*
* \author
*  Ittiam
*
**************************************************************************
*/
WORD32 ihevce_dmgr_map_set_sync(
    void *pv_dep_mngr_state, WORD32 offset_x, WORD32 offset_y, WORD32 i4_map_value)
{
    dep_mngr_state_t *ps_dep_mngr_state;
    WORD8 *pi1_tile_start;
    WORD32 map_stride;

    ps_dep_mngr_state = (dep_mngr_state_t *)pv_dep_mngr_state;

    /* Sanity Checks */
    ASSERT(offset_x >= (-ps_dep_mngr_state->ai4_tile_xtra_ctb[1]));
    ASSERT(offset_y >= (-ps_dep_mngr_state->ai4_tile_xtra_ctb[0]));

    ASSERT(
        offset_x <
        (ps_dep_mngr_state->i4_num_horz_units - ps_dep_mngr_state->ai4_tile_xtra_ctb[1]));

    ASSERT(
        offset_y <
        (ps_dep_mngr_state->i4_num_vert_units - ps_dep_mngr_state->ai4_tile_xtra_ctb[0]));

    DATA_SYNC();

    map_stride = ps_dep_mngr_state->i4_num_horz_units;

    pi1_tile_start = (WORD8 *)ps_dep_mngr_state->pv_units_prcsd_in_row;

    /* Set the flag to indicate that this CTB has been processed */
    *(pi1_tile_start + offset_y * map_stride + offset_x) = (WORD8)i4_map_value;

    if(1 == ps_dep_mngr_state->i1_sem_enable)
    {
        WORD32 wait_thrd_id;

        /* Post on threads waiting on the current row */
        for(wait_thrd_id = 0; wait_thrd_id < ps_dep_mngr_state->i4_num_thrds; wait_thrd_id++)
        {
            /* Post on the semaphore */
            /* Map-mode: semaphore post is unconditionally done on all threads */
            osal_sem_post(ps_dep_mngr_state->ppv_thrd_sem_handles[wait_thrd_id]);
        }
    }

    return 0;
}
