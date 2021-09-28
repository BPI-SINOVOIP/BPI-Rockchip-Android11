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
/*  File Name         : osal_handle.h                                        */
/*                                                                           */
/*  Description       : This file contains all the necessary structure       */
/*                      declarations use by OSAL library.                    */
/*                                                                           */
/*  List of Functions : None                                                 */
/*                                                                           */
/*  Issues / Problems : None                                                 */
/*                                                                           */
/*  Revision History  :                                                      */
/*                                                                           */
/*         DD MM YYYY   Author(s)       Changes (Describe the changes made)  */
/*         06 03 2006   Ittiam          Draft                                */
/*                                                                           */
/*****************************************************************************/

#ifndef OSAL_HANDLE_H
#define OSAL_HANDLE_H

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

#define MAX_FDS 40
#define DEBUG_ORDER 100

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/

typedef enum
{
    CREATED,
    DESTROYED,
    ERRORED
} DEBUG_STATE_T;

/*****************************************************************************/
/* Typedefs                                                                  */
/*****************************************************************************/

typedef struct
{
    void *handle;
    WORD32 state;
} debug_handle_t;

typedef struct
{
    debug_handle_t thread_handle[DEBUG_ORDER];
    WORD32 thread_count;
    debug_handle_t mutex_handle[DEBUG_ORDER];
    WORD32 mutex_count;
    debug_handle_t mbox_handle[DEBUG_ORDER];
    WORD32 mbox_count;
    debug_handle_t socket_handle[DEBUG_ORDER];
    WORD32 socket_count;
    debug_handle_t sem_handle[DEBUG_ORDER];
    WORD32 sem_count;
    debug_handle_t select_engine_handle[DEBUG_ORDER];
    WORD32 select_engine_count;
} osal_debug_t;

typedef struct
{
    void *mmr_handle; /* Handle to memory manager */
    void *(*alloc)(void *mmr_handle, UWORD32 size); /* Call back for allocation */
    void (*free)(void *mmr_handle, void *mem); /* Call back for free       */

} osal_t;

#endif /* OSAL_HANDLE_H */
