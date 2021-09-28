/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef __VTS_SYSFUZZER_COMMON_UTILS_IFSPECUTIL_H__
#define __VTS_SYSFUZZER_COMMON_UTILS_IFSPECUTIL_H__

#include <string>

#include "test/vts/proto/ComponentSpecificationMessage.pb.h"

#define VTS_INTERFACE_SPECIFICATION_FUNCTION_NAME_PREFIX "vts_func_"

using namespace std;

namespace android {
namespace vts {

// Reads the given file and parse the file contents into a
// ComponentSpecificationMessage.
bool ParseInterfaceSpec(const char* file_path,
                        ComponentSpecificationMessage* message);

// Returns the function name prefix of a given interface specification.
string GetFunctionNamePrefix(const ComponentSpecificationMessage& message);

// Get HAL version (represented by two integers) string to be used to
// build a relevant dir path.
//
// Args:
//     version_major: int, HAL major version, e.g. 1.10 -> 1.
//     version_minor: int, HAL minor version, e.g. 1.10 -> 10.
//     for_macro: bool, if true, it returns version 1.10 as V1_10
//
// Returns:
//     string, for version 1.10, if for_macro is true, it returns V1_10,
//     otherwise, it returns 1.10.
string GetVersionString(int version_major, int version_minor,
                        bool for_macro = false);

// deprecated
// Get HAL version string to be used to build a relevant dir path.
//
// Args:
//     version: float, HAL version, e.g. 1.10.
//     for_macro: bool, if true, it returns version 1.10 as V1_10.
//
// Returns:
//     string, for version 1.10, if for_macro is true, it returns V1_10,
//     otherwise, it returns 1.10.
string GetVersionString(float version, bool for_macro=false);

// Get the driver library name for a given HIDL HAL.
//
// Args:
//     package_name: string, name of target package.
//     version_major: int, hal major version, e.g. 1.10 -> 1.
//     version_minor: int, hal minor version, e.g. 1.10 -> 10.
//
// Returns:
//     string, the driver lib name built from the arguments.
string GetHidlHalDriverLibName(const string& package_name,
                               const int version_major,
                               const int version_minor);

// Get the FQName for a given HIDL HAL.
//
// Args:
//     package_name: string, name of target package.
//     version_major: int, hal major version, e.g. 1.10 -> 1.
//     version_minor: int, hal minor version, e.g. 1.10 -> 10.
//     interface_name: string, name of target interface.
//
// Returns:
//     string, the interface FQ name built from the arguments.
string GetInterfaceFQName(const string& package_name, const int version_major,
                          const int version_minor,
                          const string& interface_name);

// Extract package name from full hidl type name
// e.g. ::android::hardware::nfc::V1_0::INfc -> android.hardware.nfc
string GetPackageName(const string& type_name);

// Extract version from full hidl type name
// e.g. ::android::hardware::nfc::V1_0::INfc -> "1_0"
string GetVersion(const string& type_name);

// Extract major version from version string
// for_macro is true if the input version string has "_"
// e.g. "1_10" -> 1 if for_macro is true
//      "1.10" -> 1 if for_macro is false
int GetVersionMajor(const string& version, bool for_macro = false);

// Extract minor version from version string
// for_macro is true if the input version string has "_"
// e.g. "1_10" -> 10 if for_macro is true
//      "1.10" -> 10 if for_macro is false
int GetVersionMinor(const string& version, bool for_macro = false);

// Extract component name from full hidl type name
// e.g. ::android::hardware::nfc::V1_0::INfc -> INfc
string GetComponentName(const string& type_name);

}  // namespace vts
}  // namespace android

#endif  // __VTS_SYSFUZZER_COMMON_UTILS_IFSPECUTIL_H__
