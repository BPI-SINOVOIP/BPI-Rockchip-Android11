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
* \file ihevce_buffer_que_private.h
*
* \brief
*    This file contains private structures & definitions of Buffer Queue manager
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_BUFFER_QUE_PRIVATE_H_
#define _IHEVCE_BUFFER_QUE_PRIVATE_H_

/*****************************************************************************/
/* Enums                                                                     */
/*****************************************************************************/
typedef enum
{

    BUFFER_QUE_CTXT = 0,
    BUFFER_QUE_NUM_USER_MEM,
    BUFFER_QUE_PROD_STS_MEM,
    BUFFER_QUE_ENC_SEQ_MEM,
    BUFFER_QUE_QUED_SEQ_MEM,

    /* should be last entry */
    NUM_BUFFER_QUE_MEM_RECS

} BUFFER_QUE_MEM_T;

/*****************************************************************************/
/* Structures                                                                */
/*****************************************************************************/

typedef struct
{
    UWORD32 u4_next_disp_seq; /*! < Next display sequence number   */
    UWORD32 u4_last_disp_seq; /*! < Last displayed sequence number */
    UWORD32 u4_last_cons; /*! < last consumed buffer ID        */
    UWORD32 u4_last_prod; /*! < last produced buffer ID        */
    WORD32 i4_num_bufs; /*! < number of buffers              */
    WORD32 i4_num_active_bufs; /*! < number of active buffers       */

    UWORD32 *pu4_enc_seq; /*! < Array to store encode seq of each
                                      buffer */
    UWORD32 *pu4_que_seq; /*! < Array to store queued seq of each
                                       buffer */
    void **ppv_buff_ptrs; /*! < Pointer to array of buffer structure */
    WORD32 *pi4_num_users; /*! < Array to store number of users */
    WORD32 *pi4_produced_sts; /*! < Array to store produced status */
} buf_que_t;

#endif  //_IHEVCE_BUFFER_QUE_PRIVATE_H_
