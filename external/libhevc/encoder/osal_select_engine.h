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

/*****************************************************************************/
/*                                                                           */
/*  File Name         : osal_select_engine.h                                 */
/*                                                                           */
/*  Description       : This file contains OSAL Select Engine handle         */
/*                      structure definition.                                */
/*                                                                           */
/*  List of Functions : None                                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         26 05 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef OSAL_SELECT_ENGINE_H
#define OSAL_SELECT_ENGINE_H

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

#define ACTIVE 1
#define SHUTDOWN 2
#define DEAD 3

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

typedef struct
{
    osal_select_entry_t *readfds[MAX_FDS]; /* To check for read             */
    UWORD32 read_count; /* Count of read descriptors     */
    osal_select_entry_t *writefds[MAX_FDS]; /* To check for write            */
    UWORD32 write_count; /* Count of write descriptors    */
    osal_select_entry_t *exceptfds[MAX_FDS]; /* Check for errors              */
    UWORD32 except_count; /* Count of write descriptors    */
    WORD32 id; /* To generate id for each entry */
    volatile WORD32 state; /* State of select engine        */
    void *thread_handle; /* Select engine thread handle   */
    void *mutex_handle; /* Mutex for mutual exclusion.   */
    void *mmr_handle; /* Handle to memory manager      */
    osal_t *hdl; /* Associated OSAL handle        */

    /* Timeout for thread sleep */
    UWORD32 select_timeout;

    /* Timeout for SELECT system called by osal library */
    UWORD32 select_poll_interval;
} select_engine_t;

#endif /* OSAL_SELECT_ENGINE_H */
