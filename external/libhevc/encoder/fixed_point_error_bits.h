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
* \file fixed_point_error_bits.h
*
* \brief
*    This file contains all the necessary declarations for
*    error bits processing functions
*
* \date
*
* \author
*    ittiam
*
******************************************************************************
*/

#ifndef FIXED_POINT_ERROR_BITS_H
#define FIXED_POINT_ERROR_BITS_H

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/
typedef struct error_bits_t *error_bits_handle;

/*****************************************************************************/
/* Function Declarations                                                     */
/*****************************************************************************/

WORD32 error_bits_num_fill_use_free_memtab(
    error_bits_handle *pps_error_bits, itt_memtab_t *ps_memtab, ITT_FUNC_TYPE_E e_func_type);
void init_error_bits(error_bits_handle ps_error_bits, WORD32 i4_max_tgt_frm_rate, WORD32 i4_bitrate);
void update_error_bits(error_bits_handle ps_error_bits);
WORD32 get_error_bits(error_bits_handle ps_error_bits);

void change_frm_rate_in_error_bits(error_bits_handle ps_error_bits, WORD32 i4_tgt_frm_rate);
void change_bitrate_in_error_bits(error_bits_handle ps_error_bits, WORD32 i4_bitrate);

#endif
