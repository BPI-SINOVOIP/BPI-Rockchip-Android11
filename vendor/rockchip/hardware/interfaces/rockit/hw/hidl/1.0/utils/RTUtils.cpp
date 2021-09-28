/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "RTUtils"

#include <dlfcn.h>
#include <android-base/logging.h>
#include <hidl/HidlBinderSupport.h>
#include "RTUtils.h"

namespace rockchip {
namespace hardware {
namespace rockit {
namespace hw {
namespace V1_0 {
namespace utils {

int toStatusT(Status status) {
    switch (status) {
    case Status::OK:
        return 0;
    default:
        return -1;
    }
}

hidl_vec<uint8_t> inHidlBytes(void const* l, size_t size) {
    hidl_vec<uint8_t> t;
    t.setToExternal(static_cast<uint8_t*>(const_cast<void*>(l)), size, false);
    return t;
}

}  // namespace utils
}  // namespace V1_0
}  // namespace hw
}  // namespace rockit
}  // namespace hardware
}  // namespace rockchip

