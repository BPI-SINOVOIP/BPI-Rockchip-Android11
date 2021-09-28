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

#include <vintf/Version.h>
#include <vintf/VersionRange.h>

namespace android {
namespace vintf {
namespace details {

// All <version> tags in <hal format="aidl"> tags are ignored, and an implicit version
// is inserted so that compatibility checks for different HAL formats can be unified.
// This is an implementation detail of libvintf and won't be written to actual XML files.
// 0.0 is not used because FQName / FqInstance consider it an invalid value.
static constexpr VersionRange kFakeAidlVersionRange{SIZE_MAX, SIZE_MAX};
static constexpr Version kFakeAidlVersion = kFakeAidlVersionRange.minVer();

}  // namespace details
}  // namespace vintf
}  // namespace android
