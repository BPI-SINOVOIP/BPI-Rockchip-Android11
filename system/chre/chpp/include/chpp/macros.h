/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef CHPP_MACROS_H_
#define CHPP_MACROS_H_

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UNUSED_VAR
#define UNUSED_VAR(var) ((void)(var))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b)) ? (a) : (b)
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
#endif

#ifndef CHPP_ASSERT
#define CHPP_ASSERT(var) assert(var)
#endif

#ifndef CHPP_NOT_NULL
#define CHPP_NOT_NULL(var) CHPP_ASSERT((var) != NULL)
#endif

/**
 * Macros for defining (compiler dependent) packed structures
 */
#if defined(__GNUC__) || defined(__clang__)
// For GCC and clang
#define CHPP_PACKED_START
#define CHPP_PACKED_END
#define CHPP_PACKED_ATTR __attribute__((packed))
#elif defined(__ICCARM__) || defined(__CC_ARM)
// For IAR ARM and Keil MDK-ARM compilers
#define CHPP_PACKED_START _Pragma("pack(push, 1)")
#define CHPP_PACKED_END _Pragma("pack(pop)")
#define CHPP_PACKED_ATTR
#else
// Unknown compiler
#error Unrecognized compiler
#endif

#ifdef __cplusplus
}
#endif

#endif  // CHPP_MACROS_H_
