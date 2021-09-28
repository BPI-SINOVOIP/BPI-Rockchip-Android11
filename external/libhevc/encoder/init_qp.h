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
* \file init_qp.h
*
* \brief
*    This file contains all the necessary declarations for
*    qp initialization functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _INIT_QP_H_
#define _INIT_QP_H_

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct init_qp_t *init_qp_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

WORD32 init_qp_num_fill_use_free_memtab(
    init_qp_handle *pps_init_qp, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);

void init_init_qp(
    init_qp_handle ps_init_qp,
    WORD32 *pi4_min_max_qp,
    WORD32 i4_num_pels_in_frame,
    WORD32 i4_is_hbr);

/* If the remaining pels in frame is zero we would be using the init time pixels for calculating the bits per pixel */
WORD32 get_init_qp_using_pels_bits_per_frame(
    init_qp_handle ps_init_qp,
    picture_type_e e_pic_type,
    WORD32 i4_bits_remaining_in_frame,
    WORD32 i4_rem_pels_in_frame);

void change_init_qp_max_qp(init_qp_handle ps_init_qp, WORD32 *pi4_min_max_qp);
#endif /* _INIT_QP_H_ */
