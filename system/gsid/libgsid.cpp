//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android/gsi/IGsiService.h>
#include <binder/IServiceManager.h>
#include <libgsi/libgsi.h>

namespace android {
namespace gsi {

using android::sp;

sp<IGsiService> GetGsiService() {
    auto sm = android::defaultServiceManager();
    auto name = android::String16(kGsiServiceName);
    android::sp<android::IBinder> res = sm->waitForService(name);
    if (res) {
        return android::interface_cast<IGsiService>(res);
    }
    LOG(ERROR) << "Unable to GetGsiService";
    return nullptr;
}

}  // namespace gsi
}  // namespace android
