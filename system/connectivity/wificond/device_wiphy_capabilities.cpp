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

#include "device_wiphy_capabilities.h"

#include <android-base/logging.h>

#include "wificond/parcelable_utils.h"

using android::status_t;

namespace android {
namespace net {
namespace wifi {
namespace nl80211 {

DeviceWiphyCapabilities::DeviceWiphyCapabilities() {
  is80211nSupported_ = false;
  is80211acSupported_ = false;
  is80211axSupported_ = false;
  is160MhzSupported_ = false;
  is80p80MhzSupported_ = false;
  maxTxStreams_ = 1;
  maxRxStreams_ = 1;
}

status_t DeviceWiphyCapabilities::writeToParcel(::android::Parcel* parcel) const {
  RETURN_IF_FAILED(parcel->writeBool(is80211nSupported_));
  RETURN_IF_FAILED(parcel->writeBool(is80211acSupported_));
  RETURN_IF_FAILED(parcel->writeBool(is80211axSupported_));
  RETURN_IF_FAILED(parcel->writeBool(is160MhzSupported_ ));
  RETURN_IF_FAILED(parcel->writeBool(is80p80MhzSupported_));
  RETURN_IF_FAILED(parcel->writeUint32(maxTxStreams_));
  RETURN_IF_FAILED(parcel->writeUint32(maxRxStreams_));
  return ::android::OK;
}

status_t DeviceWiphyCapabilities::readFromParcel(const ::android::Parcel* parcel) {
  RETURN_IF_FAILED(parcel->readBool(&is80211nSupported_));
  RETURN_IF_FAILED(parcel->readBool(&is80211acSupported_));
  RETURN_IF_FAILED(parcel->readBool(&is80211axSupported_));
  RETURN_IF_FAILED(parcel->readBool(&is160MhzSupported_));
  RETURN_IF_FAILED(parcel->readBool(&is80p80MhzSupported_));
  RETURN_IF_FAILED(parcel->readUint32(&maxTxStreams_));
  RETURN_IF_FAILED(parcel->readUint32(&maxRxStreams_));

  return ::android::OK;
}

}  // namespace nl80211
}  // namespace wifi
}  // namespace net
}  // namespace android
