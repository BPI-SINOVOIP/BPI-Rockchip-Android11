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

#define LOG_TAG "audio_proxy_client"

#include <utils/Log.h>

#include "AudioProxyManager.h"
#include "public/audio_proxy.h"

namespace {
class AudioProxyImpl {
 public:
  static AudioProxyImpl* getInstance();

  bool registerDevice(audio_proxy_device_t* device);

 private:
  AudioProxyImpl();
  ~AudioProxyImpl() = default;

  std::unique_ptr<audio_proxy::AudioProxyManager> mManager;
};

AudioProxyImpl::AudioProxyImpl() {
  mManager = audio_proxy::V5_0::createAudioProxyManager();
  ALOGE_IF(!mManager, "Failed to create audio proxy manager");
}

bool AudioProxyImpl::registerDevice(audio_proxy_device_t* device) {
  return mManager && mManager->registerDevice(device);
}

// static
AudioProxyImpl* AudioProxyImpl::getInstance() {
  static AudioProxyImpl instance;
  return &instance;
}

}  // namespace

extern "C" int audio_proxy_register_device(audio_proxy_device_t* device) {
  return AudioProxyImpl::getInstance()->registerDevice(device) ? 0 : -1;
}
