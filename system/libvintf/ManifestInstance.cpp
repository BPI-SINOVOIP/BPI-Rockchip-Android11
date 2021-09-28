/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef LIBVINTF_TARGET
#define LOG_TAG "libvintf"
#include <android-base/logging.h>
#endif

#include "ManifestInstance.h"

#include <utility>

#include "parse_string.h"

namespace android {
namespace vintf {

ManifestInstance::ManifestInstance() = default;

ManifestInstance::ManifestInstance(const ManifestInstance&) = default;

ManifestInstance::ManifestInstance(ManifestInstance&&) noexcept = default;

ManifestInstance& ManifestInstance::operator=(const ManifestInstance&) = default;

ManifestInstance& ManifestInstance::operator=(ManifestInstance&&) noexcept = default;

ManifestInstance::ManifestInstance(FqInstance&& fqInstance, TransportArch&& ta, HalFormat fmt)
    : mFqInstance(std::move(fqInstance)), mTransportArch(std::move(ta)), mHalFormat(fmt) {}
ManifestInstance::ManifestInstance(const FqInstance& fqInstance, const TransportArch& ta,
                                   HalFormat fmt)
    : mFqInstance(fqInstance), mTransportArch(ta), mHalFormat(fmt) {}

const std::string& ManifestInstance::package() const {
    return mFqInstance.getPackage();
}

Version ManifestInstance::version() const {
    return mFqInstance.getVersion();
}

const std::string& ManifestInstance::interface() const {
    return mFqInstance.getInterface();
}

const std::string& ManifestInstance::instance() const {
    return mFqInstance.getInstance();
}

Transport ManifestInstance::transport() const {
    return mTransportArch.transport;
}

Arch ManifestInstance::arch() const {
    return mTransportArch.arch;
}

HalFormat ManifestInstance::format() const {
    return mHalFormat;
}

const FqInstance& ManifestInstance::getFqInstance() const {
    return mFqInstance;
}

bool ManifestInstance::operator==(const ManifestInstance& other) const {
    return mFqInstance == other.mFqInstance && mTransportArch == other.mTransportArch &&
           mHalFormat == other.mHalFormat;
}
bool ManifestInstance::operator<(const ManifestInstance& other) const {
    if (mFqInstance < other.mFqInstance) return true;
    if (other.mFqInstance < mFqInstance) return false;
    if (mTransportArch < other.mTransportArch) return true;
    if (other.mTransportArch < mTransportArch) return false;
    return mHalFormat < other.mHalFormat;
}

std::string ManifestInstance::getSimpleFqInstance() const {
    FqInstance e;
    bool success = false;
    switch (format()) {
        case HalFormat::AIDL: {
            // Hide fake version when printing human-readable message or to manifest XML.
            success = e.setTo(interface(), instance());
        } break;
        case HalFormat::HIDL:
            [[fallthrough]];
        case HalFormat::NATIVE: {
            success = e.setTo(version().majorVer, version().minorVer, interface(), instance());
        } break;
    }
#ifndef LIBVINTF_TARGET
    CHECK(success) << "Cannot get simple fqinstnance from '" << mFqInstance.string() << "'";
#endif
    return success ? e.string() : "";
}

std::string ManifestInstance::description() const {
    switch (format()) {
        case HalFormat::AIDL: {
            return toAidlFqnameString(package(), interface(), instance());
        } break;
        case HalFormat::HIDL:
            [[fallthrough]];
        case HalFormat::NATIVE: {
            return getFqInstance().string();
        } break;
    }
}

}  // namespace vintf
}  // namespace android
