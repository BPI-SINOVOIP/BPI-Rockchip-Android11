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
* \file rc_sad_acc.h
*
* \brief
*    This file contains all the necessary declarations for
*    sad accumulate functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/
#ifndef _RC_SAD_ACC_H_
#define _RC_SAD_ACC_H_

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct sad_acc_t *sad_acc_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/
WORD32 sad_acc_num_fill_use_free_memtab(
    sad_acc_handle *pps_sad_acc, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);
void init_sad_acc(sad_acc_handle ps_sad_acc);
void sad_acc_put_sad(
    sad_acc_handle ps_sad_acc_handle,
    WORD32 i4_cur_intra_sad,
    WORD32 i4_cur_sad,
    WORD32 i4_cur_pic_type);
void sad_acc_get_sad(sad_acc_handle ps_sad_acc_handle, WORD32 *pi4_sad);
#endif /* _RC_SAD_ACC_H_ */
