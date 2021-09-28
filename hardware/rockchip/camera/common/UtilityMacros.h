/*
 * Copyright (C) 2014-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CAMERA3_HAL_COMMON_MACROS_H_
#define _CAMERA3_HAL_COMMON_MACROS_H_

#include <string.h> /* memset */
#include "CommonUtilMacros.h"

/**
 * \macro CLIP
 * Used to clip the Number value to between the Min and Max
 */
#define CLIP(Number, Max, Min)    \
        ((Number) > (Max) ? (Max) : ((Number) < (Min) ? (Min) : (Number)))

#define PAGE_ALIGN(x) (((x) + 0xfff) & 0xfffff000)

#define ARRAY_SIZE(array)    (sizeof(array) / sizeof((array)[0]))

/**
 *  \macro TIMEVAL2USECS
 *  Convert timeval struct to value in microseconds
 *  Helper macro to convert timeval struct to microsecond values stored in a
 *  long long signed value (equivalent to int64_t)
 */
#define TIMEVAL2USECS(x) (int64_t)(((x)->tv_sec*1000000000LL + \
                                    (x)->tv_usec*1000LL)/1000LL)

/**
 * \macro TIMEVAL2NSECS
 *  Convert timeval struct to value in nanoseconds
 *  Helper macro to convert timeval struct to nanosecond values stored in a
 *  long long signed value (equivalent to int64_t)
 */
#define TIMEVAL2NSECS(x) (int64_t)(((x)->tv_sec*1000000000LL + \
                                    (x)->tv_usec*1000LL))

/**
 *  \macro TIMESPEC2USECS
 *  Convert timespec struct to value in microseconds
 *  Helper macro to convert timespec struct to microsecond values stored in a
 *  long long signed value (equivalent to int64_t)
 */
#define TIMESPEC2USECS(x) (int64_t)((x)->tv_sec*1000000LL + (x)->tv_nsec/1000LL)

/**
 * \macro
 * check the value of a binary flag return true if the flag is present
 */
#define CHECK_FLAG(x, flag) (((x) & (flag)) == (flag))

/**
 * \macro INIT_RANGE
 * Initializes the value of a structure of type Range (see CameraWindow.h)
 */
#define INIT_RANGE(x, s, e) (x).start = s; (x).end = e;

/**
 * \macro INIT_COORDINATE
 * Initializes the value of a structure of type ia_coordinate
 *  (see rk_aiq_types.h)
 */
#define INIT_COORDINATE(p,xVal,yVal) (p).x = xVal; (p).y = yVal;

#define ALGIN4(x) (((x) + 3U) & (~3U))
#define ALIGN8(x) (((x) + 7) & ~7)
#define ALIGN16(x) (((x) + 15) & ~15)
#define ALIGN32(x) (((x) + 31) & ~31)
#define ALIGN64(x) (((x) + 63) & ~63)
#define ALIGN128(x) (((x) + 127) & ~127)

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define LIMIT(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#ifdef __GNUC__
#define INDEXED_FIELD_INITIALIZER(x) [x]=
#define NAMED_FIELD_INITIALIZER(x) .x=
#else
#define INDEXED_FIELD_INITIALIZER(x)
#define NAMED_FIELD_INITIALIZER(x)
#endif

#define DELETE_AND_NULLIFY(var) \
    do { \
        delete (var); \
        (var) = nullptr; \
    } while(0)

#define DELETE_ARRAY_AND_NULLIFY(var) \
    do { \
        delete[] (var); \
        (var) = nullptr; \
    } while(0)

#define COMPARE_RESOLUTION(b1, b2) ( \
    (((b1)->width() > (b2)->width()) || ((b1)->height() > (b2)->height())) ? 1 : \
    (((b1)->width() == (b2)->width()) && ((b1)->height() == (b2)->height())) ? 0 : -1)

/**
 * \macro IS_SAME_RESOLUTION_RATIO
 *  if (w1, h1) has the same resoltuion ratio with (w2, h2), the function will return true.
 */
#define IS_SAME_RESOLUTION_RATIO(w1, h1, w2, h2) ( \
    (fabs(((float)(w1) / (float)(h1)) / ((float)(w2) / (float)(h2)) - 1) < 0.01f) ? true : false)

#define COMPARE_FINFO(finfo, w, h) ( \
    (((finfo).width == (w)) && ((finfo).height == (h))) ? 1 : 0)

// Macro to expand define into string
#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)

#define IS_CONTROL_MODE_OFF(mode) ((mode) == ANDROID_CONTROL_MODE_OFF || \
                                   (mode) == ANDROID_CONTROL_MODE_OFF_KEEP_STATE)

#endif /* _CAMERA3_HAL_COMMON_MACROS_H_ */
