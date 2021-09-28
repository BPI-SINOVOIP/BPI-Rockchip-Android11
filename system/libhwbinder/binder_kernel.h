/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ANDROID_HARDWARE_BINDER_KERNEL_H
#define ANDROID_HARDWARE_BINDER_KERNEL_H

/**
 * Only need this file to fix the __packed__ keyword.
 */

// TODO(b/31559095): bionic on host
#ifndef __ANDROID__
#define __packed __attribute__((__packed__))
#endif

#include <linux/android/binder.h>

#endif // ANDROID_HARDWARE_BINDER_KERNEL_H
