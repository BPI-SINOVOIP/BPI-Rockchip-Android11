//
//  Copyright 2017 Google, Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at:
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//

#include "service/hal/fake_bluetooth_av_interface.h"

namespace bluetooth {
namespace hal {
namespace {

// The global test handler instances. We have to have globals since the HAL
// interface methods all have to be global and their signatures don't allow us
// to pass in user_data.
std::shared_ptr<FakeBluetoothAvInterface::TestA2dpSourceHandler>
    g_a2dp_source_handler{};
std::shared_ptr<FakeBluetoothAvInterface::TestA2dpSinkHandler>
    g_a2dp_sink_handler{};

bt_status_t FakeSourceInit(
    btav_source_callbacks_t* callbacks, int max_connected_audio_devices,
    const std::vector<btav_a2dp_codec_config_t>& codec_priorities,
    const std::vector<btav_a2dp_codec_config_t>& offloading_preference) {
  return BT_STATUS_SUCCESS;
}

bt_status_t FakeSinkInit(btav_sink_callbacks_t* callbacks) {
  return BT_STATUS_SUCCESS;
}

bt_status_t FakeConnect(const RawAddress& bd_addr) {
  if (g_a2dp_source_handler) return g_a2dp_source_handler->Connect(bd_addr);
  if (g_a2dp_sink_handler) return g_a2dp_sink_handler->Connect(bd_addr);
  return BT_STATUS_FAIL;
}

bt_status_t FakeDisconnect(const RawAddress& bd_addr) {
  if (g_a2dp_source_handler) return g_a2dp_source_handler->Disconnect(bd_addr);
  if (g_a2dp_sink_handler) return g_a2dp_sink_handler->Disconnect(bd_addr);
  return BT_STATUS_FAIL;
}

void FakeCleanup(void) {}

void FakeSetAudioFocusState(int focus_state) {
  if (g_a2dp_sink_handler)
    return g_a2dp_sink_handler->SetAudioFocusState(focus_state);
}

void FakeSetAudioTrackGain(float gain) {
  if (g_a2dp_sink_handler) return g_a2dp_sink_handler->SetAudioTrackGain(gain);
}

btav_source_interface_t fake_a2dp_source_interface = {
    .size = sizeof(btav_source_interface_t),
    .init = FakeSourceInit,
    .connect = FakeConnect,
    .disconnect = FakeDisconnect,
    .set_silence_device = nullptr,
    .set_active_device = nullptr,
    .config_codec = nullptr,
    .cleanup = FakeCleanup,
};

btav_sink_interface_t fake_a2dp_sink_interface = {
    .size = sizeof(btav_sink_interface_t),
    .init = FakeSinkInit,
    .connect = FakeConnect,
    .disconnect = FakeDisconnect,
    .cleanup = FakeCleanup,
    .set_audio_focus_state = FakeSetAudioFocusState,
    .set_audio_track_gain = FakeSetAudioTrackGain,
    .set_active_device = nullptr,
};

}  // namespace

FakeBluetoothAvInterface::FakeBluetoothAvInterface(
    std::shared_ptr<TestA2dpSourceHandler> a2dp_source_handler) {
  CHECK(!g_a2dp_source_handler);

  if (a2dp_source_handler) g_a2dp_source_handler = a2dp_source_handler;
}

FakeBluetoothAvInterface::FakeBluetoothAvInterface(
    std::shared_ptr<TestA2dpSinkHandler> a2dp_sink_handler) {
  CHECK(!g_a2dp_sink_handler);

  if (a2dp_sink_handler) g_a2dp_sink_handler = a2dp_sink_handler;
}

FakeBluetoothAvInterface::~FakeBluetoothAvInterface() {
  g_a2dp_source_handler = {};
  g_a2dp_sink_handler = {};
}

void FakeBluetoothAvInterface::NotifyConnectionState(
    const RawAddress& bda, btav_connection_state_t state) {
  for (auto& observer : a2dp_source_observers_) {
    observer.ConnectionStateCallback(this, bda, state);
  }
  for (auto& observer : a2dp_sink_observers_) {
    observer.ConnectionStateCallback(this, bda, state);
  }
}
void FakeBluetoothAvInterface::NotifyAudioState(const RawAddress& bda,
                                                btav_audio_state_t state) {
  for (auto& observer : a2dp_source_observers_) {
    observer.AudioStateCallback(this, bda, state);
  }
  for (auto& observer : a2dp_sink_observers_) {
    observer.AudioStateCallback(this, bda, state);
  }
}
void FakeBluetoothAvInterface::NotifyAudioConfig(
    const RawAddress& bda, const btav_a2dp_codec_config_t& codec_config,
    const std::vector<btav_a2dp_codec_config_t> codecs_local_capabilities,
    const std::vector<btav_a2dp_codec_config_t>
        codecs_selectable_capabilities) {
  for (auto& observer : a2dp_source_observers_) {
    observer.AudioConfigCallback(this, bda, codec_config,
                                 codecs_local_capabilities,
                                 codecs_selectable_capabilities);
  }
}
bool FakeBluetoothAvInterface::QueryMandatoryCodecPreferred(
    const RawAddress& bda) {
  // The mandatory codec is preferred only when all observers disable their
  // optional codecs.
  for (auto& observer : a2dp_source_observers_) {
    if (!observer.MandatoryCodecPreferredCallback(this, bda)) {
      return false;
    }
  }
  return true;
}
void FakeBluetoothAvInterface::NotifyAudioConfig(const RawAddress& bda,
                                                 uint32_t sample_rate,
                                                 uint8_t channel_count) {
  for (auto& observer : a2dp_sink_observers_) {
    observer.AudioConfigCallback(this, bda, sample_rate, channel_count);
  }
}

bool FakeBluetoothAvInterface::A2dpSourceEnable(
    std::vector<btav_a2dp_codec_config_t> codec_priorities) {
  return true;
}

void FakeBluetoothAvInterface::A2dpSourceDisable() {}

bool FakeBluetoothAvInterface::A2dpSinkEnable() { return true; }

void FakeBluetoothAvInterface::A2dpSinkDisable() {}

void FakeBluetoothAvInterface::AddA2dpSourceObserver(
    A2dpSourceObserver* observer) {
  CHECK(observer);
  a2dp_source_observers_.AddObserver(observer);
}

void FakeBluetoothAvInterface::RemoveA2dpSourceObserver(
    A2dpSourceObserver* observer) {
  CHECK(observer);
  a2dp_source_observers_.RemoveObserver(observer);
}

void FakeBluetoothAvInterface::AddA2dpSinkObserver(A2dpSinkObserver* observer) {
  CHECK(observer);
  a2dp_sink_observers_.AddObserver(observer);
}

void FakeBluetoothAvInterface::RemoveA2dpSinkObserver(
    A2dpSinkObserver* observer) {
  CHECK(observer);
  a2dp_sink_observers_.RemoveObserver(observer);
}

const btav_source_interface_t*
FakeBluetoothAvInterface::GetA2dpSourceHALInterface() {
  return &fake_a2dp_source_interface;
}

const btav_sink_interface_t*
FakeBluetoothAvInterface::GetA2dpSinkHALInterface() {
  return &fake_a2dp_sink_interface;
}

}  // namespace hal
}  // namespace bluetooth
