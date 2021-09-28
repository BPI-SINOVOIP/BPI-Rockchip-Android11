/*
 * Copyright 2018 Rockchip Electronics Co. LTD
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
 *
 * author: martin.cheng@rock-chips.com
 *   date: 20180704
 */

#ifndef INCLUDE_RT_BASE_RT_LOG_H_
#define INCLUDE_RT_BASE_RT_LOG_H_

#include "rt_header.h"   // NOLINT
#include <string.h>      // NOLINT

#ifndef LOG_TAG
#define LOG_TAG NULL
#endif

#define LOG_FLAG_ON   1
#define LOG_FLAG_OFF  0

//! super macro.
#define RT_LOGD_IF(condition, format, ...) if (condition > 0) LOG_TRACE(format, ##__VA_ARGS__)
#define RT_LOGE_IF(condition, format, ...) if (condition > 0) LOG_ERROR(format, ##__VA_ARGS__)

//! super macro.
#define RT_LOGT(format, ...) LOG_TRACE(format, ##__VA_ARGS__)
#define RT_LOGD(format, ...) LOG_TRACE(format, ##__VA_ARGS__)
#define RT_LOGE(format, ...) LOG_ERROR(format, ##__VA_ARGS__)

//! super macro.
#define LOG_TRACE(fmt, ...) \
        rt_log(LOG_TAG, fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
        rt_err(LOG_TAG, fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)

//! log function marcro.
// external api
#define RT_LOG_API_ENTER()   LOG_TRACE("%s called enter", __FUNCTION__)
#define RT_LOG_API_LEAVE()   LOG_TRACE("%s called leave", __FUNCTION__)
#define RT_LOG_API_CALLED()  LOG_TRACE("%s called", __FUNCTION__)

// internal func
#define RT_LOG_FUNC_ENTER()  LOG_TRACE("%s enter", __FUNCTION__)
#define RT_LOG_FUNC_LEAVE()  LOG_TRACE("%s leave", __FUNCTION__)
#define RT_LOG_FUNC_CALLED() LOG_TRACE("%s called", __FUNCTION__)

// construct/destruct
#define RT_LOG_CONSTRUCT_IN(thiz)  LOG_TRACE("%s(%p) construct", __FUNCTION__, thiz)
#define RT_LOG_DESTRUCT_DONE(thiz) LOG_TRACE("%s(%p) destructor ok", __FUNCTION__, thiz)


#ifdef __cplusplus
extern "C" {
#endif

void rt_set_log_flag(UINT32 flag);
void rt_log(const char *tag, const char *fmt, const char *fname,
                            const UINT16 row, ...);
void rt_err(const char *tag, const char *fmt, const char *fname,
                            const UINT16 row, ...);

#ifdef __cplusplus
}
#endif
#endif  // INCLUDE_RT_BASE_RT_LOG_H_
