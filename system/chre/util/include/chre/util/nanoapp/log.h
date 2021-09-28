/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef CHRE_UTIL_NANOAPP_LOG_H_
#define CHRE_UTIL_NANOAPP_LOG_H_

/**
 * @file
 * Logging macros for nanoapps. These macros allow injecting a LOG_TAG and
 * compiling nanoapps with a minimum logging level (that is different than CHRE
 * itself).
 *
 * The typical format for the LOG_TAG macro is: "[AppName]"
 */

#include <chre/re.h>

#include "chre/util/log_common.h"

#ifndef NANOAPP_MINIMUM_LOG_LEVEL
#error "NANOAPP_MINIMUM_LOG_LEVEL must be defined"
#endif  // NANOAPP_MINIMUM_LOG_LEVEL

/**
 * Logs an out of memory error with file and line number.
 */
#define LOG_OOM() LOGE("OOM at %s:%d", CHRE_FILENAME, __LINE__)

/*
 * Supply a stub implementation of the LOGx macros when the build is
 * configured with a minimum logging level that is above the requested level.
 * Otherwise just map into the chreLog function with the appropriate level.
 */

#define CHRE_LOG_TAG(level, tag, fmt, ...)         \
  do {                                             \
    CHRE_LOG_PREAMBLE                              \
    chreLog(level, "%s " fmt, tag, ##__VA_ARGS__); \
    CHRE_LOG_EPILOGUE                              \
  } while (0)

#if NANOAPP_MINIMUM_LOG_LEVEL >= CHRE_LOG_LEVEL_ERROR
#define LOGE_TAG(tag, fmt, ...) \
  CHRE_LOG_TAG(CHRE_LOG_ERROR, tag, fmt, ##__VA_ARGS__)
#else
#define LOGE_TAG(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#endif
#define LOGE(fmt, ...) LOGE_TAG(LOG_TAG, fmt, ##__VA_ARGS__)

#if NANOAPP_MINIMUM_LOG_LEVEL >= CHRE_LOG_LEVEL_WARN
#define LOGW_TAG(tag, fmt, ...) \
  CHRE_LOG_TAG(CHRE_LOG_WARN, tag, fmt, ##__VA_ARGS__)
#else
#define LOGW_TAG(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#endif
#define LOGW(fmt, ...) LOGW_TAG(LOG_TAG, fmt, ##__VA_ARGS__)

#if NANOAPP_MINIMUM_LOG_LEVEL >= CHRE_LOG_LEVEL_INFO
#define LOGI_TAG(tag, fmt, ...) \
  CHRE_LOG_TAG(CHRE_LOG_INFO, tag, fmt, ##__VA_ARGS__)
#else
#define LOGI_TAG(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#endif
#define LOGI(fmt, ...) LOGI_TAG(LOG_TAG, fmt, ##__VA_ARGS__)

#if NANOAPP_MINIMUM_LOG_LEVEL >= CHRE_LOG_LEVEL_DEBUG
#define LOGD_TAG(tag, fmt, ...) \
  CHRE_LOG_TAG(CHRE_LOG_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define LOGD_TAG(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#endif
#define LOGD(fmt, ...) LOGD_TAG(LOG_TAG, fmt, ##__VA_ARGS__)

// Use this macro when including privacy-sensitive information like the user's
// location.
#ifdef LOG_INCLUDE_SENSITIVE_INFO
#define LOGE_SENSITIVE_INFO LOGE
#define LOGE_TAG_SENSITIVE_INFO LOGE_TAG
#define LOGW_SENSITIVE_INFO LOGW
#define LOGW_TAG_SENSITIVE_INFO LOGW_TAG
#define LOGI_SENSITIVE_INFO LOGI
#define LOGI_TAG_SENSITIVE_INFO LOGI_TAG
#define LOGD_SENSITIVE_INFO LOGD
#define LOGD_TAG_SENSITIVE_INFO LOGD_TAG
#else
#define LOGE_SENSITIVE_INFO(fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGE_TAG_SENSITIVE_INFO(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGW_SENSITIVE_INFO(fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGW_TAG_SENSITIVE_INFO(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGI_SENSITIVE_INFO(fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGI_TAG_SENSITIVE_INFO(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGD_SENSITIVE_INFO(fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#define LOGD_TAG_SENSITIVE_INFO(tag, fmt, ...) CHRE_LOG_NULL(fmt, ##__VA_ARGS__)
#endif

#endif  // CHRE_UTIL_NANOAPP_LOG_H_
