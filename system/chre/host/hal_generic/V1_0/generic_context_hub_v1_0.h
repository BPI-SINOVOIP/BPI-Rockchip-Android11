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

#ifndef ANDROID_HARDWARE_CONTEXTHUB_V1_0_CONTEXTHUB_H
#define ANDROID_HARDWARE_CONTEXTHUB_V1_0_CONTEXTHUB_H

#include "generic_context_hub_base.h"

namespace android {
namespace hardware {
namespace contexthub {
namespace V1_0 {
namespace implementation {

using ::android::hardware::contexthub::common::implementation::
    GenericContextHubBase;

class GenericContextHub : public GenericContextHubBase<V1_0::IContexthub> {};

// Put the implementation of this method in a cc file to help catch build issues
// at compile time.
extern "C" V1_0::IContexthub *HIDL_FETCH_IContexthub(const char * /* name */);

}  // namespace implementation
}  // namespace V1_0
}  // namespace contexthub
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_CONTEXTHUB_V1_0_CONTEXTHUB_H
