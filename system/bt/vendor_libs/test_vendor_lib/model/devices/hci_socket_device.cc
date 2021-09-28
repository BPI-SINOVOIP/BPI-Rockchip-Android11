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

#include "hci_socket_device.h"

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "model/setup/device_boutique.h"
#include "os/log.h"

using std::vector;

namespace test_vendor_lib {

HciSocketDevice::HciSocketDevice(int file_descriptor) : socket_file_descriptor_(file_descriptor) {
  advertising_interval_ms_ = std::chrono::milliseconds(1000);

  page_scan_delay_ms_ = std::chrono::milliseconds(600);

  properties_.SetPageScanRepetitionMode(0);
  properties_.SetClassOfDevice(0x600420);
  properties_.SetExtendedInquiryData({
      16,  // length
      9,   // Type: Device Name
      'g',
      'D',
      'e',
      'v',
      'i',
      'c',
      'e',
      '-',
      'h',
      'c',
      'i',
      '_',
      'n',
      'e',
      't',
  });
  properties_.SetName({
      'g',
      'D',
      'e',
      'v',
      'i',
      'c',
      'e',
      '-',
      'H',
      'C',
      'I',
      '_',
      'N',
      'e',
      't',
  });

  h4_ = hci::H4Packetizer(
      socket_file_descriptor_,
      [this](const std::vector<uint8_t>& raw_command) {
        std::shared_ptr<std::vector<uint8_t>> packet_copy = std::make_shared<std::vector<uint8_t>>(raw_command);
        HandleCommand(packet_copy);
      },
      [](const std::vector<uint8_t>&) { LOG_ALWAYS_FATAL("Unexpected Event in HciSocketDevice!"); },
      [this](const std::vector<uint8_t>& raw_acl) {
        std::shared_ptr<std::vector<uint8_t>> packet_copy = std::make_shared<std::vector<uint8_t>>(raw_acl);
        HandleAcl(packet_copy);
      },
      [this](const std::vector<uint8_t>& raw_sco) {
        std::shared_ptr<std::vector<uint8_t>> packet_copy = std::make_shared<std::vector<uint8_t>>(raw_sco);
        HandleSco(packet_copy);
      },
      [this]() {
        LOG_INFO("HCI socket device disconnected");
        close_callback_();
      });

  RegisterEventChannel([this](std::shared_ptr<std::vector<uint8_t>> packet) {
    SendHci(hci::PacketType::EVENT, packet);
  });
  RegisterAclChannel([this](std::shared_ptr<std::vector<uint8_t>> packet) { SendHci(hci::PacketType::ACL, packet); });
  RegisterScoChannel([this](std::shared_ptr<std::vector<uint8_t>> packet) { SendHci(hci::PacketType::SCO, packet); });
}

void HciSocketDevice::TimerTick() {
  h4_.OnDataReady(socket_file_descriptor_);
  DualModeController::TimerTick();
}

void HciSocketDevice::SendHci(hci::PacketType packet_type, const std::shared_ptr<std::vector<uint8_t>> packet) {
  if (socket_file_descriptor_ == -1) {
    LOG_INFO("socket_file_descriptor == -1");
    return;
  }
  uint8_t type = static_cast<uint8_t>(packet_type);
  int bytes_written;
  bytes_written = write(socket_file_descriptor_, &type, sizeof(type));
  if (bytes_written != sizeof(type)) {
    LOG_WARN("bytes_written %d != sizeof(type)", bytes_written);
  }
  bytes_written = write(socket_file_descriptor_, packet->data(), packet->size());
  if (static_cast<size_t>(bytes_written) != packet->size()) {
    LOG_WARN("bytes_written %d != packet->size", bytes_written);
  }
}

void HciSocketDevice::RegisterCloseCallback(std::function<void()> close_callback) {
  close_callback_ = close_callback;
}

}  // namespace test_vendor_lib
