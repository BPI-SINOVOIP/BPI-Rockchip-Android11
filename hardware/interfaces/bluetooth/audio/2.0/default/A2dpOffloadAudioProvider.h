/*
 * Copyright 2018 The Android Open Source Project
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

#pragma once

#include "BluetoothAudioProvider.h"

namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {
namespace V2_0 {
namespace implementation {

class A2dpOffloadAudioProvider : public BluetoothAudioProvider {
 public:
  A2dpOffloadAudioProvider();

  bool isValid(const SessionType& sessionType) override;

  Return<void> startSession(const sp<IBluetoothAudioPort>& hostIf,
                            const AudioConfiguration& audioConfig,
                            startSession_cb _hidl_cb) override;

 private:
  Return<void> onSessionReady(startSession_cb _hidl_cb) override;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
