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
* \file hme_common_utils.h
*
* \brief
*    Common utility functions used by ME
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_COMMON_UTILS_H_
#define _HME_COMMON_UTILS_H_

#include "ihevc_platform_macros.h"

/*****************************************************************************/
/* Macros                                                                    */
/*****************************************************************************/

#define MEDIAN4(a, b, c, d, e) (median4_##e(a, b, c, d))

/*****************************************************************************/
/* Functions                                                                 */
/*****************************************************************************/
/**
********************************************************************************
*  @fn     S32 median4_s16(S16 i2_n1, S16 i2_n2, S16 i2_n3, S16 i2_n4);
*
*  @brief  Returns median4 of 4 16 bits signed nubers
*
*  @param[in] i2_n1 : first number
*
*  @param[in] i2_n2 : 2nd number
*
*  @param[in] i2_n3 : 3rd number
*
*  @param[in] i2_n4 : 4th number (order does not matter)
*
*  @return range of the number
********************************************************************************
*/
S16 median4_s16(S16 i2_n1, S16 i2_n2, S16 i2_n3, S16 i2_n4);

/**
********************************************************************************
*  @fn     S32 hme_get_range(U32 u4_num);
*
*  @brief  Returns the range of the number
*
*  @param[in] u4_num : number whose range is to be found
*
*  @return range of the number
********************************************************************************
*/

static INLINE S32 hme_get_range(U32 u4_num)
{
    S32 r;

    GETRANGE(r, u4_num);
    return (r);
}

/**
********************************************************************************
*  @fn     S32 hme_compute_2d_sum_unsigned(void *pv_inp,
*                                       S32 i4_blk_wd,
*                                       S32 i4_blk_ht,
*                                   S32 i4_stride,
*                                   S32 i4_datatype)
*
*  @brief  Computes and returns 2D sum of a unsigned 2d buffer, with datatype
*          equal to 8/16/32 bit.
*
*  @param[in] pv_inp : input pointer
*
*  @param[in] i4_blk_wd : block width
*
*  @param[in] i4_blk_ht : block ht
*
*  @param[in] i4_stride : stride
*
*  @param[in] i4_datatype : datatype 1 - 8 bit, 2 - 16 bit, 4 - 32 bit
*
*  @return sum of i4_blk_wd * i4_blk_ht number of entries starting at pv_inp
********************************************************************************
*/

U32 hme_compute_2d_sum_unsigned(
    void *pv_inp, S32 i4_blk_wd, S32 i4_blk_ht, S32 i4_stride, S32 i4_datatype);

/**
********************************************************************************
*  @fn     S32 get_rand_num(S32 low, S32 high)
*
*  @brief  returns a radom integer in the closed interval [low, high - 1]
*
*  @param[in] low : lower limit
*
*  @param[in] high : higher limit
*
*  @return S32 result: the random number
********************************************************************************
*/
S32 get_rand_num(S32 low, S32 high);

#endif /* #ifndef _HME_COMMON_UTILS_H_ */
