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

#pragma once

#include <android-base/macros.h>
#include <keymasterV4_1/Keymaster.h>
#include <keymasterV4_1/authorization_set.h>

#include <memory>
#include <string>
#include <utility>

namespace android {
namespace kernel {

namespace km {

using namespace ::android::hardware::keymaster::V4_1;

// Surprisingly -- to me, at least -- this is totally fine.  You can re-define
// symbols that were brought in via a using directive (the "using namespace")
// above.  In general this seems like a dangerous thing to rely on, but in this
// case its implications are simple and straightforward: km::ErrorCode refers to
// the 4.0 ErrorCode, though we pull everything else from 4.1.
using ErrorCode = ::android::hardware::keymaster::V4_0::ErrorCode;
using V4_1_ErrorCode = ::android::hardware::keymaster::V4_1::ErrorCode;

}  // namespace km

using KmDevice = km::support::Keymaster;

// Wrapper for a Keymaster device
class Keymaster {
 public:
  Keymaster();
  // false if we failed to open the keymaster device.
  explicit operator bool() { return mDevice.get() != nullptr; }
  // Generate a key in the keymaster from the given params.
  bool generateKey(const km::AuthorizationSet& inParams, std::string* key);
  // Import a key into the keymaster
  bool importKey(const km::AuthorizationSet& inParams, km::KeyFormat format,
                 const std::string& key, std::string* outKeyBlob);
  // Exports a keymaster key with STORAGE_KEY tag wrapped with a per-boot
  // ephemeral key
  bool exportKey(const std::string& kmKey, std::string* key);
  // If the keymaster supports it, permanently delete a key.
  bool deleteKey(const std::string& key);
  // Replace stored key blob in response to KM_ERROR_KEY_REQUIRES_UPGRADE.
  bool upgradeKey(const std::string& oldKey,
                  const km::AuthorizationSet& inParams, std::string* newKey);

 private:
  android::sp<KmDevice> mDevice;
  DISALLOW_COPY_AND_ASSIGN(Keymaster);
};

}  // namespace kernel
}  // namespace android
