/*
 * Copyright (C) 2014-2017 Intel Corporation.
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

#define LOG_TAG "CommonUtils"

#include "LogHelper.h"
#include <sstream>
#include "stdlib.h"
#include "Utils.h"
#include "LogHelper.h"
#include <string.h>
#include <sys/time.h>

NAMESPACE_DECLARATION {
void getTokens(const char *s, const char delim, std::vector<std::string> &tokens)
{
    std::stringstream ss(s);
    std::string item;

    for (size_t i = 0; std::getline(ss, item, delim); i++) {
        tokens.push_back(item);
    }
}

// Parse string like "640x480" or "10000,20000"
// copy from android CameraParameters.cpp
int parsePair(const char *str, int *first, int *second, char delim, char **endptr)
{
    // Find the first integer.
    char *end;
    int w = (int)strtol(str, &end, 10);
    // If a delimiter does not immediately follow, give up.
    if (*end != delim) {
        LOGE("Cannot find delimiter (%c) in str=%s", delim, str);
        return -1;
    }

    // Find the second integer, immediately after the delimiter.
    int h = (int)strtol(end+1, &end, 10);

    *first = w;
    *second = h;

    if (endptr) {
        *endptr = end;
    }

    return 0;
}

#define FRAC_BITS_CURR_LOC 8 /* Value of 8 is maximum in order to avoid overflow with 16-bit inputs */
#define FRAC_BASE (short)(1) << FRAC_BITS_CURR_LOC

/*!
 * \brief Resize a 2D array with linear interpolation.
 *
 * @param[in,out]
 *  in a_src                pointer to input array (width-major)
 *  in a_src_w              width of the input array
 *  in a_src_h              height of the input array
 *  in a_dst                pointer to output array (width-major)
 *  in a_dst_w              width of the output array
 *  in a_dst_h              height of the output array
 */
template <typename T> int resize2dArray(
    const T* a_src, int a_src_w, int a_src_h,
    T* a_dst, int a_dst_w, int a_dst_h)
{
    int i, j, step_size_w, step_size_h, rounding_term;

    if (a_src_w < 2 || a_dst_w < 2 || a_src_h < 2 || a_dst_h < 2) {
        return  -1;
    }
    nsecs_t startTime = systemTime();
    step_size_w = ((a_src_w-1)<<FRAC_BITS_CURR_LOC) / (a_dst_w-1);
    step_size_h = ((a_src_h-1)<<FRAC_BITS_CURR_LOC) / (a_dst_h-1);
    rounding_term = (1<<(2*FRAC_BITS_CURR_LOC-1));
    for (j = 0; j < a_dst_h; ++j) {
        unsigned int curr_loc_h, curr_loc_lower_h;
        curr_loc_h = j * step_size_h;
        curr_loc_lower_h = (curr_loc_h > 0) ? (curr_loc_h-1)>>FRAC_BITS_CURR_LOC : 0;

        for (i = 0; i < a_dst_w; ++i) {
            unsigned int curr_loc_w, curr_loc_lower_w;

            curr_loc_w = i * step_size_w;
            curr_loc_lower_w = (curr_loc_w > 0) ? (curr_loc_w-1)>>FRAC_BITS_CURR_LOC : 0;

            a_dst[a_dst_w*j+i] =
                (a_src[curr_loc_lower_w + curr_loc_lower_h*a_src_w]  *
                        (((curr_loc_lower_w+1)<<FRAC_BITS_CURR_LOC)-curr_loc_w) *
                        (((curr_loc_lower_h+1)<<FRAC_BITS_CURR_LOC)-curr_loc_h) +
                a_src[curr_loc_lower_w+1 + curr_loc_lower_h*a_src_w]  *
                        (curr_loc_w-((curr_loc_lower_w)<<FRAC_BITS_CURR_LOC))   *
                        (((curr_loc_lower_h+1)<<FRAC_BITS_CURR_LOC)-curr_loc_h) +
                a_src[curr_loc_lower_w + (curr_loc_lower_h+1)*a_src_w]  *
                        (((curr_loc_lower_w+1)<<FRAC_BITS_CURR_LOC)-curr_loc_w) *
                        (curr_loc_h-((curr_loc_lower_h)<<FRAC_BITS_CURR_LOC)) +
                a_src[curr_loc_lower_w+1 + (curr_loc_lower_h+1)*a_src_w]  *
                        (curr_loc_w-((curr_loc_lower_w)<<FRAC_BITS_CURR_LOC))   *
                        (curr_loc_h-((curr_loc_lower_h)<<FRAC_BITS_CURR_LOC))
                + rounding_term) / (FRAC_BASE * FRAC_BASE);
        }
    }
    LOGD("resize the 2D array cost %dus", (unsigned)((systemTime() - startTime) / 1000));

    return 0;
}

template int resize2dArray<float>(
    const float* a_src, int a_src_w, int a_src_h,
    float* a_dst, int a_dst_w, int a_dst_h);
template int resize2dArray<int>(
    const int* a_src, int a_src_w, int a_src_h,
    int* a_dst, int a_dst_w, int a_dst_h);

nsecs_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return nsecs_t(t.tv_sec)*1000000000LL + t.tv_nsec;
}

} NAMESPACE_DECLARATION_END
