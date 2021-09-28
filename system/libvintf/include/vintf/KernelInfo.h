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

#pragma once

#include <map>
#include <string>
#include <vector>

#include "MatrixKernel.h"
#include "Version.h"

namespace android {
namespace vintf {

namespace details {
class MockRuntimeInfo;
struct StaticRuntimeInfo;
}  // namespace details

// KernelInfo includes kernel-specific information on a device.
class KernelInfo {
   public:
    KernelInfo() = default;

    const KernelVersion& version() const;
    const std::map<std::string, std::string>& configs() const;
    Level level() const;

    // mVersion = x'.y'.z', minLts = x.y.z,
    // match if x == x' , y == y' , and z <= z'.
    bool matchKernelVersion(const KernelVersion& minLts) const;
    // return true if all kernel configs in matrixConfigs matches.
    bool matchKernelConfigs(const std::vector<KernelConfig>& matrixConfigs,
                            std::string* error = nullptr) const;
    // return vector of pointers to elements in "kernels" that this matches
    // kernel requirement specified.
    std::vector<const MatrixKernel*> getMatchedKernelRequirements(
        const std::vector<MatrixKernel>& kernels, Level kernelLevel,
        std::string* error = nullptr) const;
    bool operator==(const KernelInfo& other) const;

    // Merge information from "other".
    bool merge(KernelInfo* other, std::string* error = nullptr);

   private:
    friend class AssembleVintfImpl;
    friend class details::MockRuntimeInfo;
    friend struct details::StaticRuntimeInfo;
    friend struct KernelInfoConverter;
    friend struct LibVintfTest;
    friend struct RuntimeInfoFetcher;
    friend struct RuntimeInfo;

    std::vector<const MatrixKernel*> getMatchedKernelVersionAndConfigs(
        const std::vector<const MatrixKernel*>& kernels, std::string* error) const;

    // x.y.z
    KernelVersion mVersion;
    // /proc/config.gz
    // Key: CONFIG_xxx; Value: the value after = sign.
    std::map<std::string, std::string> mConfigs;
    // Kernel FCM version
    Level mLevel = Level::UNSPECIFIED;
};

}  // namespace vintf
}  // namespace android
