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
* \file ihevce_buffer_que_interface.h
*
* \brief
*    This file contains interface prototypes of Buffer Queue manager functions
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _IHEVCE_BUFFER_QUE_INTERFACE_H_
#define _IHEVCE_BUFFER_QUE_INTERFACE_H_

/*****************************************************************************/
/* Extern Function Declarations                                              */
/*****************************************************************************/

/* Create APIs */
WORD32 ihevce_buff_que_get_num_mem_recs(void);

WORD32 ihevce_buff_que_get_mem_recs(
    iv_mem_rec_t *ps_mem_tab, WORD32 max_num_bufs_in_que, WORD32 i4_mem_space);

void *ihevce_buff_que_init(iv_mem_rec_t *ps_mem_tab, WORD32 num_bufs_in_que, void **ppv_buff_ptrs);

/* Process APIs */
void *ihevce_buff_que_get_free_buf(void *pv_buf_que, WORD32 *pi4_id);

void *ihevce_buff_que_get_next_buf(void *pv_buf_que, WORD32 *pi4_id);

void *ihevce_buff_que_get_next_reorder_buf(void *pv_buf_que, WORD32 *pi4_id);

WORD32 ihevce_buff_que_set_buf_prod(void *pv_buf_que, WORD32 buf_id, WORD32 num_users);
/*!< Note :The manager assumes that once a buffer is requested from Q atleast
 * one consumer will consume it. so num_uers should be 0 when buffer has only
 * one consumer, In general num_users should be passed as MAX num
 * consumers - 1 */

WORD32 ihevce_buff_que_rel_buf(void *pv_buf_que, WORD32 buf_id);

WORD32 ihevce_buff_que_get_active_bufs(void *pv_buf_que);

WORD32 ihevce_buff_que_set_reorder_buf(void *pv_buf_que, WORD32 buf_id);

void *ihevce_buff_que_get_buf(void *pv_buf_que, WORD32 i4_id);

#endif /* _IHEVCE_BUFFER_QUE_INTERFACE_H_ */
