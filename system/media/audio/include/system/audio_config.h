/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef ANDROID_AUDIO_CONFIG_H
#define ANDROID_AUDIO_CONFIG_H

#ifdef __cplusplus

#include <string>
#include <vector>

#include <cutils/properties.h>

namespace android {

// Returns a vector of paths where audio configuration files
// must be searched, in the provided order.
static inline std::vector<std::string> audio_get_configuration_paths() {
    static const std::vector<std::string> paths = []() {
        char value[PROPERTY_VALUE_MAX] = {};
        if (property_get("ro.boot.product.vendor.sku", value, "") <= 0) {
            return std::vector<std::string>({"/odm/etc", "/vendor/etc", "/system/etc"});
        } else {
            return std::vector<std::string>({
                    "/odm/etc", std::string("/vendor/etc/audio/sku_") + value,
                    "/vendor/etc", "/system/etc"});
        }
    }();
    return paths;
}

}  // namespace android

#endif  // __cplusplus

#endif  // ANDROID_AUDIO_CONFIG_H
