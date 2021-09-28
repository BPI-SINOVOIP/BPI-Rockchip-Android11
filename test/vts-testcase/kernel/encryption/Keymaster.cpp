/*
 * Copyright (C) 2016 The Android Open Source Project
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

// TODO(154013771): this is copied from vold and modified to remove un-needed
// methods and use std::string instead of KeyBuffer. We should instead
// create a library to support both.

#include "Keymaster.h"

#include <android-base/logging.h>
#include <keymasterV4_1/authorization_set.h>
#include <keymasterV4_1/keymaster_utils.h>

namespace android {
namespace kernel {

using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::keymaster::V4_0::SecurityLevel;

Keymaster::Keymaster() {
  auto devices = KmDevice::enumerateAvailableDevices();
  for (auto& dev : devices) {
    // Do not use StrongBox for device encryption / credential encryption.  If a
    // security chip is present it will have Weaver, which already strengthens
    // CE.  We get no additional benefit from using StrongBox here, so skip it.
    if (dev->halVersion().securityLevel != SecurityLevel::STRONGBOX) {
      mDevice = std::move(dev);
      break;
    }
  }
  if (!mDevice) return;
  auto& version = mDevice->halVersion();
  LOG(INFO) << "Using " << version.keymasterName << " from "
            << version.authorName << " for encryption.  Security level: "
            << toString(version.securityLevel)
            << ", HAL: " << mDevice->descriptor() << "/"
            << mDevice->instanceName();
}

bool Keymaster::generateKey(const km::AuthorizationSet& inParams,
                            std::string* key) {
  km::ErrorCode km_error;
  auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<uint8_t>& keyBlob,
                    const km::KeyCharacteristics& /*ignored*/) {
    km_error = ret;
    if (km_error != km::ErrorCode::OK) return;
    if (key)
      key->assign(reinterpret_cast<const char*>(&keyBlob[0]), keyBlob.size());
  };

  auto error = mDevice->generateKey(inParams.hidl_data(), hidlCb);
  if (!error.isOk()) {
    LOG(ERROR) << "generate_key failed: " << error.description();
    return false;
  }
  if (km_error != km::ErrorCode::OK) {
    LOG(ERROR) << "generate_key failed, code " << int32_t(km_error);
    return false;
  }
  return true;
}

bool Keymaster::importKey(const km::AuthorizationSet& inParams,
                          km::KeyFormat format, const std::string& key,
                          std::string* outKeyBlob) {
  km::ErrorCode km_error;
  auto hidlCb = [&](km::ErrorCode ret, const hidl_vec<uint8_t>& keyBlob,
                    const km::KeyCharacteristics& /*ignored*/) {
    km_error = ret;
    if (km_error != km::ErrorCode::OK) return;
    if (outKeyBlob)
      outKeyBlob->assign(reinterpret_cast<const char*>(&keyBlob[0]),
                         keyBlob.size());
  };
  hidl_vec<uint8_t> hidlKey;
  hidlKey.setToExternal(
      const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(key.data())),
      static_cast<size_t>(key.size()));

  auto error =
      mDevice->importKey(inParams.hidl_data(), format, hidlKey, hidlCb);
  if (!error.isOk()) {
    LOG(ERROR) << "importKey failed: " << error.description();
    return false;
  }
  if (km_error != km::ErrorCode::OK) {
    LOG(ERROR) << "importKey failed, code " << int32_t(km_error);
    return false;
  }
  return true;
}

bool Keymaster::exportKey(const std::string& kmKey, std::string* key) {
  auto kmKeyBlob =
      km::support::blob2hidlVec(std::string(kmKey.data(), kmKey.size()));
  km::ErrorCode km_error;
  auto hidlCb = [&](km::ErrorCode ret,
                    const hidl_vec<uint8_t>& exportedKeyBlob) {
    km_error = ret;
    if (km_error != km::ErrorCode::OK) return;
    if (key)
      key->assign(reinterpret_cast<const char*>(&exportedKeyBlob[0]),
                  exportedKeyBlob.size());
  };
  auto error =
      mDevice->exportKey(km::KeyFormat::RAW, kmKeyBlob, {}, {}, hidlCb);
  if (!error.isOk()) {
    LOG(ERROR) << "export_key failed: " << error.description();
    return false;
  }
  if (km_error != km::ErrorCode::OK) {
    LOG(ERROR) << "export_key failed, code " << int32_t(km_error);
    return false;
  }
  return true;
}

bool Keymaster::deleteKey(const std::string& key) {
  auto keyBlob = km::support::blob2hidlVec(key);
  auto error = mDevice->deleteKey(keyBlob);
  if (!error.isOk()) {
    LOG(ERROR) << "delete_key failed: " << error.description();
    return false;
  }
  if (error != km::ErrorCode::OK) {
    LOG(ERROR) << "delete_key failed, code " << int32_t(km::ErrorCode(error));
    return false;
  }
  return true;
}

bool Keymaster::upgradeKey(const std::string& oldKey,
                           const km::AuthorizationSet& inParams,
                           std::string* newKey) {
  auto oldKeyBlob = km::support::blob2hidlVec(oldKey);
  km::ErrorCode km_error;
  auto hidlCb = [&](km::ErrorCode ret,
                    const hidl_vec<uint8_t>& upgradedKeyBlob) {
    km_error = ret;
    if (km_error != km::ErrorCode::OK) return;
    if (newKey)
      newKey->assign(reinterpret_cast<const char*>(&upgradedKeyBlob[0]),
                     upgradedKeyBlob.size());
  };
  auto error = mDevice->upgradeKey(oldKeyBlob, inParams.hidl_data(), hidlCb);
  if (!error.isOk()) {
    LOG(ERROR) << "upgrade_key failed: " << error.description();
    return false;
  }
  if (km_error != km::ErrorCode::OK) {
    LOG(ERROR) << "upgrade_key failed, code " << int32_t(km_error);
    return false;
  }
  return true;
}

}  // namespace kernel
}  // namespace android
