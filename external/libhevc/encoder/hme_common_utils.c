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
/* File Includes                                                             */
/*****************************************************************************/
/* System include files */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/* User include files */
#include "ihevc_typedefs.h"
#include "ihevc_macros.h"
#include "ihevc_debug.h"
#include "ihevc_platform_macros.h"

#include "hme_datatype.h"
#include "hme_common_defs.h"

/*****************************************************************************/
/* Function Definitions                                                      */
/*****************************************************************************/

/**
********************************************************************************
*  @fn     S16 median4_s16(S16 i2_n1, S16 i2_n2, S16 i2_n3, S16 i2_n4);
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
S16 median4_s16(S16 i2_n1, S16 i2_n2, S16 i2_n3, S16 i2_n4)
{
    S16 i2_max, i2_min;

    i2_max = MAX(i2_n1, i2_n2);
    i2_max = MAX(i2_max, i2_n3);
    i2_max = MAX(i2_max, i2_n4);

    i2_min = MIN(i2_n1, i2_n2);
    i2_min = MIN(i2_min, i2_n3);
    i2_min = MIN(i2_min, i2_n4);

    return ((S16)((i2_n1 + i2_n2 + i2_n3 + i2_n4 - i2_max - i2_min) >> 1));
}

U32 hme_compute_2d_sum_u08(U08 *pu1_inp, S32 i4_wd, S32 i4_ht, S32 i4_stride)
{
    S32 i, j;
    U32 u4_sum = 0;

    for(i = 0; i < i4_ht; i++)
    {
        for(j = 0; j < i4_wd; j++)
            u4_sum += (U32)pu1_inp[j];

        pu1_inp += i4_stride;
    }

    return (u4_sum);
}
U32 hme_compute_2d_sum_u16(U16 *pu2_inp, S32 i4_wd, S32 i4_ht, S32 i4_stride)
{
    S32 i, j;
    U32 u4_sum = 0;

    for(i = 0; i < i4_ht; i++)
    {
        for(j = 0; j < i4_wd; j++)
            u4_sum += (U32)pu2_inp[j];

        pu2_inp += i4_stride;
    }

    return (u4_sum);
}
U32 hme_compute_2d_sum_u32(U32 *pu4_inp, S32 i4_wd, S32 i4_ht, S32 i4_stride)
{
    S32 i, j;
    U32 u4_sum = 0;

    for(i = 0; i < i4_ht; i++)
    {
        for(j = 0; j < i4_wd; j++)
            u4_sum += (U32)pu4_inp[j];

        pu4_inp += i4_stride;
    }

    return (u4_sum);
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
    void *pv_inp, S32 i4_blk_wd, S32 i4_blk_ht, S32 i4_stride, S32 i4_datatype)
{
    if(i4_datatype == sizeof(U08))
        return (hme_compute_2d_sum_u08((U08 *)pv_inp, i4_blk_wd, i4_blk_ht, i4_stride));
    else if(i4_datatype == sizeof(U16))
        return (hme_compute_2d_sum_u16((U16 *)pv_inp, i4_blk_wd, i4_blk_ht, i4_stride));
    else if(i4_datatype == sizeof(U32))
        return (hme_compute_2d_sum_u32((U32 *)pv_inp, i4_blk_wd, i4_blk_ht, i4_stride));
    else
        ASSERT(0);

    return 0;
}

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
S32 get_rand_num(S32 low, S32 high)
{
    double num;
    S32 result;
    num = (double)rand() / (double)RAND_MAX;
    num = num * (high - low) + low;

    result = (S32)floor((num + 0.5));
    if(result < low)
        result = low;
    if(result >= high)
        result = high - 1;

    return (result);
}
