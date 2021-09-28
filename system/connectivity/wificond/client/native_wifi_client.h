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

#ifndef WIFICOND_NATIVE_WIFI_CLIENT_H_
#define WIFICOND_NATIVE_WIFI_CLIENT_H_

#include <vector>

#include <binder/Parcel.h>
#include <binder/Parcelable.h>

namespace android {
namespace net {
namespace wifi {
namespace nl80211 {

class NativeWifiClient : public ::android::Parcelable {
 public:
  NativeWifiClient() = default;
  bool operator==(const NativeWifiClient& rhs) const {
    return mac_address_ == rhs.mac_address_;
  }
  ::android::status_t writeToParcel(::android::Parcel* parcel) const override;
  ::android::status_t readFromParcel(const ::android::Parcel* parcel) override;

  std::vector<uint8_t> mac_address_;
};

}  // namespace nl80211
}  // namespace wifi
}  // namespace net
}  // namespace android

#endif  // WIFICOND_NATIVE_WIFI_CLIENT_H_
