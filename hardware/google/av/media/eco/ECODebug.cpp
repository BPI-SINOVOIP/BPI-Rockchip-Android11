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

#include "eco/ECODebug.h"

namespace android {
namespace media {
namespace eco {

uint32_t gECOLogLevel = 0;

void updateLogLevel() {
    gECOLogLevel = property_get_int32(kDebugLogsLevelProperty, 0);
    ALOGI("ECOService log level is %d", gECOLogLevel);
}

}  // namespace eco
}  // namespace media
}  // namespace android
