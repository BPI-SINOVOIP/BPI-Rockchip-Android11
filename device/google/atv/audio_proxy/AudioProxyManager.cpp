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

#include "AudioProxyManager.h"

#include <mutex>

// clang-format off
#include PATH(device/google/atv/audio_proxy/FILE_VERSION/IAudioProxyDevicesManager.h)
// clang-format on

#include <hidl/HidlTransportSupport.h>
#include <utils/Log.h>

#include "AudioProxyDevice.h"
#include "BusDeviceImpl.h"

#define QUOTE(s) #s
#define TO_STR(s) QUOTE(s)

using ::android::sp;
using ::android::status_t;
using ::android::hardware::hidl_death_recipient;
using ::android::hardware::Return;
using ::device::google::atv::audio_proxy::CPP_VERSION::
    IAudioProxyDevicesManager;

namespace audio_proxy {
namespace CPP_VERSION {
namespace {

bool checkDevice(audio_proxy_device_t* device) {
  return device && device->get_address && device->open_output_stream &&
         device->close_output_stream;
}

class DeathRecipient;

class AudioProxyManagerImpl : public AudioProxyManager {
 public:
  explicit AudioProxyManagerImpl(const sp<IAudioProxyDevicesManager>& manager);
  ~AudioProxyManagerImpl() override = default;

  bool registerDevice(audio_proxy_device_t* device) override;

  void reconnectService();

 private:
  std::mutex mLock;
  sp<IAudioProxyDevicesManager> mService;
  std::unique_ptr<AudioProxyDevice> mDevice;

  sp<DeathRecipient> mDeathRecipient;
};

class DeathRecipient : public hidl_death_recipient {
 public:
  explicit DeathRecipient(AudioProxyManagerImpl& manager) : mManager(manager) {}
  ~DeathRecipient() override = default;

  void serviceDied(
      uint64_t cookie,
      const android::wp<::android::hidl::base::V1_0::IBase>& who) override {
    mManager.reconnectService();
  }

 private:
  AudioProxyManagerImpl& mManager;
};

AudioProxyManagerImpl::AudioProxyManagerImpl(
    const sp<IAudioProxyDevicesManager>& manager)
    : mService(manager), mDeathRecipient(new DeathRecipient(*this)) {
  mService->linkToDeath(mDeathRecipient, 1234);
}

bool AudioProxyManagerImpl::registerDevice(audio_proxy_device_t* device) {
  if (!checkDevice(device)) {
    ALOGE("Invalid device.");
    return false;
  }

  std::lock_guard<std::mutex> guard(mLock);
  if (mDevice) {
    ALOGE("Device already registered!");
    return false;
  }

  mDevice = std::make_unique<AudioProxyDevice>(device);

  const char* address = mDevice->getAddress();
  return mService->registerDevice(address, new BusDeviceImpl(mDevice.get()));
}

void AudioProxyManagerImpl::reconnectService() {
  std::lock_guard<std::mutex> guard(mLock);
  mService = IAudioProxyDevicesManager::getService();
  if (!mService) {
    ALOGE("Failed to reconnect service");
    return;
  }

  if (mDevice) {
    bool success = mService->registerDevice(mDevice->getAddress(),
                                            new BusDeviceImpl(mDevice.get()));
    ALOGE_IF(!success, "fail to register device after reconnect.");
  }
}

}  // namespace

std::unique_ptr<AudioProxyManager> createAudioProxyManager() {
  auto service = IAudioProxyDevicesManager::getService();
  if (!service) {
    return nullptr;
  }

  ALOGI("Connect to audio proxy service %s", TO_STR(FILE_VERSION));
  return std::make_unique<AudioProxyManagerImpl>(service);
}

}  // namespace CPP_VERSION
}  // namespace audio_proxy
