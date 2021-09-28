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

#ifndef ANDROID_FRAMEWORKS_ML_NN_RUNTIME_PACKAGEINFO_PACKAGE_INFO_H
#define ANDROID_FRAMEWORKS_ML_NN_RUNTIME_PACKAGEINFO_PACKAGE_INFO_H

#include <sys/types.h>

//
// Define a C interface to the neuralnetworks APEX helper functionality
//

__BEGIN_DECLS

// Collection of app-related information retreived from Package Manager.
typedef struct ANeuralNetworks_PackageInfo {
    // Null-terminated package name (nullptr if not an Android app).
    // Referenced memory is allocated by the ANeuralNetworks_fetch_PackageInfo
    // method, and MUST be released by a ANeuralNetworks_free_PackageInfo call.
    char* appPackageName;

    // Is the app a system app? (false if not an Android app)
    bool appIsSystemApp;
    // Is the app preinstalled on vendor image? (false if not an Android app)
    bool appIsOnVendorImage;
    // Is the app preinstalled on product image? (false if not an Android app)
    bool appIsOnProductImage;
} ANeuralNetworks_PackageInfo;

// Query PackageManagerNative service about Android app properties.
// On success, it will allocate memory for PackageInfo fields, which must be
// released by a ANeuralNetworks_free_PackageInfo call
bool ANeuralNetworks_fetch_PackageInfo(uid_t uid, ANeuralNetworks_PackageInfo* appPackageInfo);

// Free memory allocated for PackageInfo fields (doesn't free the actual package info
// struct).
void ANeuralNetworks_free_PackageInfo(ANeuralNetworks_PackageInfo* appPackageInfo);

__END_DECLS

#endif  // ANDROID_FRAMEWORKS_ML_NN_RUNTIME_PACKAGEINFO_PACKAGE_INFO_H
