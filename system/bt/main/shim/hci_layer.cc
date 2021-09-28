/*
 * Copyright 2019 The Android Open Source Project
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

#define LOG_TAG "bt_shim_hci"

#include <base/bind.h>
#include <frameworks/base/core/proto/android/bluetooth/hci/enums.pb.h>
#include <algorithm>
#include <cstdint>

#include "btcore/include/module.h"
#include "hci/hci_layer.h"
#include "main/shim/hci_layer.h"
#include "main/shim/shim.h"
#include "osi/include/allocator.h"
#include "osi/include/future.h"
#include "packet/raw_builder.h"
#include "stack/include/bt_types.h"

/**
 * Callback data wrapped as opaque token bundled with the command
 * transmit request to the Gd layer.
 *
 * Upon completion a token for a corresponding command transmit.
 * request is returned from the Gd layer.
 */
using CommandCallbackData = struct {
  void* context;
};

constexpr size_t kBtHdrSize = sizeof(BT_HDR);
constexpr size_t kCommandLengthSize = sizeof(uint8_t);
constexpr size_t kCommandOpcodeSize = sizeof(uint16_t);

static hci_t interface;
static base::Callback<void(const base::Location&, BT_HDR*)> send_data_upwards;

namespace {
bool IsCommandStatusOpcode(bluetooth::hci::OpCode op_code) {
  switch (op_code) {
    case bluetooth::hci::OpCode::INQUIRY:
    case bluetooth::hci::OpCode::CREATE_CONNECTION:
    case bluetooth::hci::OpCode::DISCONNECT:
    case bluetooth::hci::OpCode::ACCEPT_CONNECTION_REQUEST:
    case bluetooth::hci::OpCode::REJECT_CONNECTION_REQUEST:
    case bluetooth::hci::OpCode::CHANGE_CONNECTION_PACKET_TYPE:
    case bluetooth::hci::OpCode::AUTHENTICATION_REQUESTED:
    case bluetooth::hci::OpCode::SET_CONNECTION_ENCRYPTION:
    case bluetooth::hci::OpCode::CHANGE_CONNECTION_LINK_KEY:
    case bluetooth::hci::OpCode::MASTER_LINK_KEY:
    case bluetooth::hci::OpCode::REMOTE_NAME_REQUEST:
    case bluetooth::hci::OpCode::READ_REMOTE_SUPPORTED_FEATURES:
    case bluetooth::hci::OpCode::READ_REMOTE_EXTENDED_FEATURES:
    case bluetooth::hci::OpCode::READ_REMOTE_VERSION_INFORMATION:
    case bluetooth::hci::OpCode::READ_CLOCK_OFFSET:
    case bluetooth::hci::OpCode::SETUP_SYNCHRONOUS_CONNECTION:
    case bluetooth::hci::OpCode::ACCEPT_SYNCHRONOUS_CONNECTION:
    case bluetooth::hci::OpCode::REJECT_SYNCHRONOUS_CONNECTION:
    case bluetooth::hci::OpCode::ENHANCED_SETUP_SYNCHRONOUS_CONNECTION:
    case bluetooth::hci::OpCode::ENHANCED_ACCEPT_SYNCHRONOUS_CONNECTION:
    case bluetooth::hci::OpCode::HOLD_MODE:
    case bluetooth::hci::OpCode::SNIFF_MODE:
    case bluetooth::hci::OpCode::EXIT_SNIFF_MODE:
    case bluetooth::hci::OpCode::QOS_SETUP:
    case bluetooth::hci::OpCode::SWITCH_ROLE:
    case bluetooth::hci::OpCode::FLOW_SPECIFICATION:
    case bluetooth::hci::OpCode::REFRESH_ENCRYPTION_KEY:
    case bluetooth::hci::OpCode::LE_CREATE_CONNECTION:
    case bluetooth::hci::OpCode::LE_CONNECTION_UPDATE:
    case bluetooth::hci::OpCode::LE_READ_REMOTE_FEATURES:
    case bluetooth::hci::OpCode::LE_READ_LOCAL_P_256_PUBLIC_KEY_COMMAND:
    case bluetooth::hci::OpCode::LE_GENERATE_DHKEY_COMMAND:
    case bluetooth::hci::OpCode::LE_SET_PHY:
    case bluetooth::hci::OpCode::LE_EXTENDED_CREATE_CONNECTION:
    case bluetooth::hci::OpCode::LE_PERIODIC_ADVERTISING_CREATE_SYNC:
      return true;
    default:
      return false;
  }
}

std::unique_ptr<bluetooth::packet::RawBuilder> MakeUniquePacket(
    const uint8_t* data, size_t len) {
  bluetooth::packet::RawBuilder builder;
  std::vector<uint8_t> bytes(data, data + len);

  auto payload = std::make_unique<bluetooth::packet::RawBuilder>();
  payload->AddOctets(bytes);

  return payload;
}
}  // namespace

