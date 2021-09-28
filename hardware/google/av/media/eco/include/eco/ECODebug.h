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

#ifndef ANDROID_MEDIA_ECO_DEBUG_H_
#define ANDROID_MEDIA_ECO_DEBUG_H_

#include <cutils/atomic.h>
#include <cutils/properties.h>

#include "ECOData.h"

namespace android {
namespace media {
namespace eco {

static const char* kDisableEcoServiceProperty = "vendor.media.ecoservice.disable";
static const char* kDebugLogsLevelProperty = "vendor.media.ecoservice.log.level";
static const char* kDebugLogStats = "vendor.media.ecoservice.log.stats";
static const char* kDebugLogStatsSize = "vendor.media.ecoservice.log.stats.size";
static const char* kDebugLogInfos = "vendor.media.ecoservice.log.info";
static const char* kDebugLogInfosSize = "vendor.media.ecoservice.log.info.size";

// A debug variable that should only be accessed by ECOService through updateLogLevel. It is rare
// that this variable will have race condition. But if so, it is ok as this is just for debugging.
extern uint32_t gECOLogLevel;

enum ECOLogLevel : uint32_t {
    ECO_MSGLEVEL_DEBUG = 0x01,    ///< debug logs
    ECO_MSGLEVEL_VERBOSE = 0x02,  ///< very detailed logs
    ECO_MSGLEVEL_ALL = 0x03,      ///< both debug logs and detailed logs
};

#define ECOLOGV(format, args...) ALOGD_IF((gECOLogLevel & ECO_MSGLEVEL_VERBOSE), format, ##args)
#define ECOLOGD(format, args...) ALOGD_IF((gECOLogLevel & ECO_MSGLEVEL_DEBUG), format, ##args)
#define ECOLOGI(format, args...) ALOGI(format, ##args)
#define ECOLOGW(format, args...) ALOGW(format, ##args)
#define ECOLOGE(format, args...) ALOGE(format, ##args)

void updateLogLevel();

// Convenience methods for constructing binder::Status objects for error returns
#define STATUS_ERROR(errorCode, errorString)  \
    binder::Status::fromServiceSpecificError( \
            errorCode, String8::format("%s:%d: %s", __FUNCTION__, __LINE__, errorString))

#define STATUS_ERROR_FMT(errorCode, errorString, ...) \
    binder::Status::fromServiceSpecificError(         \
            errorCode,                                \
            String8::format("%s:%d: " errorString, __FUNCTION__, __LINE__, __VA_ARGS__))

}  // namespace eco
}  // namespace media
}  // namespace android

#endif  // ANDROID_MEDIA_ECO_DEBUG_H_
