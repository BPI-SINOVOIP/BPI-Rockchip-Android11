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

#define LOG_TAG "PackageInfo"

#include "PackageInfo.h"
#include <android-base/logging.h>
#include <android/content/pm/IPackageManagerNative.h>
#include <binder/IServiceManager.h>
#include <string>
#include <vector>

bool ANeuralNetworks_fetch_PackageInfo(uid_t uid, ANeuralNetworks_PackageInfo* appPackageInfo) {
    if (appPackageInfo == nullptr) {
        LOG(ERROR) << "appPackageInfo can't be a nullptr";
        return false;
    }

    ::android::sp<::android::IServiceManager> sm(::android::defaultServiceManager());
    ::android::sp<::android::IBinder> binder(sm->getService(::android::String16("package_native")));
    if (binder == nullptr) {
        LOG(ERROR) << "getService package_native failed";
        return false;
    }

    ::android::sp<::android::content::pm::IPackageManagerNative> packageMgr =
            ::android::interface_cast<::android::content::pm::IPackageManagerNative>(binder);
    std::vector<int> uids{static_cast<int>(uid)};
    std::vector<std::string> names;
    ::android::binder::Status status = packageMgr->getNamesForUids(uids, &names);
    if (!status.isOk()) {
        LOG(ERROR) << "package_native::getNamesForUids failed: "
                   << status.exceptionMessage().c_str();
        return false;
    }
    const std::string& packageName = names[0];

    int flags = 0;
    status = packageMgr->getLocationFlags(packageName, &flags);
    if (!status.isOk()) {
        LOG(ERROR) << "package_native::getLocationFlags failed: "
                   << status.exceptionMessage().c_str();
        return false;
    }

    appPackageInfo->appPackageName = new char[packageName.size() + 1];
    memcpy(appPackageInfo->appPackageName, packageName.c_str(), packageName.size() + 1);

    // isSystemApp()
    appPackageInfo->appIsSystemApp =
            ((flags & ::android::content::pm::IPackageManagerNative::LOCATION_SYSTEM) != 0);
    // isVendor()
    appPackageInfo->appIsOnVendorImage =
            ((flags & ::android::content::pm::IPackageManagerNative::LOCATION_VENDOR) != 0);
    // isProduct()
    appPackageInfo->appIsOnProductImage =
            ((flags & ::android::content::pm::IPackageManagerNative::LOCATION_PRODUCT) != 0);
    return true;
}

void ANeuralNetworks_free_PackageInfo(ANeuralNetworks_PackageInfo* appPackageInfo) {
    if (appPackageInfo != nullptr) {
        if (appPackageInfo->appPackageName != nullptr) {
            delete[] appPackageInfo->appPackageName;
        }
    }
}
