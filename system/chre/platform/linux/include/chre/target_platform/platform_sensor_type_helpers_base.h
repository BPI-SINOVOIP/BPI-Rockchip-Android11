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

#ifndef CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_
#define CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_

namespace chre {

/**
 * Can be used to expose static methods to the PlatformSensorTypeHelpers class
 * for use in working with vendor sensor types. Currently, this is unused in the
 * Linux implementation as sensors are not supported.
 */
class PlatformSensorTypeHelpersBase {};

}  // namespace chre

#endif  // CHRE_TARGET_PLATFORM_SENSOR_TYPE_HELPERS_BASE_H_