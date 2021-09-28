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

#define LOG_TAG "GCH_HalVendorTags"
#include <log/log.h>

#include "vendor_tag_defs.h"
#include "vendor_tag_utils.h"
#include "vendor_tags.h"

namespace android {
namespace google_camera_hal {
namespace hal_vendor_tag_utils {

status_t ModifyDefaultRequestSettings(RequestTemplate /*type*/,
                                      HalCameraMetadata* /*default_settings*/) {
  // Placeholder to modify default request settings with HAL vendor tag
  // default values
  return OK;
}

status_t ModifyCharacteristicsKeys(HalCameraMetadata* metadata) {
  if (metadata == nullptr) {
    ALOGE("%s: metadata is nullptr", __FUNCTION__);
    return BAD_VALUE;
  }

  camera_metadata_ro_entry entry;
  status_t res;
  // Get the request keys
  res = metadata->Get(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, &entry);
  if (res != OK) {
    ALOGE("%s: failed to get ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS",
          __FUNCTION__);
    return res;
  }
  std::vector<int32_t> request_keys(entry.data.i32,
                                    entry.data.i32 + entry.count);

  // Get the result keys
  res = metadata->Get(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, &entry);
  if (res != OK) {
    ALOGE("%s: failed to get ANDROID_REQUEST_AVAILABLE_RESULT_KEYS",
          __FUNCTION__);
    return res;
  }
  std::vector<int32_t> result_keys(entry.data.i32, entry.data.i32 + entry.count);

  // Get the session keys
  std::vector<int32_t> session_keys;
  res = metadata->Get(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, &entry);
  if (res == OK) {
    session_keys.insert(session_keys.end(), entry.data.i32, entry.data.i32 + entry.count);
  } else {
    ALOGW("%s: failed to get ANDROID_REQUEST_AVAILABLE_SESSION_KEYS",
          __FUNCTION__);
  }

  // Get the characteristic keys
  res = metadata->Get(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS, &entry);
  if (res != OK) {
    ALOGE("%s: failed to get ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS",
          __FUNCTION__);
    return res;
  }
  std::vector<int32_t> characteristics_keys(entry.data.i32,
                                            entry.data.i32 + entry.count);

  // VendorTagIds::kLogicalCamDefaultPhysicalId
  res = metadata->Get(ANDROID_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS, &entry);
  if (res == OK) {
    characteristics_keys.push_back(VendorTagIds::kLogicalCamDefaultPhysicalId);
  }
  // VendorTagIds::kHybridAeEnabled
  request_keys.push_back(VendorTagIds::kHybridAeEnabled);
  result_keys.push_back(VendorTagIds::kHybridAeEnabled);
  // VendorTagIds::kHdrPlusDisabled
  request_keys.push_back(VendorTagIds::kHdrPlusDisabled);
  result_keys.push_back(VendorTagIds::kHdrPlusDisabled);
  session_keys.push_back(VendorTagIds::kHdrPlusDisabled);
  // VendorTagIds::kHdrplusPayloadFrames
  characteristics_keys.push_back(VendorTagIds::kHdrplusPayloadFrames);
  // VendorTagIds::kHdrUsageMode
  characteristics_keys.push_back(VendorTagIds::kHdrUsageMode);
  // VendorTagIds::kProcessingMode
  request_keys.push_back(VendorTagIds::kProcessingMode);
  // VendorTagIds::kThermalThrottling
  request_keys.push_back(VendorTagIds::kThermalThrottling);
  // VendorTagIds::kOutputIntent
  request_keys.push_back(VendorTagIds::kOutputIntent);
  // VendorTagIds::kSensorModeFullFov
  request_keys.push_back(VendorTagIds::kSensorModeFullFov);
  result_keys.push_back(VendorTagIds::kSensorModeFullFov);
  session_keys.push_back(VendorTagIds::kSensorModeFullFov);

  // Update the static metadata with the new set of keys
  if (metadata->Set(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS, request_keys.data(),
                    request_keys.size()) != OK ||
      metadata->Set(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS, result_keys.data(),
                    result_keys.size()) != OK ||
      metadata->Set(ANDROID_REQUEST_AVAILABLE_SESSION_KEYS, session_keys.data(),
                    session_keys.size()) != OK ||
      metadata->Set(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                    characteristics_keys.data(),
                    characteristics_keys.size()) != OK) {
    ALOGE("%s Updating static metadata failed", __FUNCTION__);
    return UNKNOWN_ERROR;
  }

  return OK;
}
}  // namespace hal_vendor_tag_utils
}  // namespace google_camera_hal
}  // namespace android
