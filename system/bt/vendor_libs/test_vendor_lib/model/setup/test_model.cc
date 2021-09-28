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

#include "test_model.h"

// TODO: Remove when registration works
#include "model/devices/beacon.h"
#include "model/devices/beacon_swarm.h"
#include "model/devices/car_kit.h"
#include "model/devices/classic.h"
#include "model/devices/keyboard.h"
#include "model/devices/remote_loopback_device.h"
#include "model/devices/scripted_beacon.h"
#include "model/devices/sniffer.h"

#include <memory>

#include <stdlib.h>
#include <iomanip>
#include <iostream>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/values.h"

#include "os/log.h"
#include "osi/include/osi.h"

#include "device_boutique.h"
#include "include/phy.h"
#include "model/devices/hci_socket_device.h"
#include "model/devices/link_layer_socket_device.h"

using std::vector;

namespace test_vendor_lib {

TestModel::TestModel(
    std::function<AsyncTaskId(std::chrono::milliseconds, const TaskCallback&)> event_scheduler,

    std::function<AsyncTaskId(std::chrono::milliseconds, std::chrono::milliseconds, const TaskCallback&)>
        periodic_event_scheduler,

    std::function<void(AsyncTaskId)> cancel, std::function<int(const std::string&, int)> connect_to_remote)
    : schedule_task_(event_scheduler), schedule_periodic_task_(periodic_event_scheduler), cancel_task_(cancel),
      connect_to_remote_(connect_to_remote) {
  // TODO: Remove when registration works!
  example_devices_.push_back(std::make_shared<Beacon>());
  example_devices_.push_back(std::make_shared<BeaconSwarm>());
  example_devices_.push_back(std::make_shared<Keyboard>());
  example_devices_.push_back(std::make_shared<CarKit>());
  example_devices_.push_back(std::make_shared<Classic>());
  example_devices_.push_back(std::make_shared<Sniffer>());
  example_devices_.push_back(std::make_shared<ScriptedBeacon>());
  example_devices_.push_back(std::make_shared<RemoteLoopbackDevice>());
}

void TestModel::SetTimerPeriod(std::chrono::milliseconds new_period) {
  timer_period_ = new_period;

  if (timer_tick_task_ == kInvalidTaskId) return;

  // Restart the timer with the new period
  StopTimer();
  StartTimer();
}

void TestModel::StartTimer() {
  LOG_INFO("StartTimer()");
  timer_tick_task_ =
      schedule_periodic_task_(std::chrono::milliseconds(0), timer_period_, [this]() { TestModel::TimerTick(); });
}

void TestModel::StopTimer() {
  LOG_INFO("StopTimer()");
  cancel_task_(timer_tick_task_);
  timer_tick_task_ = kInvalidTaskId;
}

size_t TestModel::Add(std::shared_ptr<Device> new_dev) {
  devices_counter_++;
  devices_[devices_counter_] = new_dev;
  return devices_counter_;
}

void TestModel::Del(size_t dev_index) {
  auto device = devices_.find(dev_index);
  if (device == devices_.end()) {
    LOG_WARN("Del: can't find device!");
    return;
  }
  devices_.erase(dev_index);
}

size_t TestModel::AddPhy(Phy::Type phy_type) {
  phys_counter_++;
  std::shared_ptr<PhyLayerFactory> new_phy = std::make_shared<PhyLayerFactory>(phy_type, phys_counter_);
  phys_[phys_counter_] = new_phy;
  return phys_counter_;
}

void TestModel::DelPhy(size_t phy_index) {
  auto phy = phys_.find(phy_index);
  if (phy == phys_.end()) {
    LOG_WARN("DelPhy: can't find device!");
    return;
  }
  phys_.erase(phy_index);
}

void TestModel::AddDeviceToPhy(size_t dev_index, size_t phy_index) {
  auto device = devices_.find(dev_index);
  if (device == devices_.end()) {
    LOG_WARN("%s: can't find device!", __func__);
    return;
  }
  auto phy = phys_.find(phy_index);
  if (phy == phys_.end()) {
    LOG_WARN("%s: can't find phy!", __func__);
    return;
  }
  auto dev = device->second;
  dev->RegisterPhyLayer(phy->second->GetPhyLayer(
      [dev](model::packets::LinkLayerPacketView packet) {
        dev->IncomingPacket(packet);
      },
      device->first));
}

void TestModel::DelDeviceFromPhy(size_t dev_index, size_t phy_index) {
  auto device = devices_.find(dev_index);
  if (device == devices_.end()) {
    LOG_WARN("%s: can't find device!", __func__);
    return;
  }
  auto phy = phys_.find(phy_index);
  if (phy == phys_.end()) {
    LOG_WARN("%s: can't find phy!", __func__);
    return;
  }
  device->second->UnregisterPhyLayer(phy->second->GetType(), phy->second->GetFactoryId());
}

void TestModel::AddLinkLayerConnection(int socket_fd, Phy::Type phy_type) {
  std::shared_ptr<Device> dev = LinkLayerSocketDevice::Create(socket_fd, phy_type);
  int index = Add(dev);
  for (auto& phy : phys_) {
    if (phy_type == phy.second->GetType()) {
      AddDeviceToPhy(index, phy.first);
    }
  }
}

void TestModel::IncomingLinkLayerConnection(int socket_fd) {
  // TODO: Handle other phys
  AddLinkLayerConnection(socket_fd, Phy::Type::BR_EDR);
}

void TestModel::AddRemote(const std::string& server, int port, Phy::Type phy_type) {
  int socket_fd = connect_to_remote_(server, port);
  if (socket_fd < 0) {
    return;
  }
  AddLinkLayerConnection(socket_fd, phy_type);
}

void TestModel::IncomingHciConnection(int socket_fd) {
  auto dev = HciSocketDevice::Create(socket_fd);
  size_t index = Add(std::static_pointer_cast<Device>(dev));
  std::string addr = "da:4c:10:de:17:";  // Da HCI dev
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(2) << std::hex << (index % 256);
  addr += stream.str();

  dev->Initialize({"IgnoredTypeName", addr});
  LOG_INFO("initialized %s", addr.c_str());
  for (auto& phy : phys_) {
    AddDeviceToPhy(index, phy.first);
  }
  dev->RegisterTaskScheduler(schedule_task_);
  dev->RegisterTaskCancel(cancel_task_);
  dev->RegisterCloseCallback([this, socket_fd, index] { OnHciConnectionClosed(socket_fd, index); });
}

void TestModel::OnHciConnectionClosed(int socket_fd, size_t index) {
  auto device = devices_.find(index);
  if (device == devices_.end()) {
    LOG_WARN("OnHciConnectionClosed: can't find device!");
    return;
  }
  int close_result = close(socket_fd);
  ASSERT_LOG(close_result == 0, "can't close: %s", strerror(errno));
  device->second->UnregisterPhyLayers();
  devices_.erase(index);
}

void TestModel::SetDeviceAddress(size_t index, Address address) {
  auto device = devices_.find(index);
  if (device == devices_.end()) {
    LOG_WARN("SetDeviceAddress can't find device!");
    return;
  }
  device->second->SetAddress(address);
}

const std::string& TestModel::List() {
  list_string_ = "";
  list_string_ += " Devices: \r\n";
  for (auto& dev : devices_) {
    list_string_ += "  " + std::to_string(dev.first) + ":";
    list_string_ += dev.second->ToString() + " \r\n";
  }
  list_string_ += " Phys: \r\n";
  for (auto& phy : phys_) {
    list_string_ += "  " + std::to_string(phy.first) + ":";
    list_string_ += phy.second->ToString() + " \r\n";
  }
  return list_string_;
}

void TestModel::TimerTick() {
  for (auto dev = devices_.begin(); dev != devices_.end();) {
    auto tmp = dev;
    dev++;
    tmp->second->TimerTick();
  }
}

void TestModel::Reset() {
  StopTimer();
  devices_.clear();
  phys_.clear();
}

}  // namespace test_vendor_lib
