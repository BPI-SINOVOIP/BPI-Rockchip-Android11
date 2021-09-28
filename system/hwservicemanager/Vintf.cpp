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

#define LOG_TAG "hwservicemanager"
//#define LOG_NDEBUG 0

#include "Vintf.h"

#include <android-base/logging.h>
#include <vintf/parse_string.h>
#include <vintf/VintfObject.h>

namespace android {
namespace hardware {

vintf::Transport getTransportFromManifest(
        const FQName &fqName, const std::string &instanceName,
        const std::shared_ptr<const vintf::HalManifest>& vm) {
    if (vm == nullptr) {
        return vintf::Transport::EMPTY;
    }
    return vm->getHidlTransport(fqName.package(), fqName.getVersion(),
                                fqName.name(), instanceName);
}

vintf::Transport getTransport(const std::string &interfaceName, const std::string &instanceName) {
    FQName fqName;

    if (!FQName::parse(interfaceName, &fqName)) {
        LOG(ERROR) << __FUNCTION__ << ": " << interfaceName
                   << " is not a valid fully-qualified name.";
        return vintf::Transport::EMPTY;
    }
    if (!fqName.hasVersion()) {
        LOG(ERROR) << __FUNCTION__ << ": " << fqName.string()
                   << " does not specify a version.";
        return vintf::Transport::EMPTY;
    }
    if (fqName.name().empty()) {
        LOG(ERROR) << __FUNCTION__ << ": " << fqName.string()
                   << " does not specify an interface name.";
        return vintf::Transport::EMPTY;
    }

    vintf::Transport tr = getTransportFromManifest(fqName, instanceName,
            vintf::VintfObject::GetFrameworkHalManifest());
    if (tr != vintf::Transport::EMPTY) {
        return tr;
    }
    tr = getTransportFromManifest(fqName, instanceName,
            vintf::VintfObject::GetDeviceHalManifest());
    if (tr != vintf::Transport::EMPTY) {
        return tr;
    }

    LOG(INFO) << __FUNCTION__ << ": Cannot find entry " << fqName.string() << "/" << instanceName
              << " in either framework or device manifest.";
    return vintf::Transport::EMPTY;
}

static void insertManifestInstances(const FQName& fqName,
                                    const std::shared_ptr<const vintf::HalManifest>& manifest,
                                    const std::string& manifestType,
                                    std::set<std::string>* toSet) {
    if (manifest == nullptr) {
        LOG(ERROR) << "Device is missing " << manifestType << " manifest.";
        return;
    }

    std::set<std::string> manifestSet = manifest->getHidlInstances(
        fqName.package(), fqName.getVersion(), fqName.name());

    toSet->insert(manifestSet.begin(), manifestSet.end());
}

std::set<std::string> getInstances(const std::string& interfaceName) {
    FQName fqName;
    if (!FQName::parse(interfaceName, &fqName) || !fqName.isFullyQualified() ||
            fqName.isValidValueName() || !fqName.isInterfaceName()) {
        LOG(ERROR) << __FUNCTION__ << ": " << interfaceName
                   << " is not a valid fully-qualified name.";
        return {};
    }

    std::set<std::string> ret;

    insertManifestInstances(
        fqName, vintf::VintfObject::GetDeviceHalManifest(), "device", &ret);
    insertManifestInstances(
        fqName, vintf::VintfObject::GetFrameworkHalManifest(), "framework", &ret);

    return ret;
}


}  // hardware
}  // android
