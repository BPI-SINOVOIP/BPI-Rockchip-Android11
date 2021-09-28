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

#ifndef WIFICOND_WIFI_KEYSTORE_HAL_CONNECTOR_H_
#define WIFICOND_WIFI_KEYSTORE_HAL_CONNECTOR_H_

namespace android {
namespace wificond {

// Class for loading the wifi keystore HAL service.
class WifiKeystoreHalConnector {
 public:
  WifiKeystoreHalConnector() = default;
  ~WifiKeystoreHalConnector() = default;

  void start();

  DISALLOW_COPY_AND_ASSIGN(WifiKeystoreHalConnector);
};

}  // namespace wificond
}  // namespace android

#endif  // WIFICOND_WIFI_KEYSTORE_HAL_CONNECTOR_H_
