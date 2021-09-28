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
* \file ihevce_dep_mngr_private.h
*
* \brief
*    This file contains private structures & definations of Sync manager
*
* \date
*    13/12/2013
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_DEP_MANAGER_PRIVATE_H_
#define _IHEVCE_DEP_MANAGER_PRIVATE_H_

/*****************************************************************************/
/* File Includes                                                             */
/*****************************************************************************/

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
    DEP_MNGR_CTXT = 0,
    DEP_MNGR_UNITS_PRCSD_MEM,
    DEP_MNGR_WAIT_THRD_ID_MEM,
    DEP_MNGR_SEM_HANDLE_MEM,

    /* should be last entry */
    NUM_DEP_MNGR_MEM_RECS
} DEP_MNGR_MEM_T;

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

typedef struct
{
    /*! Number of Vertical units to be processed */
    WORD32 i4_num_vert_units;

    /*! Maximun Number of Horizontal units to be processed */
    WORD32 i4_num_horz_units;

    /*! Number of column tiles for which encoder is working */
    WORD32 i4_num_tile_cols;

    /*! Array to update the units which got processed in each row */
    /*! For num_tile_cols > 1 , the memory layout is
        0-max_num_vert_units for col_tile 0
        0-max_num_vert_units for col_tile 1
        ..
        ..
        0-max_num_vert_units for col_tile N-1
    */
    void *pv_units_prcsd_in_row;

    /*! Array to register the thread ids of waiting threads in each row */
    /*! Memory Layout : (Row - Row) 1 entry per row
        Memory Layout : (Frame - Frame) Num threads per frame
        Memory layout : (Row - Frame)
        Num threads for Row 0
        Num threads for Row 1
        Num threads for Row 2
        ..
        ..
        Num threads for Row N-1
    */
    WORD32 *pi4_wait_thrd_id;

    /*! Number of threads in the dependency chain */
    WORD32 i4_num_thrds;

    /*! Pointer to Array of Thread semaphore handle */
    void **ppv_thrd_sem_handles;

    /*! Dependency Manager Mode */
    WORD32 i4_dep_mngr_mode; /* @sa DEP_MNGR_MODE_T */

    /*! 0 : Semaphore not used., 1 : Uses semaphore                          */
    /*! Note : This is required for using spin-lock for some dependencies.   */
    /*! If 0, uses spin-lock(do-while) rather than semaphore for Sync        */
    WORD8 i1_sem_enable;

    /*0: top, 1: left, 2: right, 3: bottom */
    WORD8 ai4_tile_xtra_ctb[4];

    /* temp var: delete it */
    //WORD32   i4_frame_map_complete;

} dep_mngr_state_t;

/*****************************************************************************/
/* Extern Variable Declarations                                              */
/*****************************************************************************/

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

#endif  //_IHEVCE_DEP_MANAGER_PRIVATE_H_
