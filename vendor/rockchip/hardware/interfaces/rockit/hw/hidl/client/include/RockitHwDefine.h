/*
 * Copyright (C) 2019 Rockchip Electronics Co. LTD
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

#ifndef ANDROID_RT_HW_DEFINE_H
#define ANDROID_RT_HW_DEFINE_H

#include <sys/types.h>

enum RTHWType {
    HW_TYPE_UNKNOWN = -1,
    HW_DECODER_MPI = 1,
    HW_DECODER_VPUAPI = 2,
    HW_ENCODER_MPI = 3,
    HW_ENCODER_VPUAPI = 4,
    HW_IEP = 5,
    HW_RGA = 6,
    HW_ISP = 7,
   /* add more here if need */
};

enum RTHWEvent {
    HW_SUCCESS = 0,
    HW_ERROR_INFOR = 1,
    HW_FAIL = 2,
    /* add more here if need */
};

enum RTHWBufferType  {
    RT_HW_BUFFER_DRM = 0,
    RT_HW_BUFFER_ION = 1,
   /* add more here if need */
};

struct RTHWParamPair {
    uint32_t     key;
    uint64_t     value;
    /* add more here if need */
};

struct RTHWParamPairs {
    uint32_t     counter;
    RTHWParamPair *pairs;
};

struct RTHWBuffer {
    uint32_t        bufferType;
    uint32_t        bufferId;
    uint32_t        size;
    uint32_t        length;
    RTHWParamPairs  pair;

    /* add more here if need */
};

#endif
