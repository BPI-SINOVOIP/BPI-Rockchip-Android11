/******************************************************************************
 *
 *  Copyright 2019 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#pragma once

#include <cstdlib>

#ifndef LOG_TAG
#define LOG_TAG "bt"
#endif

#if defined(OS_ANDROID)

#include <log/log.h>

/* When including headers from legacy stack, this log definitions collide with existing logging system. Remove once we
 * get rid of legacy stack. */
#ifndef LOG_VERBOSE

#define LOG_VERBOSE(fmt, args...) ALOGV("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##args)
#define LOG_DEBUG(fmt, args...) ALOGD("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##args)
#define LOG_INFO(fmt, args...) ALOGI("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##args)
#define LOG_WARN(fmt, args...) ALOGW("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##args)
#define LOG_ERROR(fmt, args...) ALOGE("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##args)

#endif /* LOG_VERBOSE*/

#else

/* syslog didn't work well here since we would be redefining LOG_DEBUG. */
#include <chrono>
#include <cstdio>
#include <ctime>

/* When including headers from legacy stack, this log definitions collide with existing logging system. Remove once we
 * get rid of legacy stack. */
#ifndef LOG_VERBOSE

#define LOGWRAPPER(fmt, args...)                                                                                      \
  do {                                                                                                                \
    auto now = std::chrono::system_clock::now();                                                                      \
    auto now_ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);                                       \
    auto now_t = std::chrono::system_clock::to_time_t(now);                                                           \
    /* YYYY-MM-DD_HH:MM:SS.sss is 23 byte long, plus 1 for null terminator */                                         \
    char buf[24];                                                                                                     \
    auto l = std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now_t));                            \
    snprintf(buf + l, sizeof(buf) - l, ".%03u", static_cast<unsigned int>(now_ms.time_since_epoch().count() % 1000)); \
    fprintf(stderr, "%s %s - %s:%d - %s: " fmt "\n", buf, LOG_TAG, __FILE__, __LINE__, __func__, ##args);             \
  } while (false)

#define LOG_VERBOSE(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_DEBUG(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_INFO(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_WARN(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_ERROR(...) LOGWRAPPER(__VA_ARGS__)
#define LOG_ALWAYS_FATAL(...) \
  do {                        \
    LOGWRAPPER(__VA_ARGS__);  \
    abort();                  \
  } while (false)

#endif /* LOG_VERBOE */

#endif /* defined(OS_ANDROID) */

#define ASSERT(condition)                                    \
  do {                                                       \
    if (!(condition)) {                                      \
      LOG_ALWAYS_FATAL("assertion '" #condition "' failed"); \
    }                                                        \
  } while (false)

#define ASSERT_LOG(condition, fmt, args...)                                 \
  do {                                                                      \
    if (!(condition)) {                                                     \
      LOG_ALWAYS_FATAL("assertion '" #condition "' failed - " fmt, ##args); \
    }                                                                       \
  } while (false)
