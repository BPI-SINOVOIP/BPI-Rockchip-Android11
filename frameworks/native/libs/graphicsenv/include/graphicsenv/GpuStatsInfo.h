/*
 * Copyright 2019 The Android Open Source Project
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

#pragma once

#include <string>
#include <vector>

#include <binder/Parcelable.h>

namespace android {

/*
 * class for transporting gpu global stats from GpuService to authorized
 * recipents. This class is intended to be a data container.
 */
class GpuStatsGlobalInfo : public Parcelable {
public:
    GpuStatsGlobalInfo() = default;
    GpuStatsGlobalInfo(const GpuStatsGlobalInfo&) = default;
    virtual ~GpuStatsGlobalInfo() = default;
    virtual status_t writeToParcel(Parcel* parcel) const;
    virtual status_t readFromParcel(const Parcel* parcel);
    std::string toString() const;

    std::string driverPackageName = "";
    std::string driverVersionName = "";
    uint64_t driverVersionCode = 0;
    int64_t driverBuildTime = 0;
    int32_t glLoadingCount = 0;
    int32_t glLoadingFailureCount = 0;
    int32_t vkLoadingCount = 0;
    int32_t vkLoadingFailureCount = 0;
    int32_t vulkanVersion = 0;
    int32_t cpuVulkanVersion = 0;
    int32_t glesVersion = 0;
    int32_t angleLoadingCount = 0;
    int32_t angleLoadingFailureCount = 0;
};

/*
 * class for transporting gpu app stats from GpuService to authorized recipents.
 * This class is intended to be a data container.
 */
class GpuStatsAppInfo : public Parcelable {
public:
    GpuStatsAppInfo() = default;
    GpuStatsAppInfo(const GpuStatsAppInfo&) = default;
    virtual ~GpuStatsAppInfo() = default;
    virtual status_t writeToParcel(Parcel* parcel) const;
    virtual status_t readFromParcel(const Parcel* parcel);
    std::string toString() const;

    std::string appPackageName = "";
    uint64_t driverVersionCode = 0;
    std::vector<int64_t> glDriverLoadingTime = {};
    std::vector<int64_t> vkDriverLoadingTime = {};
    std::vector<int64_t> angleDriverLoadingTime = {};
    bool cpuVulkanInUse = false;
    bool falsePrerotation = false;
    bool gles1InUse = false;
};

/*
 * class for holding the gpu stats in GraphicsEnv before sending to GpuService.
 */
class GpuStatsInfo {
public:
    enum Api {
        API_GL = 0,
        API_VK = 1,
    };

    enum Driver {
        NONE = 0,
        GL = 1,
        GL_UPDATED = 2,
        VULKAN = 3,
        VULKAN_UPDATED = 4,
        ANGLE = 5,
    };

    enum Stats {
        CPU_VULKAN_IN_USE = 0,
        FALSE_PREROTATION = 1,
        GLES_1_IN_USE = 2,
    };

    GpuStatsInfo() = default;
    GpuStatsInfo(const GpuStatsInfo&) = default;
    virtual ~GpuStatsInfo() = default;

    std::string driverPackageName = "";
    std::string driverVersionName = "";
    uint64_t driverVersionCode = 0;
    int64_t driverBuildTime = 0;
    std::string appPackageName = "";
    int32_t vulkanVersion = 0;
    Driver glDriverToLoad = Driver::NONE;
    Driver glDriverFallback = Driver::NONE;
    Driver vkDriverToLoad = Driver::NONE;
    Driver vkDriverFallback = Driver::NONE;
    bool glDriverToSend = false;
    bool vkDriverToSend = false;
    int64_t glDriverLoadingTime = 0;
    int64_t vkDriverLoadingTime = 0;
};

} // namespace android
