/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <android-base/logging.h>
#include <hidl/HidlLazyUtils.h>
#include <hidl/HidlTransportSupport.h>

#include "gnss.h"

int main(int /* argc */, char* /* argv */ []) {
    ::android::sp<goldfish::Gnss20> gnss(new goldfish::Gnss20);

    ::android::hardware::configureRpcThreadpool(1, true);

    auto serviceRegistrar = ::android::hardware::LazyServiceRegistrar::getInstance();
    CHECK_EQ(serviceRegistrar.registerService(gnss), ::android::OK)
        << "Failed to register Gnss HAL";

    ::android::hardware::joinRpcThreadpool();
}
