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

#ifndef CHPP_LOG_H_
#define CHPP_LOG_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

// TODO: consider switching to a PAL interface to assist platform adaptation
#define CHPP_LINUX_LOG(level, color, fmt, ...)                         \
  printf("\e[" color "m%s %s:%d\t" fmt "\e[0m\n", level, __FILENAME__, \
         __LINE__, ##__VA_ARGS__)

#define LOGE(fmt, ...) CHPP_LINUX_LOG("E", "91", fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) CHPP_LINUX_LOG("W", "93", fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) CHPP_LINUX_LOG("I", "96", fmt, ##__VA_ARGS__)
#define LOGD(fmt, ...) CHPP_LINUX_LOG("D", "97", fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif  // CHPP_LOG_H_
