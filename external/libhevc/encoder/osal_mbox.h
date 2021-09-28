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
/*  File Name         : osal_mbox.h                                          */
/*                                                                           */
/*  Description       : This file contains OSAL Mail box handle structure    */
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

#ifndef OSAL_MBOX_H
#define OSAL_MBOX_H

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

/* Mail box handle structure. In WIN32 mail boxes are associated with each   */
/* Thread. So, thread id represents the mail box in question. In POSIX, name */
/* distinguishs the mail boxes.                                              */

typedef struct
{
    WORD32 mq_id; /* Message queue identifier               */
    void *mmr_handle; /* Pointer to memory manager handle       */
    osal_t *hdl; /* Associated OSAL handle                 */
} mbox_handle_t;

typedef struct
{
    void *mmr_handle; /* Pointer to memory manager handle                 */
    osal_t *hdl; /* Associated OSAL handle                           */

    void *count_sem; /* Semaphore to take care of get to an empty queue  */
    void *sync_mutex; /* Mutex to maintain sync in post msg and get msg   */
    void *data; /* msg posted or got from the queue                 */

    UWORD32 write_count; /* Post Count                                       */
    UWORD32 read_count; /* Get Count                                        */
    UWORD32 msg_size; /* Size of the msg                                  */
    UWORD32 mbox_len; /* Max number of messages the mailbox can handle    */
} custom_mbox_handle_t;

#endif /* OSAL_MBOX_H */
