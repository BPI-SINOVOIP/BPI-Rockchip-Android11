/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "utils-fake.h"

namespace android {
namespace vintf {
namespace details {

MockRuntimeInfo::MockRuntimeInfo() {
    kernel_info_.mVersion = {3, 18, 31};
    kernel_info_.mConfigs = {{"CONFIG_64BIT", "y"},
                             {"CONFIG_ANDROID_BINDER_DEVICES", "\"binder,hwbinder\""},
                             {"CONFIG_ARCH_MMAP_RND_BITS", "24"},
                             {"CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES", "\"\""},
                             {"CONFIG_ILLEGAL_POINTER_VALUE", "0xdead000000000000"}};
    ON_CALL(*this, fetchAllInformation(_)).WillByDefault(Invoke(this, &MockRuntimeInfo::doFetch));
}

status_t MockRuntimeInfo::doFetch(RuntimeInfo::FetchFlags flags) {
    if (failNextFetch_) {
        failNextFetch_ = false;
        return android::UNKNOWN_ERROR;
    }

    if (flags & RuntimeInfo::FetchFlag::CPU_VERSION) {
        mOsName = "Linux";
        mNodeName = "localhost";
        mOsRelease = "3.18.31-g936f9a479d0f";
        mOsVersion = "#4 SMP PREEMPT Wed Feb 1 18:10:52 PST 2017";
        mHardwareId = "aarch64";
        mKernel.mVersion = kernel_info_.mVersion;
    }

    if (flags & RuntimeInfo::FetchFlag::POLICYVERS) {
        mKernelSepolicyVersion = 30;
    }

    if (flags & RuntimeInfo::FetchFlag::CONFIG_GZ) {
        mKernel.mConfigs = kernel_info_.mConfigs;
    }
    // fetchAllInformtion does not fetch kernel FCM version
    return OK;
}
void MockRuntimeInfo::setNextFetchKernelInfo(KernelVersion&& v,
                                             std::map<std::string, std::string>&& configs) {
    kernel_info_.mVersion = std::move(v);
    kernel_info_.mConfigs = std::move(configs);
}
void MockRuntimeInfo::setNextFetchKernelInfo(const KernelVersion& v,
                                             const std::map<std::string, std::string>& configs) {
    kernel_info_.mVersion = v;
    kernel_info_.mConfigs = configs;
}

}  // namespace details
}  // namespace vintf
}  // namespace android
