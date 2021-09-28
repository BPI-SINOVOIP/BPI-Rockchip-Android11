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

#define LOG_TAG "android.hardware.tv.cec@1.0-service-shim"

#include <android/hardware/tv/cec/1.0/IHdmiCec.h>
#include <hidl/LegacySupport.h>
#include "HdmiCecMock.h"

using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::hardware::tv::cec::V1_0::IHdmiCec;
using android::hardware::tv::cec::V1_0::implementation::HdmiCecMock;

int main() {
    configureRpcThreadpool(8, true /* callerWillJoin */);

    // Setup hwbinder service
    android::sp<IHdmiCec> service = new HdmiCecMock();
    android::status_t status;
    status = service->registerAsService();
    LOG_ALWAYS_FATAL_IF(status != android::OK, "Error while registering mock cec service: %d",
                        status);

    joinRpcThreadpool();
    return 0;
}
