/*
 * Copyright (C) 2012-2017 Intel Corporation
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

#ifndef __LOGHELPERANDROID__
#define __LOGHELPERANDROID__

// System dependencies
#include <utils/Log.h>
#include "EnumPrinthelper.h"

#define ENV_CAMERA_HAL_DEBUG  "persist.vendor.camera.debug"
#define ENV_CAMERA_HAL_PERF   "persist.vendor.camera.perf"
#define ENV_CAMERA_HAL_DUMP   "persist.vendor.camera.dump"
// Properties for debugging.
#define ENV_CAMERA_HAL_DUMP_SKIP_NUM  "persist.vendor.camera.dump.skip"
#define ENV_CAMERA_HAL_DUMP_INTERVAL  "persist.vendor.camera.dump.gap"
#define ENV_CAMERA_HAL_DUMP_COUNT     "persist.vendor.camera.dump.cnt"
#define ENV_CAMERA_HAL_DUMP_PATH      "persist.vendor.camera.dump.path"

#ifdef RKCAMERA_REDEFINE_LOG

typedef enum {
    CAM_NO_MODULE,
    CAM_HAL_MODULE,
    CAM_JPEG_MODULE,
    CAM_LAST_MODULE
} cam_modules_t;

/* values that persist.vendor.camera.global.debug can be set to */
/* all camera modules need to map their internal debug levels to this range */
typedef enum {
    CAM_GLBL_DBG_NONE  = 0,
    CAM_GLBL_DBG_ERR   = 1,
    CAM_GLBL_DBG_WARN  = 2,
    CAM_GLBL_DBG_INFO  = 3,
    CAM_GLBL_DBG_DEBUG = 4,
    CAM_GLBL_DBG_HIGH  = 5,
    CAM_GLBL_DBG_LOW   = 6
} cam_global_debug_level_t;

extern int g_cam_log[CAM_LAST_MODULE][CAM_GLBL_DBG_LOW + 1];

/* #define FATAL_IF(cond, ...) LOG_ALWAYS_FATAL_IF(cond, ## __VA_ARGS__) */
#ifndef CAM_MODULE
#define CAM_MODULE CAM_HAL_MODULE
#endif

#undef CLOGx
#define CLOGx(module, level, fmt, args...)                         \
{\
if (g_cam_log[module][level]) {                                  \
  rk_camera_debug_log(module, level, LOG_TAG, fmt, ##args); \
}\
}

#undef CLOGI
#define CLOGI(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_INFO, fmt, ##args)
#undef CLOGD
#define CLOGD(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_DEBUG, fmt, ##args)
#undef CLOGL
#define CLOGL(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_LOW, fmt, ##args)
#undef CLOGW
#define CLOGW(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_WARN, fmt, ##args)
#undef CLOGH
#define CLOGH(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_HIGH, fmt, ##args)
#undef CLOGE
#define CLOGE(module, fmt, args...)                \
    CLOGx(module, CAM_GLBL_DBG_ERR, fmt, ##args)

#undef LOGD
#define LOGD(fmt, args...) CLOGD(CAM_MODULE, fmt, ##args)
#undef LOGL
#define LOGL(fmt, args...) CLOGL(CAM_MODULE, fmt, ##args)
#undef LOGW
#define LOGW(fmt, args...) CLOGW(CAM_MODULE, fmt, ##args)
#undef LOGH
#define LOGH(fmt, args...) CLOGH(CAM_MODULE, fmt, ##args)
#undef LOGE
#define LOGE(fmt, args...) CLOGE(CAM_MODULE, fmt, ##args)
#undef LOGI
#define LOGI(fmt, args...) CLOGI(CAM_MODULE, fmt, ##args)

class ScopedLog {
public:
inline ScopedLog(int level, const char* name) :
        mLevel(level),
        mName(name) {
            if (g_cam_log[CAM_MODULE][mLevel] == 1) {
                ALOGD("ENTER-%s", mName);
            }
}

inline ~ScopedLog() {
    if (g_cam_log[CAM_MODULE][mLevel] == 1) {
        ALOGD("EXIT-%s", mName);
    }
}

private:
    int mLevel;
    const char* mName;
};

/* reads and updates camera logging properties */
void rk_camera_set_dbg_log_properties(void);

/* generic logger function */
void rk_camera_debug_log(const cam_modules_t module,
                   const cam_global_debug_level_t level,
                   const char *tag, const char *fmt, ...);

void rk_camera_debug_open(void);
void rk_camera_debug_close(void);

#define LOG1(fmt, args...) LOGI(fmt, ##args)
#define LOG2(fmt, args...) LOGI(fmt, ##args)
#define LOGR(fmt, args...) LOGI(fmt, ##args)
#define LOGAIQ(fmt, args...) LOGI(fmt, ##args)
#define LOGV(fmt, args...) LOGI(fmt, ##args)

/* #undef LOGE */
/* #define LOGE(fmt, args...) LOGI(fmt, ##args) */
/* #undef LOGW */
/* #define LOGW(fmt, args...) LOGE(fmt, ##args) */
// CAMTRACE_NAME traces the beginning and end of the current scope.  To trace
// the correct start and end times this macro should be declared first in the
// scope body.
#define HAL_TRACE_NAME(level, name) ScopedLog __tracer(level, name )
#define HAL_TRACE_CALL(level) HAL_TRACE_NAME(level, __FUNCTION__)
#define HAL_TRACE_CALL_PRETTY(level) HAL_TRACE_NAME(level, __PRETTY_FUNCTION__)



#else

#undef LOGD
#define LOGD(fmt, args...) ALOGD(fmt, ##args)
#undef LOGL
#define LOGL(fmt, args...) ALOGD(fmt, ##args)
#undef LOGW
#define LOGW(fmt, args...) ALOGW(fmt, ##args)
#undef LOGH
#define LOGH(fmt, args...) ALOGD(fmt, ##args)
#undef LOGE
#define LOGE(fmt, args...) ALOGE(fmt, ##args)
#undef LOGI
#define LOGI(fmt, args...) ALOGV(fmt, ##args)



#define LOG1(...)
#define LOG2(...)
#define LOGR(...)
#define LOGAIQ(...)

#define LOGD(...)
#define LOGV(...)
#define LOGI(...)

#define HAL_TRACE_NAME(level, name)
#define HAL_TRACE_CALL(level)
#define HAL_TRACE_CALL_PRETTY(level)

#endif
#endif /* __LOGHELPERANDROID__ */
