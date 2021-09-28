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
* \file hme_common_defs.h
*
* \brief
*    Important definitions, enumerations, macros and structures used by ME
*
* \date
*    18/09/2012
*
* \author
*    Ittiam
*
******************************************************************************
*/

#ifndef _HME_COMMON_DEFS_H_
#define _HME_COMMON_DEFS_H_

/*****************************************************************************/
/* Constant Macros                                                           */
/*****************************************************************************/
#define MAX_32BIT_VAL (0x7FFFFFFF)
#define MAX_SIGNED_16BIT_VAL (0x07FFF)
#define INTERP_INTERMED_BUF_SIZE (72 * 72 * 2)

/*****************************************************************************/
/* Function Macros                                                           */
/*****************************************************************************/
#define HME_CLIP(x, min, max) (((x) < (min)) ? (min) : (((x) > (max)) ? (max) : (x)))

#define ARG_NOT_USED(x) ((void)(x))
/**
******************************************************************************
 *  @brief Average of 2 numbers of any datatype
******************************************************************************
*/
#define AVG2(x, y) (((x) + (y) + 1) >> 1)

#define FLOOR16(x) ((x) & (~15))
#define FLOOR8(x) ((x) & (~7))

#define SET_PIC_LIMIT(s_pic_limit, pad_x, pad_y, wd, ht, num_post_refine)                          \
    {                                                                                              \
        s_pic_limit.i2_min_x = (S16)(-(pad_x) + (num_post_refine));                                \
        s_pic_limit.i2_min_y = (S16)(-(pad_y) + (num_post_refine));                                \
        s_pic_limit.i2_max_x = (S16)((wd) + (pad_x) - (num_post_refine));                          \
        s_pic_limit.i2_max_y = (S16)((ht) + (pad_y) - (num_post_refine));                          \
    }

#define SCALE_FOR_POC_DELTA(x, y, node, ref_tgt, pi2_ref_scf)                                      \
    {                                                                                              \
        x = node->s_mv.i2_mvx;                                                                     \
        y = node->s_mv.i2_mvy;                                                                     \
        x = x * pi2_ref_scf[ref_tgt * MAX_NUM_REF + node->i1_ref_idx];                             \
        y = y * pi2_ref_scf[ref_tgt * MAX_NUM_REF + node->i1_ref_idx];                             \
        x = (x + 128) >> 8;                                                                        \
        y = (y + 128) >> 8;                                                                        \
        HME_CLIP(x, -32768, 32767);                                                                \
        HME_CLIP(y, -32768, 32767);                                                                \
    }

#define SWAP_HME(a, b, data_type)                                                                  \
    {                                                                                              \
        data_type temp = a;                                                                        \
        a = b;                                                                                     \
        b = temp;                                                                                  \
    }

/**
******************************************************************************
 *  @brief Check if MVs lie within a range
******************************************************************************
*/
#define CHECK_MV_WITHIN_RANGE(x, y, range)                                                         \
    (((x) > (range)->i2_min_x) && ((x) < (range)->i2_max_x) && ((y) > (range)->i2_min_y) &&        \
     ((y) < (range)->i2_max_y))

/*****************************************************************************/
/* Structure                                                                 */
/*****************************************************************************/

/**
 ******************************************************************************
 *  @struct mv_t
 *  @brief  Basic Motion vector structure (x and y components)
 ******************************************************************************
*/
typedef struct
{
    S16 i2_mv_x;
    S16 i2_mv_y;
} hme_mv_t;

#endif /* #ifndef _HME_COMMON_DEFS_H_ */
