/*
 * Copyright (C) 2019 ARM Limited. All rights reserved.
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

#ifndef MALI_GRALLOC_LOG_H
#define MALI_GRALLOC_LOG_H

#ifndef LOG_TAG
#define LOG_TAG "mali_gralloc"
#endif

#include <log/log.h>

/* Delegate logging to Android */
#define MALI_GRALLOC_LOGI(...) ALOGI(__VA_ARGS__)
#define MALI_GRALLOC_LOGV(...) ALOGV(__VA_ARGS__)
#define MALI_GRALLOC_LOGW(...) ALOGW(__VA_ARGS__)
#define MALI_GRALLOC_LOGE(...) ALOGE(__VA_ARGS__)

#endif
