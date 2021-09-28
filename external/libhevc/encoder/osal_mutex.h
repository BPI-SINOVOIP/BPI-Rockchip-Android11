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
/*  File Name         : osal_mutex.h                                         */
/*                                                                           */
/*  Description       : This file contains OSAL Mutex handle structure       */
/*                      definition.                                          */
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

#ifndef OSAL_MUTEX_H
#define OSAL_MUTEX_H

/* Mutex Handle structure in WIN32 contains only handle to its mutex. */
typedef struct
{
    pthread_mutex_t mutex_handle; /* Mutex Identifier                       */
    void *mmr_handle; /* Pointer to memory manager handle       */
    osal_t *hdl; /* Associated OSAL handle                 */

} mutex_handle_t;

#endif /* OSAL_MUTEX_H */
