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

#ifndef WIFICOND_DEVICE_WIPHY_CAPABILITIES_H_
#define WIFICOND_DEVICE_WIPHY_CAPABILITIES_H_

#include <binder/Parcel.h>
#include <binder/Parcelable.h>

namespace android {
namespace net {
namespace wifi {
namespace nl80211 {

class DeviceWiphyCapabilities : public ::android::Parcelable {
 public:
  DeviceWiphyCapabilities();
  bool operator==(const DeviceWiphyCapabilities& rhs) const {
    return (is80211nSupported_ == rhs.is80211nSupported_
            && is80211acSupported_ == rhs.is80211acSupported_
            && is80211axSupported_ == rhs.is80211axSupported_
            && is160MhzSupported_ == rhs.is160MhzSupported_
            && is80p80MhzSupported_ == rhs.is80p80MhzSupported_
            && maxTxStreams_ == rhs.maxTxStreams_
            && maxRxStreams_ == rhs.maxRxStreams_);
  }
  ::android::status_t writeToParcel(::android::Parcel* parcel) const override;
  ::android::status_t readFromParcel(const ::android::Parcel* parcel) override;

  bool is80211nSupported_;
  bool is80211acSupported_;
  bool is80211axSupported_;
  bool is160MhzSupported_;
  bool is80p80MhzSupported_;
  uint32_t maxTxStreams_;
  uint32_t maxRxStreams_;
};

}  // namespace nl80211
}  // namespace wifi
}  // namespace net
}  // namespace android

#endif  // WIFICOND_DEVICE_WIPHY_CAPABILITIES_H_
