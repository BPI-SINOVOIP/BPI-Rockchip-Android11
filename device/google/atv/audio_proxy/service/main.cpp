// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define LOG_TAG "audio_proxy_service"

#include <hidl/HidlTransportSupport.h>
#include <utils/Log.h>

#include "AudioProxyDevicesManagerImpl.h"
#include "DevicesFactoryImpl.h"

using ::android::sp;
using ::android::status_t;
using ::audio_proxy::service::DevicesFactoryImpl;

namespace {
status_t registerAudioProxyDevicesManager() {
  sp<audio_proxy::service::AudioProxyDevicesManagerImpl> manager =
      new audio_proxy::service::AudioProxyDevicesManagerImpl();
  return manager->registerAsService();
}
}  // namespace

int main(int argc, char** argv) {
  android::hardware::configureRpcThreadpool(1, true /* callerWillJoin */);

  status_t status = registerAudioProxyDevicesManager();
  if (status != android::OK) {
    ALOGE("fail to register devices factory manager: %x", status);
    return -1;
  }

  ::android::hardware::joinRpcThreadpool();

  // `joinRpcThreadpool` should never return. Return -2 here for unexpected
  // process exit.
  return -2;
}
