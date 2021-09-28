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
/*  File Name         : osal_thread.h                                        */
/*                                                                           */
/*  Description       : This file contains OSAL Thread handle structure      */
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

#ifndef OSAL_THREAD_H
#define OSAL_THREAD_H

/*****************************************************************************/
/* Constants                                                                 */
/*****************************************************************************/

#define DIV_COEFF 10
#define MEGA_CONST 1000 * 1000
#define WAIT_INTERVAL 100

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/* Thread handle which stores attributes related to a thread based on the    */
/* platform its being used under.                                            */
typedef struct
{
    pthread_t thread_handle; /* POSIX Thread handle                    */
    WORD32 thread_id; /* Thread Identifier.                     */
    void *mmr_handle; /* Pointer to memory manager handle       */
    osal_t *hdl; /* Associated OSAL handle                 */
    WORD32 priority; /* Thread priority, used in thread suspend*/
    WORD32 policy; /* Scheduling policy                      */
    WORD32 exit_code; /* Exit code on which thread shall exit   */
    WORD32 (*thread_func)(void *); /* Starting point of execution of thread  */
    void *thread_param; /* Thread function argument.              */
} thread_handle_t;

#endif /* OSAL_THREAD_H */
