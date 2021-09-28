/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2015-2017 Intel Corporation
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

#ifndef ROCKIT_OSAL_SIDEBAND_ERRORS_H_
#define ROCKIT_OSAL_SIDEBAND_ERRORS_H_

#include <sys/types.h>
#include <errno.h>

namespace android {

typedef int         status_t;

#define CLEAR(x) memset (&(x), 0, sizeof (x))

}

// ---------------------------------------------------------------------------

#endif // ANDROID_ERRORS_H
