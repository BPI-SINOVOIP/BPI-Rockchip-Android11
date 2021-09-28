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

#include <unistd.h>
#include <sys/capability.h>

#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android/hidl/manager/1.2/IServiceManager.h>
#include <android/system/wifi/keystore/1.0/IKeystore.h>
#include <hidl/HidlTransportSupport.h>

#include <wifikeystorehal/keystore.h>

#include "wifi_keystore_hal_connector.h"

using android::hardware::configureRpcThreadpool;
using android::system::wifi::keystore::V1_0::IKeystore;
using android::system::wifi::keystore::V1_0::implementation::Keystore;

namespace android {
namespace wificond {

void WifiKeystoreHalConnector::start() {
  /**
   * Register the wifi keystore HAL service to run in passthrough mode.
   * This will spawn off a new thread which will service the HIDL
   * transactions.
   */
  configureRpcThreadpool(1, false /* callerWillJoin */);
  android::sp<IKeystore> wifiKeystoreHalService = new Keystore();
  android::status_t err = wifiKeystoreHalService->registerAsService();
  CHECK(err == android::OK) << "Cannot register wifi keystore HAL service: " << err;
}
}  // namespace wificond
}  // namespace android

