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

#include <aidl/Vintf.h>

#include <android-base/logging.h>
#include <vintf/VintfObject.h>

namespace android {

std::vector<std::string> getAidlHalInstanceNames(const std::string& descriptor) {
    size_t lastDot = descriptor.rfind('.');
    CHECK(lastDot != std::string::npos) << "Invalid descriptor: " << descriptor;
    const std::string package = descriptor.substr(0, lastDot);
    const std::string iface = descriptor.substr(lastDot + 1);

    std::vector<std::string> ret;

    auto deviceManifest = vintf::VintfObject::GetDeviceHalManifest();
    for (const std::string& instance : deviceManifest->getAidlInstances(package, iface)) {
        ret.push_back(descriptor + "/" + instance);
    }

    auto frameworkManifest = vintf::VintfObject::GetFrameworkHalManifest();
    for (const std::string& instance : frameworkManifest->getAidlInstances(package, iface)) {
        ret.push_back(descriptor + "/" + instance);
    }

    return ret;
}

std::vector<std::string> getAidlHalInstanceNames(const String16& descriptor) {
    return getAidlHalInstanceNames(String8(descriptor).c_str());
}

}  // namespace android
