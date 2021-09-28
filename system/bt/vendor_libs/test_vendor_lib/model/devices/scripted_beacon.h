/*
 * Copyright 2020 The Android Open Source Project
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

#include <cstdint>
#include <vector>
#include <fstream>

#include "model/devices/scripted_beacon_ble_payload.pb.h"
#include "beacon.h"

using android::bluetooth::test_vendor_lib::model::devices::ScriptedBeaconBleAdProto::PlaybackEvent;

namespace test_vendor_lib {
// Pretend to be a lot of beacons by advertising from a file.
class ScriptedBeacon : public Beacon {
 public:
  ScriptedBeacon();
  virtual ~ScriptedBeacon() = default;

  static std::shared_ptr<Device> Create() {
    return std::make_shared<ScriptedBeacon>();
  }

  // Return a string representation of the type of device.
  virtual std::string GetTypeString() const override {
    return "scripted_beacon";
  }

  virtual std::string ToString() const override {
    return "scripted_beacon " + config_file_;
  }

  // Set the address and advertising interval from string args.
  void Initialize(const std::vector<std::string>& args) override;

  void TimerTick() override;

  void IncomingPacket(model::packets::LinkLayerPacketView packet_view) override;

 private:
  static bool registered_;
  std::string config_file_{};
  std::string events_file_{};
  std::ofstream events_ostream_;
  struct Advertisement {
    std::vector<uint8_t> ad;
    Address address;
    std::chrono::steady_clock::time_point ad_time;
  };

  void populate_event(PlaybackEvent * event, PlaybackEvent::PlaybackEventType type);

  void get_next_advertisement();

  void set_state(
      android::bluetooth::test_vendor_lib::model::devices::
          ScriptedBeaconBleAdProto::PlaybackEvent::PlaybackEventType type);

  Advertisement next_ad_{};
  int packet_num_{0};
  PlaybackEvent::PlaybackEventType current_state_{PlaybackEvent::UNKNOWN};
  std::chrono::steady_clock::time_point next_check_time_{};
  android::bluetooth::test_vendor_lib::model::devices::ScriptedBeaconBleAdProto::BleAdvertisementList ble_ad_list_;
};
}  // namespace test_vendor_lib