static future_t* hci_module_shut_down(void);
static future_t* hci_module_start_up(void);

EXPORT_SYMBOL extern const module_t gd_hci_module = {
    .name = GD_HCI_MODULE,
    .init = nullptr,
    .start_up = hci_module_start_up,
    .shut_down = hci_module_shut_down,
    .clean_up = nullptr,
    .dependencies = {GD_SHIM_MODULE, nullptr}};

static future_t* hci_module_start_up(void) {
  return nullptr;
}

static future_t* hci_module_shut_down(void) {
  return nullptr;
}

static void set_data_cb(
    base::Callback<void(const base::Location&, BT_HDR*)> send_data_cb) {
  send_data_upwards = std::move(send_data_cb);
}

void OnTransmitPacketCommandComplete(command_complete_cb complete_callback,
                                     void* context,
                                     bluetooth::hci::CommandCompleteView view) {
  std::vector<const uint8_t> data(view.begin(), view.end());

  BT_HDR* response = static_cast<BT_HDR*>(osi_calloc(data.size() + kBtHdrSize));
  std::copy(data.begin(), data.end(), response->data);
  response->len = data.size();

  complete_callback(response, context);
}

void OnTransmitPacketStatus(command_status_cb status_callback, void* context,
                            bluetooth::hci::CommandStatusView view) {
  std::vector<const uint8_t> data(view.begin(), view.end());

  BT_HDR* response = static_cast<BT_HDR*>(osi_calloc(data.size() + kBtHdrSize));
  std::copy(data.begin(), data.end(), response->data);
  response->len = data.size();

  uint8_t status = static_cast<uint8_t>(view.GetStatus());
  status_callback(status, response, context);
}

using bluetooth::common::BindOnce;
using bluetooth::common::Unretained;

static void transmit_command(BT_HDR* command,
                             command_complete_cb complete_callback,
                             command_status_cb status_callback, void* context) {
  CHECK(command != nullptr);
  uint8_t* data = command->data + command->offset;
  size_t len = command->len;
  CHECK(len >= (kCommandOpcodeSize + kCommandLengthSize));

  // little endian command opcode
  uint16_t command_op_code = (data[1] << 8 | data[0]);
  // Gd stack API requires opcode specification and calculates length, so
  // no need to provide opcode or length here.
  data += (kCommandOpcodeSize + kCommandLengthSize);
  len -= (kCommandOpcodeSize + kCommandLengthSize);

  const bluetooth::hci::OpCode op_code =
      static_cast<const bluetooth::hci::OpCode>(command_op_code);

  auto payload = MakeUniquePacket(data, len);
  auto packet =
      bluetooth::hci::CommandPacketBuilder::Create(op_code, std::move(payload));

  if (IsCommandStatusOpcode(op_code)) {
    bluetooth::shim::GetHciLayer()->EnqueueCommand(
        std::move(packet),
        BindOnce(OnTransmitPacketStatus, status_callback, context),
        bluetooth::shim::GetGdShimHandler());
  } else {
    bluetooth::shim::GetHciLayer()->EnqueueCommand(
        std::move(packet),
        BindOnce(OnTransmitPacketCommandComplete, complete_callback, context),
        bluetooth::shim::GetGdShimHandler());
  }
}

const hci_t* bluetooth::shim::hci_layer_get_interface() {
  static bool loaded = false;
  if (!loaded) {
    loaded = true;
    interface.set_data_cb = set_data_cb;
    interface.transmit_command = transmit_command;
    interface.transmit_command_futured = nullptr;
    interface.transmit_downward = nullptr;
  }
  return &interface;
}
