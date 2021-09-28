/*
 * Copyright 2015 The Android Open Source Project
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

#include "dual_mode_controller.h"

#include <memory>

#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>

#include "os/log.h"
#include "packet/raw_builder.h"

namespace gd_hci = ::bluetooth::hci;
using gd_hci::ErrorCode;
using gd_hci::LoopbackMode;
using gd_hci::OpCode;
using std::vector;

namespace test_vendor_lib {
constexpr char DualModeController::kControllerPropertiesFile[];
constexpr uint16_t DualModeController::kSecurityManagerNumKeys;
constexpr uint16_t kNumCommandPackets = 0x01;

// Device methods.
void DualModeController::Initialize(const std::vector<std::string>& args) {
  if (args.size() < 2) return;

  Address addr;
  if (Address::FromString(args[1], addr)) {
    properties_.SetAddress(addr);
  } else {
    LOG_ALWAYS_FATAL("Invalid address: %s", args[1].c_str());
  }
};

std::string DualModeController::GetTypeString() const {
  return "Simulated Bluetooth Controller";
}

void DualModeController::IncomingPacket(
    model::packets::LinkLayerPacketView incoming) {
  link_layer_controller_.IncomingPacket(incoming);
}

void DualModeController::TimerTick() {
  link_layer_controller_.TimerTick();
}

void DualModeController::SendCommandCompleteUnknownOpCodeEvent(uint16_t command_opcode) const {
  std::unique_ptr<bluetooth::packet::RawBuilder> raw_builder_ptr =
      std::make_unique<bluetooth::packet::RawBuilder>();
  raw_builder_ptr->AddOctets1(kNumCommandPackets);
  raw_builder_ptr->AddOctets2(command_opcode);
  raw_builder_ptr->AddOctets1(
      static_cast<uint8_t>(ErrorCode::UNKNOWN_HCI_COMMAND));

  auto packet = gd_hci::EventPacketBuilder::Create(
      gd_hci::EventCode::COMMAND_COMPLETE, std::move(raw_builder_ptr));
  send_event_(std::move(packet));
}

DualModeController::DualModeController(const std::string& properties_filename, uint16_t num_keys)
    : Device(properties_filename), security_manager_(num_keys) {
  loopback_mode_ = LoopbackMode::NO_LOOPBACK;

  Address public_address;
  ASSERT(Address::FromString("3C:5A:B4:04:05:06", public_address));
  properties_.SetAddress(public_address);

  link_layer_controller_.RegisterRemoteChannel(
      [this](std::shared_ptr<model::packets::LinkLayerPacketBuilder> packet,
             Phy::Type phy_type) {
        DualModeController::SendLinkLayerPacket(packet, phy_type);
      });

#define SET_HANDLER(opcode, method)                                \
  active_hci_commands_[opcode] = [this](CommandPacketView param) { \
    method(param);                                                 \
  };
  SET_HANDLER(OpCode::RESET, Reset);
  SET_HANDLER(OpCode::READ_BUFFER_SIZE, ReadBufferSize);
  SET_HANDLER(OpCode::HOST_BUFFER_SIZE, HostBufferSize);
  SET_HANDLER(OpCode::SNIFF_SUBRATING, SniffSubrating);
  SET_HANDLER(OpCode::READ_ENCRYPTION_KEY_SIZE, ReadEncryptionKeySize);
  SET_HANDLER(OpCode::READ_LOCAL_VERSION_INFORMATION,
              ReadLocalVersionInformation);
  SET_HANDLER(OpCode::READ_BD_ADDR, ReadBdAddr);
  SET_HANDLER(OpCode::READ_LOCAL_SUPPORTED_COMMANDS,
              ReadLocalSupportedCommands);
  SET_HANDLER(OpCode::READ_LOCAL_SUPPORTED_FEATURES,
              ReadLocalSupportedFeatures);
  SET_HANDLER(OpCode::READ_LOCAL_SUPPORTED_CODECS, ReadLocalSupportedCodecs);
  SET_HANDLER(OpCode::READ_LOCAL_EXTENDED_FEATURES, ReadLocalExtendedFeatures);
  SET_HANDLER(OpCode::READ_REMOTE_EXTENDED_FEATURES,
              ReadRemoteExtendedFeatures);
  SET_HANDLER(OpCode::SWITCH_ROLE, SwitchRole);
  SET_HANDLER(OpCode::READ_REMOTE_SUPPORTED_FEATURES,
              ReadRemoteSupportedFeatures);
  SET_HANDLER(OpCode::READ_CLOCK_OFFSET, ReadClockOffset);
  SET_HANDLER(OpCode::IO_CAPABILITY_REQUEST_REPLY, IoCapabilityRequestReply);
  SET_HANDLER(OpCode::USER_CONFIRMATION_REQUEST_REPLY,
              UserConfirmationRequestReply);
  SET_HANDLER(OpCode::USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY,
              UserConfirmationRequestNegativeReply);
  SET_HANDLER(OpCode::IO_CAPABILITY_REQUEST_NEGATIVE_REPLY,
              IoCapabilityRequestNegativeReply);
  SET_HANDLER(OpCode::READ_INQUIRY_RESPONSE_TRANSMIT_POWER_LEVEL,
              ReadInquiryResponseTransmitPowerLevel);
  SET_HANDLER(OpCode::WRITE_SIMPLE_PAIRING_MODE, WriteSimplePairingMode);
  SET_HANDLER(OpCode::WRITE_LE_HOST_SUPPORT, WriteLeHostSupport);
  SET_HANDLER(OpCode::WRITE_SECURE_CONNECTIONS_HOST_SUPPORT,
              WriteSecureConnectionsHostSupport);
  SET_HANDLER(OpCode::SET_EVENT_MASK, SetEventMask);
  SET_HANDLER(OpCode::READ_INQUIRY_MODE, ReadInquiryMode);
  SET_HANDLER(OpCode::WRITE_INQUIRY_MODE, WriteInquiryMode);
  SET_HANDLER(OpCode::READ_PAGE_SCAN_TYPE, ReadPageScanType);
  SET_HANDLER(OpCode::WRITE_PAGE_SCAN_TYPE, WritePageScanType);
  SET_HANDLER(OpCode::WRITE_INQUIRY_SCAN_TYPE, WriteInquiryScanType);
  SET_HANDLER(OpCode::READ_INQUIRY_SCAN_TYPE, ReadInquiryScanType);
  SET_HANDLER(OpCode::AUTHENTICATION_REQUESTED, AuthenticationRequested);
  SET_HANDLER(OpCode::SET_CONNECTION_ENCRYPTION, SetConnectionEncryption);
  SET_HANDLER(OpCode::CHANGE_CONNECTION_LINK_KEY, ChangeConnectionLinkKey);
  SET_HANDLER(OpCode::MASTER_LINK_KEY, MasterLinkKey);
  SET_HANDLER(OpCode::WRITE_AUTHENTICATION_ENABLE, WriteAuthenticationEnable);
  SET_HANDLER(OpCode::READ_AUTHENTICATION_ENABLE, ReadAuthenticationEnable);
  SET_HANDLER(OpCode::WRITE_CLASS_OF_DEVICE, WriteClassOfDevice);
  SET_HANDLER(OpCode::READ_PAGE_TIMEOUT, ReadPageTimeout);
  SET_HANDLER(OpCode::WRITE_PAGE_TIMEOUT, WritePageTimeout);
  SET_HANDLER(OpCode::WRITE_LINK_SUPERVISION_TIMEOUT,
              WriteLinkSupervisionTimeout);
  SET_HANDLER(OpCode::HOLD_MODE, HoldMode);
  SET_HANDLER(OpCode::SNIFF_MODE, SniffMode);
  SET_HANDLER(OpCode::EXIT_SNIFF_MODE, ExitSniffMode);
  SET_HANDLER(OpCode::QOS_SETUP, QosSetup);
  SET_HANDLER(OpCode::WRITE_DEFAULT_LINK_POLICY_SETTINGS,
              WriteDefaultLinkPolicySettings);
  SET_HANDLER(OpCode::FLOW_SPECIFICATION, FlowSpecification);
  SET_HANDLER(OpCode::WRITE_LINK_POLICY_SETTINGS, WriteLinkPolicySettings);
  SET_HANDLER(OpCode::CHANGE_CONNECTION_PACKET_TYPE,
              ChangeConnectionPacketType);
  SET_HANDLER(OpCode::WRITE_LOCAL_NAME, WriteLocalName);
  SET_HANDLER(OpCode::READ_LOCAL_NAME, ReadLocalName);
  SET_HANDLER(OpCode::WRITE_EXTENDED_INQUIRY_RESPONSE,
              WriteExtendedInquiryResponse);
  SET_HANDLER(OpCode::REFRESH_ENCRYPTION_KEY, RefreshEncryptionKey);
  SET_HANDLER(OpCode::WRITE_VOICE_SETTING, WriteVoiceSetting);
  SET_HANDLER(OpCode::READ_NUMBER_OF_SUPPORTED_IAC, ReadNumberOfSupportedIac);
  SET_HANDLER(OpCode::READ_CURRENT_IAC_LAP, ReadCurrentIacLap);
  SET_HANDLER(OpCode::WRITE_CURRENT_IAC_LAP, WriteCurrentIacLap);
  SET_HANDLER(OpCode::READ_PAGE_SCAN_ACTIVITY, ReadPageScanActivity);
  SET_HANDLER(OpCode::WRITE_PAGE_SCAN_ACTIVITY, WritePageScanActivity);
  SET_HANDLER(OpCode::READ_INQUIRY_SCAN_ACTIVITY, ReadInquiryScanActivity);
  SET_HANDLER(OpCode::WRITE_INQUIRY_SCAN_ACTIVITY, WriteInquiryScanActivity);
  SET_HANDLER(OpCode::READ_SCAN_ENABLE, ReadScanEnable);
  SET_HANDLER(OpCode::WRITE_SCAN_ENABLE, WriteScanEnable);
  SET_HANDLER(OpCode::SET_EVENT_FILTER, SetEventFilter);
  SET_HANDLER(OpCode::INQUIRY, Inquiry);
  SET_HANDLER(OpCode::INQUIRY_CANCEL, InquiryCancel);
  SET_HANDLER(OpCode::ACCEPT_CONNECTION_REQUEST, AcceptConnectionRequest);
  SET_HANDLER(OpCode::REJECT_CONNECTION_REQUEST, RejectConnectionRequest);
  SET_HANDLER(OpCode::LINK_KEY_REQUEST_REPLY, LinkKeyRequestReply);
  SET_HANDLER(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY,
              LinkKeyRequestNegativeReply);
  SET_HANDLER(OpCode::DELETE_STORED_LINK_KEY, DeleteStoredLinkKey);
  SET_HANDLER(OpCode::REMOTE_NAME_REQUEST, RemoteNameRequest);
  SET_HANDLER(OpCode::LE_SET_EVENT_MASK, LeSetEventMask);
  SET_HANDLER(OpCode::LE_READ_BUFFER_SIZE, LeReadBufferSize);
  SET_HANDLER(OpCode::LE_READ_LOCAL_SUPPORTED_FEATURES,
              LeReadLocalSupportedFeatures);
  SET_HANDLER(OpCode::LE_SET_RANDOM_ADDRESS, LeSetRandomAddress);
  SET_HANDLER(OpCode::LE_SET_ADVERTISING_PARAMETERS,
              LeSetAdvertisingParameters);
  SET_HANDLER(OpCode::LE_SET_ADVERTISING_DATA, LeSetAdvertisingData);
  SET_HANDLER(OpCode::LE_SET_SCAN_RESPONSE_DATA, LeSetScanResponseData);
  SET_HANDLER(OpCode::LE_SET_ADVERTISING_ENABLE, LeSetAdvertisingEnable);
  SET_HANDLER(OpCode::LE_SET_SCAN_PARAMETERS, LeSetScanParameters);
  SET_HANDLER(OpCode::LE_SET_SCAN_ENABLE, LeSetScanEnable);
  SET_HANDLER(OpCode::LE_CREATE_CONNECTION, LeCreateConnection);
  SET_HANDLER(OpCode::CREATE_CONNECTION, CreateConnection);
  SET_HANDLER(OpCode::DISCONNECT, Disconnect);
  SET_HANDLER(OpCode::LE_CREATE_CONNECTION_CANCEL, LeConnectionCancel);
  SET_HANDLER(OpCode::LE_READ_WHITE_LIST_SIZE, LeReadWhiteListSize);
  SET_HANDLER(OpCode::LE_CLEAR_WHITE_LIST, LeClearWhiteList);
  SET_HANDLER(OpCode::LE_ADD_DEVICE_TO_WHITE_LIST, LeAddDeviceToWhiteList);
  SET_HANDLER(OpCode::LE_REMOVE_DEVICE_FROM_WHITE_LIST,
              LeRemoveDeviceFromWhiteList);
  SET_HANDLER(OpCode::LE_RAND, LeRand);
  SET_HANDLER(OpCode::LE_READ_SUPPORTED_STATES, LeReadSupportedStates);
  SET_HANDLER(OpCode::LE_GET_VENDOR_CAPABILITIES, LeVendorCap);
  SET_HANDLER(OpCode::LE_MULTI_ADVT, LeVendorMultiAdv);
  SET_HANDLER(OpCode::LE_ADV_FILTER, LeAdvertisingFilter);
  SET_HANDLER(OpCode::LE_ENERGY_INFO, LeEnergyInfo);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_ADVERTISING_RANDOM_ADDRESS,
              LeSetExtendedAdvertisingRandomAddress);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_ADVERTISING_PARAMETERS,
              LeSetExtendedAdvertisingParameters);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_ADVERTISING_DATA,
              LeSetExtendedAdvertisingData);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_ADVERTISING_SCAN_RESPONSE,
              LeSetExtendedAdvertisingScanResponse);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_ADVERTISING_ENABLE,
              LeSetExtendedAdvertisingEnable);
  SET_HANDLER(OpCode::LE_READ_REMOTE_FEATURES, LeReadRemoteFeatures);
  SET_HANDLER(OpCode::READ_REMOTE_VERSION_INFORMATION,
              ReadRemoteVersionInformation);
  SET_HANDLER(OpCode::LE_CONNECTION_UPDATE, LeConnectionUpdate);
  SET_HANDLER(OpCode::LE_START_ENCRYPTION, LeStartEncryption);
  SET_HANDLER(OpCode::LE_ADD_DEVICE_TO_RESOLVING_LIST,
              LeAddDeviceToResolvingList);
  SET_HANDLER(OpCode::LE_REMOVE_DEVICE_FROM_RESOLVING_LIST,
              LeRemoveDeviceFromResolvingList);
  SET_HANDLER(OpCode::LE_CLEAR_RESOLVING_LIST, LeClearResolvingList);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_SCAN_PARAMETERS,
              LeSetExtendedScanParameters);
  SET_HANDLER(OpCode::LE_SET_EXTENDED_SCAN_ENABLE, LeSetExtendedScanEnable);
  SET_HANDLER(OpCode::LE_EXTENDED_CREATE_CONNECTION,
              LeExtendedCreateConnection);
  SET_HANDLER(OpCode::LE_SET_PRIVACY_MODE, LeSetPrivacyMode);
  // Testing Commands
  SET_HANDLER(OpCode::READ_LOOPBACK_MODE, ReadLoopbackMode);
  SET_HANDLER(OpCode::WRITE_LOOPBACK_MODE, WriteLoopbackMode);
#undef SET_HANDLER
}

void DualModeController::SniffSubrating(CommandPacketView command) {
  auto command_view = gd_hci::SniffSubratingView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  send_event_(gd_hci::SniffSubratingCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      command_view.GetConnectionHandle()));
}

void DualModeController::RegisterTaskScheduler(
    std::function<AsyncTaskId(std::chrono::milliseconds, const TaskCallback&)> oneshot_scheduler) {
  link_layer_controller_.RegisterTaskScheduler(oneshot_scheduler);
}

void DualModeController::RegisterPeriodicTaskScheduler(
    std::function<AsyncTaskId(std::chrono::milliseconds, std::chrono::milliseconds, const TaskCallback&)>
        periodic_scheduler) {
  link_layer_controller_.RegisterPeriodicTaskScheduler(periodic_scheduler);
}

void DualModeController::RegisterTaskCancel(std::function<void(AsyncTaskId)> task_cancel) {
  link_layer_controller_.RegisterTaskCancel(task_cancel);
}

void DualModeController::HandleAcl(std::shared_ptr<std::vector<uint8_t>> packet) {
  bluetooth::hci::PacketView<bluetooth::hci::kLittleEndian> raw_packet(packet);
  auto acl_packet = bluetooth::hci::AclPacketView::Create(raw_packet);
  ASSERT(acl_packet.IsValid());
  if (loopback_mode_ == LoopbackMode::ENABLE_LOCAL) {
    uint16_t handle = acl_packet.GetHandle();

    std::vector<bluetooth::hci::CompletedPackets> completed_packets;
    bluetooth::hci::CompletedPackets cp;
    cp.connection_handle_ = handle;
    cp.host_num_of_completed_packets_ = kNumCommandPackets;
    completed_packets.push_back(cp);
    send_event_(bluetooth::hci::NumberOfCompletedPacketsBuilder::Create(
        completed_packets));
    return;
  }

  link_layer_controller_.SendAclToRemote(acl_packet);
}

void DualModeController::HandleSco(std::shared_ptr<std::vector<uint8_t>> packet) {
  bluetooth::hci::PacketView<bluetooth::hci::kLittleEndian> raw_packet(packet);
  auto sco_packet = bluetooth::hci::ScoPacketView::Create(raw_packet);
  if (loopback_mode_ == LoopbackMode::ENABLE_LOCAL) {
    uint16_t handle = sco_packet.GetHandle();
    send_sco_(packet);
    std::vector<bluetooth::hci::CompletedPackets> completed_packets;
    bluetooth::hci::CompletedPackets cp;
    cp.connection_handle_ = handle;
    cp.host_num_of_completed_packets_ = kNumCommandPackets;
    completed_packets.push_back(cp);
    send_event_(bluetooth::hci::NumberOfCompletedPacketsBuilder::Create(
        completed_packets));
    return;
  }
}

void DualModeController::HandleIso(
    std::shared_ptr<std::vector<uint8_t>> /* packet */) {
  // TODO: implement handling similar to HandleSco
}

void DualModeController::HandleCommand(std::shared_ptr<std::vector<uint8_t>> packet) {
  bluetooth::hci::PacketView<bluetooth::hci::kLittleEndian> raw_packet(packet);
  auto command_packet = bluetooth::hci::CommandPacketView::Create(raw_packet);
  ASSERT(command_packet.IsValid());
  auto op = command_packet.GetOpCode();

  if (loopback_mode_ == LoopbackMode::ENABLE_LOCAL &&
      // Loopback exceptions.
      op != OpCode::RESET &&
      op != OpCode::SET_CONTROLLER_TO_HOST_FLOW_CONTROL &&
      op != OpCode::HOST_BUFFER_SIZE &&
      op != OpCode::HOST_NUM_COMPLETED_PACKETS &&
      op != OpCode::READ_BUFFER_SIZE && op != OpCode::READ_LOOPBACK_MODE &&
      op != OpCode::WRITE_LOOPBACK_MODE) {
    std::unique_ptr<bluetooth::packet::RawBuilder> raw_builder_ptr =
        std::make_unique<bluetooth::packet::RawBuilder>(255);
    raw_builder_ptr->AddOctets(*packet);
    send_event_(bluetooth::hci::LoopbackCommandBuilder::Create(
        std::move(raw_builder_ptr)));
  } else if (active_hci_commands_.count(op) > 0) {
    active_hci_commands_[op](command_packet);
  } else {
    uint16_t opcode = static_cast<uint16_t>(op);
    SendCommandCompleteUnknownOpCodeEvent(opcode);
    LOG_INFO("Unknown command, opcode: 0x%04X, OGF: 0x%04X, OCF: 0x%04X",
             opcode, (opcode & 0xFC00) >> 10, opcode & 0x03FF);
  }
}

void DualModeController::RegisterEventChannel(
    const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& callback) {
  send_event_ =
      [callback](std::shared_ptr<bluetooth::hci::EventPacketBuilder> event) {
        auto bytes = std::make_shared<std::vector<uint8_t>>();
        bluetooth::packet::BitInserter bit_inserter(*bytes);
        bytes->reserve(event->size());
        event->Serialize(bit_inserter);
        callback(std::move(bytes));
      };
  link_layer_controller_.RegisterEventChannel(send_event_);
}

void DualModeController::RegisterAclChannel(
    const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& callback) {
  send_acl_ =
      [callback](std::shared_ptr<bluetooth::hci::AclPacketBuilder> acl_data) {
        auto bytes = std::make_shared<std::vector<uint8_t>>();
        bluetooth::packet::BitInserter bit_inserter(*bytes);
        bytes->reserve(acl_data->size());
        acl_data->Serialize(bit_inserter);
        callback(std::move(bytes));
      };
  link_layer_controller_.RegisterAclChannel(send_acl_);
}

void DualModeController::RegisterScoChannel(
    const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>& callback) {
  link_layer_controller_.RegisterScoChannel(callback);
  send_sco_ = callback;
}

void DualModeController::RegisterIsoChannel(
    const std::function<void(std::shared_ptr<std::vector<uint8_t>>)>&
        callback) {
  link_layer_controller_.RegisterIsoChannel(callback);
  send_iso_ = callback;
}

void DualModeController::Reset(CommandPacketView command) {
  auto command_view = gd_hci::ResetView::Create(command);
  ASSERT(command_view.IsValid());
  link_layer_controller_.Reset();
  if (loopback_mode_ == LoopbackMode::ENABLE_LOCAL) {
    loopback_mode_ = LoopbackMode::NO_LOOPBACK;
  }

  send_event_(bluetooth::hci::ResetCompleteBuilder::Create(kNumCommandPackets,
                                                           ErrorCode::SUCCESS));
}

void DualModeController::ReadBufferSize(CommandPacketView command) {
  auto command_view = gd_hci::ReadBufferSizeView::Create(command);
  ASSERT(command_view.IsValid());

  auto packet = bluetooth::hci::ReadBufferSizeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      properties_.GetAclDataPacketSize(),
      properties_.GetSynchronousDataPacketSize(),
      properties_.GetTotalNumAclDataPackets(),
      properties_.GetTotalNumSynchronousDataPackets());
  send_event_(std::move(packet));
}

void DualModeController::ReadEncryptionKeySize(CommandPacketView command) {
  auto command_view = gd_hci::ReadEncryptionKeySizeView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto packet = bluetooth::hci::ReadEncryptionKeySizeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      command_view.GetConnectionHandle(), properties_.GetEncryptionKeySize());
  send_event_(std::move(packet));
}

void DualModeController::HostBufferSize(CommandPacketView command) {
  auto command_view = gd_hci::HostBufferSizeView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::HostBufferSizeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalVersionInformation(
    CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalVersionInformationView::Create(command);
  ASSERT(command_view.IsValid());

  bluetooth::hci::LocalVersionInformation local_version_information;
  local_version_information.hci_version_ =
      static_cast<bluetooth::hci::HciVersion>(properties_.GetVersion());
  local_version_information.hci_revision_ = properties_.GetRevision();
  local_version_information.lmp_version_ =
      static_cast<bluetooth::hci::LmpVersion>(properties_.GetLmpPalVersion());
  local_version_information.manufacturer_name_ =
      properties_.GetManufacturerName();
  local_version_information.lmp_subversion_ = properties_.GetLmpPalSubversion();
  auto packet =
      bluetooth::hci::ReadLocalVersionInformationCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS, local_version_information);
  send_event_(std::move(packet));
}

void DualModeController::ReadRemoteVersionInformation(
    CommandPacketView command) {
  auto command_view = gd_hci::ReadRemoteVersionInformationView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status = link_layer_controller_.SendCommandToRemoteByHandle(
      OpCode::READ_REMOTE_VERSION_INFORMATION, command.GetPayload(),
      command_view.GetConnectionHandle());

  auto packet =
      bluetooth::hci::ReadRemoteVersionInformationStatusBuilder::Create(
          status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::ReadBdAddr(CommandPacketView command) {
  auto command_view = gd_hci::ReadBdAddrView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::ReadBdAddrCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, properties_.GetAddress());
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalSupportedCommands(CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalSupportedCommandsView::Create(command);
  ASSERT(command_view.IsValid());

  std::array<uint8_t, 64> supported_commands;
  supported_commands.fill(0x00);
  size_t len = properties_.GetSupportedCommands().size();
  if (len > 64) {
    len = 64;
  }
  std::copy_n(properties_.GetSupportedCommands().begin(), len,
              supported_commands.begin());

  auto packet =
      bluetooth::hci::ReadLocalSupportedCommandsCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS, supported_commands);
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalSupportedFeatures(CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalSupportedFeaturesView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet =
      bluetooth::hci::ReadLocalSupportedFeaturesCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS,
          properties_.GetSupportedFeatures());
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalSupportedCodecs(CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalSupportedCodecsView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::ReadLocalSupportedCodecsCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, properties_.GetSupportedCodecs(),
      properties_.GetVendorSpecificCodecs());
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalExtendedFeatures(CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalExtendedFeaturesView::Create(command);
  ASSERT(command_view.IsValid());
  uint8_t page_number = command_view.GetPageNumber();

  auto pakcet =
      bluetooth::hci::ReadLocalExtendedFeaturesCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS, page_number,
          properties_.GetExtendedFeaturesMaximumPageNumber(),
          properties_.GetExtendedFeatures(page_number));
  send_event_(std::move(pakcet));
}

void DualModeController::ReadRemoteExtendedFeatures(CommandPacketView command) {
  auto command_view = gd_hci::ReadRemoteExtendedFeaturesView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status = link_layer_controller_.SendCommandToRemoteByHandle(
      OpCode::READ_REMOTE_EXTENDED_FEATURES, command_view.GetPayload(),
      command_view.GetConnectionHandle());

  auto packet = bluetooth::hci::ReadRemoteExtendedFeaturesStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::SwitchRole(CommandPacketView command) {
  auto command_view = gd_hci::SwitchRoleView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status = link_layer_controller_.SwitchRole(
      command_view.GetBdAddr(), static_cast<uint8_t>(command_view.GetRole()));

  auto packet = bluetooth::hci::SwitchRoleStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::ReadRemoteSupportedFeatures(
    CommandPacketView command) {
  auto command_view = gd_hci::ReadRemoteSupportedFeaturesView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status = link_layer_controller_.SendCommandToRemoteByHandle(
      OpCode::READ_REMOTE_SUPPORTED_FEATURES, command_view.GetPayload(),
      command_view.GetConnectionHandle());

  auto packet =
      bluetooth::hci::ReadRemoteSupportedFeaturesStatusBuilder::Create(
          status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::ReadClockOffset(CommandPacketView command) {
  auto command_view = gd_hci::ReadClockOffsetView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();

  auto status = link_layer_controller_.SendCommandToRemoteByHandle(
      OpCode::READ_CLOCK_OFFSET, command_view.GetPayload(), handle);

  auto packet = bluetooth::hci::ReadClockOffsetStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::IoCapabilityRequestReply(CommandPacketView command) {
  auto command_view = gd_hci::IoCapabilityRequestReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();
  uint8_t io_capability = static_cast<uint8_t>(command_view.GetIoCapability());
  uint8_t oob_data_present_flag =
      static_cast<uint8_t>(command_view.GetOobPresent());
  uint8_t authentication_requirements =
      static_cast<uint8_t>(command_view.GetAuthenticationRequirements());

  auto status = link_layer_controller_.IoCapabilityRequestReply(
      peer, io_capability, oob_data_present_flag, authentication_requirements);
  auto packet = bluetooth::hci::IoCapabilityRequestReplyCompleteBuilder::Create(
      kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::UserConfirmationRequestReply(
    CommandPacketView command) {
  auto command_view = gd_hci::UserConfirmationRequestReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();

  auto status = link_layer_controller_.UserConfirmationRequestReply(peer);
  auto packet =
      bluetooth::hci::UserConfirmationRequestReplyCompleteBuilder::Create(
          kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::UserConfirmationRequestNegativeReply(
    CommandPacketView command) {
  auto command_view = gd_hci::UserConfirmationRequestNegativeReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();

  auto status =
      link_layer_controller_.UserConfirmationRequestNegativeReply(peer);
  auto packet =
      bluetooth::hci::UserConfirmationRequestNegativeReplyCompleteBuilder::
          Create(kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::UserPasskeyRequestReply(CommandPacketView command) {
  auto command_view = gd_hci::UserPasskeyRequestReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();
  uint32_t numeric_value = command_view.GetNumericValue();

  auto status =
      link_layer_controller_.UserPasskeyRequestReply(peer, numeric_value);
  auto packet = bluetooth::hci::UserPasskeyRequestReplyCompleteBuilder::Create(
      kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::UserPasskeyRequestNegativeReply(
    CommandPacketView command) {
  auto command_view = gd_hci::UserPasskeyRequestNegativeReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();

  auto status = link_layer_controller_.UserPasskeyRequestNegativeReply(peer);
  auto packet =
      bluetooth::hci::UserPasskeyRequestNegativeReplyCompleteBuilder::Create(
          kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::RemoteOobDataRequestReply(CommandPacketView command) {
  auto command_view = gd_hci::RemoteOobDataRequestReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();
  std::array<uint8_t, 16> c = command_view.GetC();
  std::array<uint8_t, 16> r = command_view.GetR();

  auto status = link_layer_controller_.RemoteOobDataRequestReply(
      peer, std::vector<uint8_t>(c.begin(), c.end()),
      std::vector<uint8_t>(r.begin(), r.end()));
  auto packet =
      bluetooth::hci::RemoteOobDataRequestReplyCompleteBuilder::Create(
          kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::RemoteOobDataRequestNegativeReply(
    CommandPacketView command) {
  auto command_view = gd_hci::RemoteOobDataRequestNegativeReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();

  auto status = link_layer_controller_.RemoteOobDataRequestNegativeReply(peer);
  auto packet =
      bluetooth::hci::RemoteOobDataRequestNegativeReplyCompleteBuilder::Create(
          kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::IoCapabilityRequestNegativeReply(
    CommandPacketView command) {
  auto command_view = gd_hci::IoCapabilityRequestNegativeReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address peer = command_view.GetBdAddr();
  ErrorCode reason = command_view.GetReason();

  auto status =
      link_layer_controller_.IoCapabilityRequestNegativeReply(peer, reason);
  auto packet =
      bluetooth::hci::IoCapabilityRequestNegativeReplyCompleteBuilder::Create(
          kNumCommandPackets, status, peer);

  send_event_(std::move(packet));
}

void DualModeController::ReadInquiryResponseTransmitPowerLevel(
    CommandPacketView command) {
  auto command_view = gd_hci::ReadInquiryResponseTransmitPowerLevelView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint8_t tx_power = 20;  // maximum
  auto packet =
      bluetooth::hci::ReadInquiryResponseTransmitPowerLevelCompleteBuilder::
          Create(kNumCommandPackets, ErrorCode::SUCCESS, tx_power);
  send_event_(std::move(packet));
}

void DualModeController::WriteSimplePairingMode(CommandPacketView command) {
  auto command_view = gd_hci::WriteSimplePairingModeView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  link_layer_controller_.WriteSimplePairingMode(
      command_view.GetSimplePairingMode() == gd_hci::Enable::ENABLED);
  auto packet = bluetooth::hci::WriteSimplePairingModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ChangeConnectionPacketType(CommandPacketView command) {
  auto command_view = gd_hci::ChangeConnectionPacketTypeView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();
  uint16_t packet_type = static_cast<uint16_t>(command_view.GetPacketType());

  auto status =
      link_layer_controller_.ChangeConnectionPacketType(handle, packet_type);

  auto packet = bluetooth::hci::ChangeConnectionPacketTypeStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::WriteLeHostSupport(CommandPacketView command) {
  auto command_view = gd_hci::WriteLeHostSupportView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WriteLeHostSupportCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::WriteSecureConnectionsHostSupport(
    CommandPacketView command) {
  auto command_view = gd_hci::WriteSecureConnectionsHostSupportView::Create(
      gd_hci::SecurityCommandView::Create(command));
  properties_.SetExtendedFeatures(properties_.GetExtendedFeatures(1) | 0x8, 1);
  auto packet =
      bluetooth::hci::WriteSecureConnectionsHostSupportCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::SetEventMask(CommandPacketView command) {
  auto command_view = gd_hci::SetEventMaskView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::SetEventMaskCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadInquiryMode(CommandPacketView command) {
  auto command_view = gd_hci::ReadInquiryModeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  gd_hci::InquiryMode inquiry_mode = gd_hci::InquiryMode::STANDARD;
  auto packet = bluetooth::hci::ReadInquiryModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, inquiry_mode);
  send_event_(std::move(packet));
}

void DualModeController::WriteInquiryMode(CommandPacketView command) {
  auto command_view = gd_hci::WriteInquiryModeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetInquiryMode(
      static_cast<uint8_t>(command_view.GetInquiryMode()));
  auto packet = bluetooth::hci::WriteInquiryModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadPageScanType(CommandPacketView command) {
  auto command_view = gd_hci::ReadPageScanTypeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  gd_hci::PageScanType page_scan_type = gd_hci::PageScanType::STANDARD;
  auto packet = bluetooth::hci::ReadPageScanTypeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, page_scan_type);
  send_event_(std::move(packet));
}

void DualModeController::WritePageScanType(CommandPacketView command) {
  auto command_view = gd_hci::WritePageScanTypeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WritePageScanTypeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadInquiryScanType(CommandPacketView command) {
  auto command_view = gd_hci::ReadInquiryScanTypeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  gd_hci::InquiryScanType inquiry_scan_type = gd_hci::InquiryScanType::STANDARD;
  auto packet = bluetooth::hci::ReadInquiryScanTypeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, inquiry_scan_type);
  send_event_(std::move(packet));
}

void DualModeController::WriteInquiryScanType(CommandPacketView command) {
  auto command_view = gd_hci::WriteInquiryScanTypeView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WriteInquiryScanTypeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::AuthenticationRequested(CommandPacketView command) {
  auto command_view = gd_hci::AuthenticationRequestedView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  auto status = link_layer_controller_.AuthenticationRequested(handle);

  auto packet = bluetooth::hci::AuthenticationRequestedStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::SetConnectionEncryption(CommandPacketView command) {
  auto command_view = gd_hci::SetConnectionEncryptionView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  uint8_t encryption_enable =
      static_cast<uint8_t>(command_view.GetEncryptionEnable());
  auto status =
      link_layer_controller_.SetConnectionEncryption(handle, encryption_enable);

  auto packet = bluetooth::hci::SetConnectionEncryptionStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::ChangeConnectionLinkKey(CommandPacketView command) {
  auto command_view = gd_hci::ChangeConnectionLinkKeyView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();

  auto status = link_layer_controller_.ChangeConnectionLinkKey(handle);

  auto packet = bluetooth::hci::ChangeConnectionLinkKeyStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::MasterLinkKey(CommandPacketView command) {
  auto command_view = gd_hci::MasterLinkKeyView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint8_t key_flag = static_cast<uint8_t>(command_view.GetKeyFlag());

  auto status = link_layer_controller_.MasterLinkKey(key_flag);

  auto packet = bluetooth::hci::MasterLinkKeyStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::WriteAuthenticationEnable(CommandPacketView command) {
  auto command_view = gd_hci::WriteAuthenticationEnableView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  properties_.SetAuthenticationEnable(
      static_cast<uint8_t>(command_view.GetAuthenticationEnable()));
  auto packet =
      bluetooth::hci::WriteAuthenticationEnableCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadAuthenticationEnable(CommandPacketView command) {
  auto command_view = gd_hci::ReadAuthenticationEnableView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::ReadAuthenticationEnableCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      static_cast<bluetooth::hci::AuthenticationEnable>(
          properties_.GetAuthenticationEnable()));
  send_event_(std::move(packet));
}

void DualModeController::WriteClassOfDevice(CommandPacketView command) {
  auto command_view = gd_hci::WriteClassOfDeviceView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  ClassOfDevice class_of_device = command_view.GetClassOfDevice();
  properties_.SetClassOfDevice(class_of_device.cod[0], class_of_device.cod[1],
                               class_of_device.cod[2]);
  auto packet = bluetooth::hci::WriteClassOfDeviceCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadPageTimeout(CommandPacketView command) {
  auto command_view = gd_hci::ReadPageTimeoutView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t page_timeout = 0x2000;
  auto packet = bluetooth::hci::ReadPageTimeoutCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, page_timeout);
  send_event_(std::move(packet));
}

void DualModeController::WritePageTimeout(CommandPacketView command) {
  auto command_view = gd_hci::WritePageTimeoutView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WritePageTimeoutCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::HoldMode(CommandPacketView command) {
  auto command_view = gd_hci::HoldModeView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  uint16_t hold_mode_max_interval = command_view.GetHoldModeMaxInterval();
  uint16_t hold_mode_min_interval = command_view.GetHoldModeMinInterval();

  auto status = link_layer_controller_.HoldMode(handle, hold_mode_max_interval,
                                                hold_mode_min_interval);

  auto packet =
      bluetooth::hci::HoldModeStatusBuilder::Create(status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::SniffMode(CommandPacketView command) {
  auto command_view = gd_hci::SniffModeView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  uint16_t sniff_max_interval = command_view.GetSniffMaxInterval();
  uint16_t sniff_min_interval = command_view.GetSniffMinInterval();
  uint16_t sniff_attempt = command_view.GetSniffAttempt();
  uint16_t sniff_timeout = command_view.GetSniffTimeout();

  auto status = link_layer_controller_.SniffMode(handle, sniff_max_interval,
                                                 sniff_min_interval,
                                                 sniff_attempt, sniff_timeout);

  auto packet = bluetooth::hci::SniffModeStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::ExitSniffMode(CommandPacketView command) {
  auto command_view = gd_hci::ExitSniffModeView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status =
      link_layer_controller_.ExitSniffMode(command_view.GetConnectionHandle());

  auto packet = bluetooth::hci::ExitSniffModeStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::QosSetup(CommandPacketView command) {
  auto command_view = gd_hci::QosSetupView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  uint8_t service_type = static_cast<uint8_t>(command_view.GetServiceType());
  uint32_t token_rate = command_view.GetTokenRate();
  uint32_t peak_bandwidth = command_view.GetPeakBandwidth();
  uint32_t latency = command_view.GetLatency();
  uint32_t delay_variation = command_view.GetDelayVariation();

  auto status =
      link_layer_controller_.QosSetup(handle, service_type, token_rate,
                                      peak_bandwidth, latency, delay_variation);

  auto packet =
      bluetooth::hci::QosSetupStatusBuilder::Create(status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::WriteDefaultLinkPolicySettings(
    CommandPacketView command) {
  auto command_view = gd_hci::WriteDefaultLinkPolicySettingsView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet =
      bluetooth::hci::WriteDefaultLinkPolicySettingsCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::FlowSpecification(CommandPacketView command) {
  auto command_view = gd_hci::FlowSpecificationView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  uint8_t flow_direction =
      static_cast<uint8_t>(command_view.GetFlowDirection());
  uint8_t service_type = static_cast<uint8_t>(command_view.GetServiceType());
  uint32_t token_rate = command_view.GetTokenRate();
  uint32_t token_bucket_size = command_view.GetTokenBucketSize();
  uint32_t peak_bandwidth = command_view.GetPeakBandwidth();
  uint32_t access_latency = command_view.GetAccessLatency();

  auto status = link_layer_controller_.FlowSpecification(
      handle, flow_direction, service_type, token_rate, token_bucket_size,
      peak_bandwidth, access_latency);

  auto packet = bluetooth::hci::FlowSpecificationStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::WriteLinkPolicySettings(CommandPacketView command) {
  auto command_view = gd_hci::WriteLinkPolicySettingsView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();
  uint16_t settings = command_view.GetLinkPolicySettings();

  auto status =
      link_layer_controller_.WriteLinkPolicySettings(handle, settings);

  auto packet = bluetooth::hci::WriteLinkPolicySettingsCompleteBuilder::Create(
      kNumCommandPackets, status, handle);
  send_event_(std::move(packet));
}

void DualModeController::WriteLinkSupervisionTimeout(
    CommandPacketView command) {
  auto command_view = gd_hci::WriteLinkSupervisionTimeoutView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetHandle();
  uint16_t timeout = command_view.GetLinkSupervisionTimeout();

  auto status =
      link_layer_controller_.WriteLinkSupervisionTimeout(handle, timeout);
  auto packet =
      bluetooth::hci::WriteLinkSupervisionTimeoutCompleteBuilder::Create(
          kNumCommandPackets, status, handle);
  send_event_(std::move(packet));
}

void DualModeController::ReadLocalName(CommandPacketView command) {
  auto command_view = gd_hci::ReadLocalNameView::Create(command);
  ASSERT(command_view.IsValid());

  std::array<uint8_t, 248> local_name;
  local_name.fill(0x00);
  size_t len = properties_.GetName().size();
  if (len > 247) {
    len = 247;  // one byte for NULL octet (0x00)
  }
  std::copy_n(properties_.GetName().begin(), len, local_name.begin());

  auto packet = bluetooth::hci::ReadLocalNameCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, local_name);
  send_event_(std::move(packet));
}

void DualModeController::WriteLocalName(CommandPacketView command) {
  auto command_view = gd_hci::WriteLocalNameView::Create(command);
  ASSERT(command_view.IsValid());
  const auto local_name = command_view.GetLocalName();
  std::vector<uint8_t> name_vec(248);
  for (size_t i = 0; i < 248; i++) {
    name_vec[i] = local_name[i];
  }
  properties_.SetName(name_vec);
  auto packet = bluetooth::hci::WriteLocalNameCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::WriteExtendedInquiryResponse(
    CommandPacketView command) {
  auto command_view = gd_hci::WriteExtendedInquiryResponseView::Create(command);
  ASSERT(command_view.IsValid());
  properties_.SetExtendedInquiryData(std::vector<uint8_t>(
      command_view.GetPayload().begin() + 1, command_view.GetPayload().end()));
  auto packet =
      bluetooth::hci::WriteExtendedInquiryResponseCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::RefreshEncryptionKey(CommandPacketView command) {
  auto command_view = gd_hci::RefreshEncryptionKeyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t handle = command_view.GetConnectionHandle();
  auto status_packet =
      bluetooth::hci::RefreshEncryptionKeyStatusBuilder::Create(
          ErrorCode::SUCCESS, kNumCommandPackets);
  send_event_(std::move(status_packet));
  // TODO: Support this in the link layer
  auto complete_packet =
      bluetooth::hci::EncryptionKeyRefreshCompleteBuilder::Create(
          ErrorCode::SUCCESS, handle);
  send_event_(std::move(complete_packet));
}

void DualModeController::WriteVoiceSetting(CommandPacketView command) {
  auto command_view = gd_hci::WriteVoiceSettingView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WriteVoiceSettingCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadNumberOfSupportedIac(CommandPacketView command) {
  auto command_view = gd_hci::ReadNumberOfSupportedIacView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint8_t num_support_iac = 0x1;
  auto packet = bluetooth::hci::ReadNumberOfSupportedIacCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, num_support_iac);
  send_event_(std::move(packet));
}

void DualModeController::ReadCurrentIacLap(CommandPacketView command) {
  auto command_view = gd_hci::ReadCurrentIacLapView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  gd_hci::Lap lap;
  lap.lap_ = 0x30;
  auto packet = bluetooth::hci::ReadCurrentIacLapCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, {lap});
  send_event_(std::move(packet));
}

void DualModeController::WriteCurrentIacLap(CommandPacketView command) {
  auto command_view = gd_hci::WriteCurrentIacLapView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WriteCurrentIacLapCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadPageScanActivity(CommandPacketView command) {
  auto command_view = gd_hci::ReadPageScanActivityView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t interval = 0x1000;
  uint16_t window = 0x0012;
  auto packet = bluetooth::hci::ReadPageScanActivityCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, interval, window);
  send_event_(std::move(packet));
}

void DualModeController::WritePageScanActivity(CommandPacketView command) {
  auto command_view = gd_hci::WritePageScanActivityView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WritePageScanActivityCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadInquiryScanActivity(CommandPacketView command) {
  auto command_view = gd_hci::ReadInquiryScanActivityView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint16_t interval = 0x1000;
  uint16_t window = 0x0012;
  auto packet = bluetooth::hci::ReadInquiryScanActivityCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, interval, window);
  send_event_(std::move(packet));
}

void DualModeController::WriteInquiryScanActivity(CommandPacketView command) {
  auto command_view = gd_hci::WriteInquiryScanActivityView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::WriteInquiryScanActivityCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::ReadScanEnable(CommandPacketView command) {
  auto command_view = gd_hci::ReadScanEnableView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::ReadScanEnableCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, gd_hci::ScanEnable::NO_SCANS);
  send_event_(std::move(packet));
}

void DualModeController::WriteScanEnable(CommandPacketView command) {
  auto command_view = gd_hci::WriteScanEnableView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetInquiryScanEnable(
      command_view.GetScanEnable() ==
          gd_hci::ScanEnable::INQUIRY_AND_PAGE_SCAN ||
      command_view.GetScanEnable() == gd_hci::ScanEnable::INQUIRY_SCAN_ONLY);
  link_layer_controller_.SetPageScanEnable(
      command_view.GetScanEnable() ==
          gd_hci::ScanEnable::INQUIRY_AND_PAGE_SCAN ||
      command_view.GetScanEnable() == gd_hci::ScanEnable::PAGE_SCAN_ONLY);
  auto packet = bluetooth::hci::WriteScanEnableCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::SetEventFilter(CommandPacketView command) {
  auto command_view = gd_hci::SetEventFilterView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::SetEventFilterCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::Inquiry(CommandPacketView command) {
  auto command_view = gd_hci::InquiryView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetInquiryLAP(command_view.GetLap().lap_);
  link_layer_controller_.SetInquiryMaxResponses(command_view.GetNumResponses());
  link_layer_controller_.StartInquiry(
      std::chrono::milliseconds(command_view.GetInquiryLength() * 1280));

  auto packet = bluetooth::hci::InquiryStatusBuilder::Create(
      ErrorCode::SUCCESS, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::InquiryCancel(CommandPacketView command) {
  auto command_view = gd_hci::InquiryCancelView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.InquiryCancel();
  auto packet = bluetooth::hci::InquiryCancelCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::AcceptConnectionRequest(CommandPacketView command) {
  auto command_view = gd_hci::AcceptConnectionRequestView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  Address addr = command_view.GetBdAddr();
  bool try_role_switch = command_view.GetRole() ==
                         gd_hci::AcceptConnectionRequestRole::BECOME_MASTER;
  auto status =
      link_layer_controller_.AcceptConnectionRequest(addr, try_role_switch);
  auto packet = bluetooth::hci::AcceptConnectionRequestStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::RejectConnectionRequest(CommandPacketView command) {
  auto command_view = gd_hci::RejectConnectionRequestView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  Address addr = command_view.GetBdAddr();
  uint8_t reason = static_cast<uint8_t>(command_view.GetReason());
  auto status = link_layer_controller_.RejectConnectionRequest(addr, reason);
  auto packet = bluetooth::hci::RejectConnectionRequestStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::LinkKeyRequestReply(CommandPacketView command) {
  auto command_view = gd_hci::LinkKeyRequestReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  Address addr = command_view.GetBdAddr();
  auto key = command_view.GetLinkKey();
  auto status = link_layer_controller_.LinkKeyRequestReply(addr, key);
  auto packet = bluetooth::hci::LinkKeyRequestReplyCompleteBuilder::Create(
      kNumCommandPackets, status);
  send_event_(std::move(packet));
}

void DualModeController::LinkKeyRequestNegativeReply(
    CommandPacketView command) {
  auto command_view = gd_hci::LinkKeyRequestNegativeReplyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  Address addr = command_view.GetBdAddr();
  auto status = link_layer_controller_.LinkKeyRequestNegativeReply(addr);
  auto packet =
      bluetooth::hci::LinkKeyRequestNegativeReplyCompleteBuilder::Create(
          kNumCommandPackets, status, addr);
  send_event_(std::move(packet));
}

void DualModeController::DeleteStoredLinkKey(CommandPacketView command) {
  auto command_view = gd_hci::DeleteStoredLinkKeyView::Create(
      gd_hci::SecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t deleted_keys = 0;

  auto flag = command_view.GetDeleteAllFlag();
  if (flag == gd_hci::DeleteStoredLinkKeyDeleteAllFlag::SPECIFIED_BD_ADDR) {
    Address addr = command_view.GetBdAddr();
    deleted_keys = security_manager_.DeleteKey(addr);
  }

  if (flag == gd_hci::DeleteStoredLinkKeyDeleteAllFlag::ALL) {
    security_manager_.DeleteAllKeys();
  }

  auto packet = bluetooth::hci::DeleteStoredLinkKeyCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, deleted_keys);

  send_event_(std::move(packet));
}

void DualModeController::RemoteNameRequest(CommandPacketView command) {
  auto command_view = gd_hci::RemoteNameRequestView::Create(
      gd_hci::DiscoveryCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address remote_addr = command_view.GetBdAddr();

  auto status = link_layer_controller_.SendCommandToRemoteByAddress(
      OpCode::REMOTE_NAME_REQUEST, command_view.GetPayload(), remote_addr);

  auto packet = bluetooth::hci::RemoteNameRequestStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::LeSetEventMask(CommandPacketView command) {
  auto command_view = gd_hci::LeSetEventMaskView::Create(command);
  ASSERT(command_view.IsValid());
  /*
  uint64_t mask = args.begin().extract<uint64_t>();
  link_layer_controller_.SetLeEventMask(mask);
*/
  auto packet = bluetooth::hci::LeSetEventMaskCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeReadBufferSize(CommandPacketView command) {
  auto command_view = gd_hci::LeReadBufferSizeView::Create(command);
  ASSERT(command_view.IsValid());

  bluetooth::hci::LeBufferSize le_buffer_size;
  le_buffer_size.le_data_packet_length_ = properties_.GetLeDataPacketLength();
  le_buffer_size.total_num_le_packets_ = properties_.GetTotalNumLeDataPackets();

  auto packet = bluetooth::hci::LeReadBufferSizeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, le_buffer_size);
  send_event_(std::move(packet));
}

void DualModeController::LeReadLocalSupportedFeatures(
    CommandPacketView command) {
  auto command_view = gd_hci::LeReadLocalSupportedFeaturesView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet =
      bluetooth::hci::LeReadLocalSupportedFeaturesCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS,
          properties_.GetLeSupportedFeatures());
  send_event_(std::move(packet));
}

void DualModeController::LeSetRandomAddress(CommandPacketView command) {
  auto command_view = gd_hci::LeSetRandomAddressView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  properties_.SetLeAddress(command_view.GetRandomAddress());
  auto packet = bluetooth::hci::LeSetRandomAddressCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetAdvertisingParameters(CommandPacketView command) {
  auto command_view = gd_hci::LeSetAdvertisingParametersView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  properties_.SetLeAdvertisingParameters(
      command_view.GetIntervalMin(), command_view.GetIntervalMax(),
      static_cast<uint8_t>(command_view.GetType()),
      static_cast<uint8_t>(command_view.GetOwnAddressType()),
      static_cast<uint8_t>(command_view.GetPeerAddressType()),
      command_view.GetPeerAddress(), command_view.GetChannelMap(),
      static_cast<uint8_t>(command_view.GetFilterPolicy()));

  auto packet =
      bluetooth::hci::LeSetAdvertisingParametersCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetAdvertisingData(CommandPacketView command) {
  auto command_view = gd_hci::LeSetAdvertisingDataView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  auto payload = command.GetPayload();
  std::vector<uint8_t> payload_bytes{payload.begin() + 1,
                                     payload.begin() + *payload.begin()};
  ASSERT_LOG(command_view.IsValid(), "%s command.size() = %zu",
             gd_hci::OpCodeText(command.GetOpCode()).c_str(), command.size());
  ASSERT(command_view.GetPayload().size() == 32);
  properties_.SetLeAdvertisement(payload_bytes);
  auto packet = bluetooth::hci::LeSetAdvertisingDataCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetScanResponseData(CommandPacketView command) {
  auto command_view = gd_hci::LeSetScanResponseDataView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  ASSERT(command_view.GetPayload().size() == 32);
  properties_.SetLeScanResponse(std::vector<uint8_t>(
      command_view.GetPayload().begin() + 1, command_view.GetPayload().end()));
  auto packet = bluetooth::hci::LeSetScanResponseDataCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetAdvertisingEnable(CommandPacketView command) {
  auto command_view = gd_hci::LeSetAdvertisingEnableView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto status = link_layer_controller_.SetLeAdvertisingEnable(
      command_view.GetAdvertisingEnable() == gd_hci::Enable::ENABLED);
  auto packet = bluetooth::hci::LeSetAdvertisingEnableCompleteBuilder::Create(
      kNumCommandPackets, status);
  send_event_(std::move(packet));
}

void DualModeController::LeSetScanParameters(CommandPacketView command) {
  auto command_view = gd_hci::LeSetScanParametersView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetLeScanType(
      static_cast<uint8_t>(command_view.GetLeScanType()));
  link_layer_controller_.SetLeScanInterval(command_view.GetLeScanInterval());
  link_layer_controller_.SetLeScanWindow(command_view.GetLeScanWindow());
  link_layer_controller_.SetLeAddressType(
      static_cast<uint8_t>(command_view.GetOwnAddressType()));
  link_layer_controller_.SetLeScanFilterPolicy(
      static_cast<uint8_t>(command_view.GetScanningFilterPolicy()));
  auto packet = bluetooth::hci::LeSetScanParametersCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetScanEnable(CommandPacketView command) {
  auto command_view = gd_hci::LeSetScanEnableView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  if (command_view.GetLeScanEnable() == gd_hci::Enable::ENABLED) {
    link_layer_controller_.SetLeScanEnable(gd_hci::OpCode::LE_SET_SCAN_ENABLE);
  } else {
    link_layer_controller_.SetLeScanEnable(gd_hci::OpCode::NONE);
  }
  link_layer_controller_.SetLeFilterDuplicates(
      command_view.GetFilterDuplicates() == gd_hci::Enable::ENABLED);
  auto packet = bluetooth::hci::LeSetScanEnableCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeCreateConnection(CommandPacketView command) {
  auto command_view = gd_hci::LeCreateConnectionView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetLeScanInterval(command_view.GetLeScanInterval());
  link_layer_controller_.SetLeScanWindow(command_view.GetLeScanWindow());
  uint8_t initiator_filter_policy =
      static_cast<uint8_t>(command_view.GetInitiatorFilterPolicy());
  link_layer_controller_.SetLeInitiatorFilterPolicy(initiator_filter_policy);

  if (initiator_filter_policy == 0) {  // White list not used
    uint8_t peer_address_type =
        static_cast<uint8_t>(command_view.GetPeerAddressType());
    Address peer_address = command_view.GetPeerAddress();
    link_layer_controller_.SetLePeerAddressType(peer_address_type);
    link_layer_controller_.SetLePeerAddress(peer_address);
  }
  link_layer_controller_.SetLeAddressType(
      static_cast<uint8_t>(command_view.GetOwnAddressType()));
  link_layer_controller_.SetLeConnectionIntervalMin(
      command_view.GetConnIntervalMin());
  link_layer_controller_.SetLeConnectionIntervalMax(
      command_view.GetConnIntervalMax());
  link_layer_controller_.SetLeConnectionLatency(command_view.GetConnLatency());
  link_layer_controller_.SetLeSupervisionTimeout(
      command_view.GetSupervisionTimeout());
  link_layer_controller_.SetLeMinimumCeLength(
      command_view.GetMinimumCeLength());
  link_layer_controller_.SetLeMaximumCeLength(
      command_view.GetMaximumCeLength());

  auto status = link_layer_controller_.SetLeConnect(true);

  auto packet = bluetooth::hci::LeCreateConnectionStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::LeConnectionUpdate(CommandPacketView command) {
  auto command_view = gd_hci::LeConnectionUpdateView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  auto status_packet = bluetooth::hci::LeConnectionUpdateStatusBuilder::Create(
      ErrorCode::CONNECTION_REJECTED_UNACCEPTABLE_BD_ADDR, kNumCommandPackets);
  send_event_(std::move(status_packet));

  auto complete_packet =
      bluetooth::hci::LeConnectionUpdateCompleteBuilder::Create(
          ErrorCode::SUCCESS, 0x0002, 0x0006, 0x0000, 0x01f4);
  send_event_(std::move(complete_packet));
}

void DualModeController::CreateConnection(CommandPacketView command) {
  auto command_view = gd_hci::CreateConnectionView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  Address address = command_view.GetBdAddr();
  uint16_t packet_type = command_view.GetPacketType();
  uint8_t page_scan_mode =
      static_cast<uint8_t>(command_view.GetPageScanRepetitionMode());
  uint16_t clock_offset =
      (command_view.GetClockOffsetValid() == gd_hci::ClockOffsetValid::VALID
           ? command_view.GetClockOffset()
           : 0);
  uint8_t allow_role_switch =
      static_cast<uint8_t>(command_view.GetAllowRoleSwitch());

  auto status = link_layer_controller_.CreateConnection(
      address, packet_type, page_scan_mode, clock_offset, allow_role_switch);

  auto packet = bluetooth::hci::CreateConnectionStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::Disconnect(CommandPacketView command) {
  auto command_view = gd_hci::DisconnectView::Create(
      gd_hci::ConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();
  uint8_t reason = static_cast<uint8_t>(command_view.GetReason());

  auto status = link_layer_controller_.Disconnect(handle, reason);

  auto packet = bluetooth::hci::DisconnectStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::LeConnectionCancel(CommandPacketView command) {
  auto command_view = gd_hci::LeCreateConnectionCancelView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.SetLeConnect(false);
  auto packet = bluetooth::hci::LeCreateConnectionCancelStatusBuilder::Create(
      ErrorCode::SUCCESS, kNumCommandPackets);
  send_event_(std::move(packet));
  /* For testing Jakub's patch:  Figure out a neat way to call this without
     recompiling.  I'm thinking about a bad device. */
  /*
  SendCommandCompleteOnlyStatus(OpCode::LE_CREATE_CONNECTION_CANCEL,
                                ErrorCode::COMMAND_DISALLOWED);
  */
}

void DualModeController::LeReadWhiteListSize(CommandPacketView command) {
  auto command_view = gd_hci::LeReadWhiteListSizeView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::LeReadWhiteListSizeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, properties_.GetLeWhiteListSize());
  send_event_(std::move(packet));
}

void DualModeController::LeClearWhiteList(CommandPacketView command) {
  auto command_view = gd_hci::LeClearWhiteListView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.LeWhiteListClear();
  auto packet = bluetooth::hci::LeClearWhiteListCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeAddDeviceToWhiteList(CommandPacketView command) {
  auto command_view = gd_hci::LeAddDeviceToWhiteListView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  if (link_layer_controller_.LeWhiteListFull()) {
    auto packet = bluetooth::hci::LeAddDeviceToWhiteListCompleteBuilder::Create(
        kNumCommandPackets, ErrorCode::MEMORY_CAPACITY_EXCEEDED);
    send_event_(std::move(packet));
    return;
  }
  uint8_t addr_type = static_cast<uint8_t>(command_view.GetAddressType());
  Address address = command_view.GetAddress();
  link_layer_controller_.LeWhiteListAddDevice(address, addr_type);
  auto packet = bluetooth::hci::LeAddDeviceToWhiteListCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeRemoveDeviceFromWhiteList(
    CommandPacketView command) {
  auto command_view = gd_hci::LeRemoveDeviceFromWhiteListView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint8_t addr_type = static_cast<uint8_t>(command_view.GetAddressType());
  Address address = command_view.GetAddress();
  link_layer_controller_.LeWhiteListRemoveDevice(address, addr_type);
  auto packet =
      bluetooth::hci::LeRemoveDeviceFromWhiteListCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeClearResolvingList(CommandPacketView command) {
  auto command_view = gd_hci::LeClearResolvingListView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  link_layer_controller_.LeResolvingListClear();
  auto packet = bluetooth::hci::LeClearResolvingListCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeAddDeviceToResolvingList(CommandPacketView command) {
  auto command_view = gd_hci::LeAddDeviceToResolvingListView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  if (link_layer_controller_.LeResolvingListFull()) {
    auto packet =
        bluetooth::hci::LeAddDeviceToResolvingListCompleteBuilder::Create(
            kNumCommandPackets, ErrorCode::MEMORY_CAPACITY_EXCEEDED);
    send_event_(std::move(packet));
    return;
  }
  uint8_t addr_type =
      static_cast<uint8_t>(command_view.GetPeerIdentityAddressType());
  Address address = command_view.GetPeerIdentityAddress();
  std::array<uint8_t, LinkLayerController::kIrk_size> peerIrk =
      command_view.GetPeerIrk();
  std::array<uint8_t, LinkLayerController::kIrk_size> localIrk =
      command_view.GetLocalIrk();

  link_layer_controller_.LeResolvingListAddDevice(address, addr_type, peerIrk,
                                                  localIrk);
  auto packet =
      bluetooth::hci::LeAddDeviceToResolvingListCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeRemoveDeviceFromResolvingList(
    CommandPacketView command) {
  auto command_view = gd_hci::LeRemoveDeviceFromResolvingListView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint8_t addr_type =
      static_cast<uint8_t>(command_view.GetPeerIdentityAddressType());
  Address address = command_view.GetPeerIdentityAddress();
  link_layer_controller_.LeResolvingListRemoveDevice(address, addr_type);
  auto packet =
      bluetooth::hci::LeRemoveDeviceFromResolvingListCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetExtendedScanParameters(
    CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedScanParametersView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto parameters = command_view.GetParameters();
  // Multiple phys are not supported.
  ASSERT(command_view.GetScanningPhys() == 1);
  ASSERT(parameters.size() == 1);

  link_layer_controller_.SetLeScanType(
      static_cast<uint8_t>(parameters[0].le_scan_type_));
  link_layer_controller_.SetLeScanInterval(parameters[0].le_scan_interval_);
  link_layer_controller_.SetLeScanWindow(parameters[0].le_scan_window_);
  link_layer_controller_.SetLeAddressType(
      static_cast<uint8_t>(command_view.GetOwnAddressType()));
  link_layer_controller_.SetLeScanFilterPolicy(
      static_cast<uint8_t>(command_view.GetScanningFilterPolicy()));
  auto packet =
      bluetooth::hci::LeSetExtendedScanParametersCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetExtendedScanEnable(CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedScanEnableView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  if (command_view.GetEnable() == gd_hci::Enable::ENABLED) {
    link_layer_controller_.SetLeScanEnable(
        gd_hci::OpCode::LE_SET_EXTENDED_SCAN_ENABLE);
  } else {
    link_layer_controller_.SetLeScanEnable(gd_hci::OpCode::NONE);
  }
  link_layer_controller_.SetLeFilterDuplicates(
      command_view.GetFilterDuplicates() == gd_hci::FilterDuplicates::ENABLED);
  auto packet = bluetooth::hci::LeSetExtendedScanEnableCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeExtendedCreateConnection(CommandPacketView command) {
  auto command_view = gd_hci::LeExtendedCreateConnectionView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());
  ASSERT_LOG(command_view.GetInitiatingPhys() == 1, "Only LE_1M is supported");
  auto params = command_view.GetPhyScanParameters();
  link_layer_controller_.SetLeScanInterval(params[0].scan_interval_);
  link_layer_controller_.SetLeScanWindow(params[0].scan_window_);
  auto initiator_filter_policy = command_view.GetInitiatorFilterPolicy();
  link_layer_controller_.SetLeInitiatorFilterPolicy(
      static_cast<uint8_t>(initiator_filter_policy));

  if (initiator_filter_policy ==
      gd_hci::InitiatorFilterPolicy::USE_PEER_ADDRESS) {
    link_layer_controller_.SetLePeerAddressType(
        static_cast<uint8_t>(command_view.GetPeerAddressType()));
    link_layer_controller_.SetLePeerAddress(command_view.GetPeerAddress());
  }
  link_layer_controller_.SetLeAddressType(
      static_cast<uint8_t>(command_view.GetOwnAddressType()));
  link_layer_controller_.SetLeConnectionIntervalMin(
      params[0].conn_interval_min_);
  link_layer_controller_.SetLeConnectionIntervalMax(
      params[0].conn_interval_max_);
  link_layer_controller_.SetLeConnectionLatency(params[0].conn_latency_);
  link_layer_controller_.SetLeSupervisionTimeout(
      params[0].supervision_timeout_);
  link_layer_controller_.SetLeMinimumCeLength(params[0].min_ce_length_);
  link_layer_controller_.SetLeMaximumCeLength(params[0].max_ce_length_);

  auto status = link_layer_controller_.SetLeConnect(true);

  send_event_(bluetooth::hci::LeExtendedCreateConnectionStatusBuilder::Create(
      status, kNumCommandPackets));
}

void DualModeController::LeSetPrivacyMode(CommandPacketView command) {
  auto command_view = gd_hci::LeSetPrivacyModeView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint8_t peer_identity_address_type =
      static_cast<uint8_t>(command_view.GetPeerIdentityAddressType());
  Address peer_identity_address = command_view.GetPeerIdentityAddress();
  uint8_t privacy_mode = static_cast<uint8_t>(command_view.GetPrivacyMode());

  if (link_layer_controller_.LeResolvingListContainsDevice(
          peer_identity_address, peer_identity_address_type)) {
    link_layer_controller_.LeSetPrivacyMode(
        peer_identity_address_type, peer_identity_address, privacy_mode);
  }

  auto packet = bluetooth::hci::LeSetPrivacyModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeReadRemoteFeatures(CommandPacketView command) {
  auto command_view = gd_hci::LeReadRemoteFeaturesView::Create(
      gd_hci::LeConnectionManagementCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();

  auto status = link_layer_controller_.SendCommandToRemoteByHandle(
      OpCode::LE_READ_REMOTE_FEATURES, command_view.GetPayload(), handle);

  auto packet = bluetooth::hci::LeConnectionUpdateStatusBuilder::Create(
      status, kNumCommandPackets);
  send_event_(std::move(packet));
}

void DualModeController::LeRand(CommandPacketView command) {
  auto command_view = gd_hci::LeRandView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());
  uint64_t random_val = 0;
  for (size_t rand_bytes = 0; rand_bytes < sizeof(uint64_t); rand_bytes += sizeof(RAND_MAX)) {
    random_val = (random_val << (8 * sizeof(RAND_MAX))) | random();
  }

  auto packet = bluetooth::hci::LeRandCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS, random_val);
  send_event_(std::move(packet));
}

void DualModeController::LeReadSupportedStates(CommandPacketView command) {
  auto command_view = gd_hci::LeReadSupportedStatesView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::LeReadSupportedStatesCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      properties_.GetLeSupportedStates());
  send_event_(std::move(packet));
}

void DualModeController::LeVendorCap(CommandPacketView command) {
  auto command_view = gd_hci::LeGetVendorCapabilitiesView::Create(
      gd_hci::VendorCommandView::Create(command));
  ASSERT(command_view.IsValid());
  vector<uint8_t> caps = properties_.GetLeVendorCap();
  if (caps.size() == 0) {
    SendCommandCompleteUnknownOpCodeEvent(
        static_cast<uint16_t>(OpCode::LE_GET_VENDOR_CAPABILITIES));
    return;
  }

  std::unique_ptr<bluetooth::packet::RawBuilder> raw_builder_ptr =
      std::make_unique<bluetooth::packet::RawBuilder>();
  raw_builder_ptr->AddOctets1(static_cast<uint8_t>(ErrorCode::SUCCESS));
  raw_builder_ptr->AddOctets(properties_.GetLeVendorCap());

  auto packet = bluetooth::hci::CommandCompleteBuilder::Create(
      kNumCommandPackets, OpCode::LE_GET_VENDOR_CAPABILITIES,
      std::move(raw_builder_ptr));
  send_event_(std::move(packet));
}

void DualModeController::LeVendorMultiAdv(CommandPacketView command) {
  auto command_view = gd_hci::LeMultiAdvtView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  SendCommandCompleteUnknownOpCodeEvent(
      static_cast<uint16_t>(OpCode::LE_MULTI_ADVT));
}

void DualModeController::LeAdvertisingFilter(CommandPacketView command) {
  auto command_view = gd_hci::LeAdvFilterView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  SendCommandCompleteUnknownOpCodeEvent(
      static_cast<uint16_t>(OpCode::LE_ADV_FILTER));
}

void DualModeController::LeEnergyInfo(CommandPacketView command) {
  auto command_view = gd_hci::LeEnergyInfoView::Create(
      gd_hci::VendorCommandView::Create(command));
  ASSERT(command_view.IsValid());
  SendCommandCompleteUnknownOpCodeEvent(
      static_cast<uint16_t>(OpCode::LE_ENERGY_INFO));
}

void DualModeController::LeSetExtendedAdvertisingRandomAddress(
    CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedAdvertisingRandomAddressView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  properties_.SetLeAddress(command_view.GetAdvertisingRandomAddress());
  send_event_(
      bluetooth::hci::LeSetExtendedAdvertisingRandomAddressCompleteBuilder::
          Create(kNumCommandPackets, ErrorCode::SUCCESS));
}

void DualModeController::LeSetExtendedAdvertisingParameters(
    CommandPacketView command) {
  auto command_view =
      gd_hci::LeSetExtendedAdvertisingLegacyParametersView::Create(
          gd_hci::LeAdvertisingCommandView::Create(command));
  // TODO: Support non-legacy parameters
  ASSERT(command_view.IsValid());
  properties_.SetLeAdvertisingParameters(
      command_view.GetPrimaryAdvertisingIntervalMin(),
      command_view.GetPrimaryAdvertisingIntervalMax(),
      static_cast<uint8_t>(bluetooth::hci::AdvertisingEventType::ADV_IND),
      static_cast<uint8_t>(command_view.GetOwnAddressType()),
      static_cast<uint8_t>(command_view.GetPeerAddressType()),
      command_view.GetPeerAddress(),
      command_view.GetPrimaryAdvertisingChannelMap(),
      static_cast<uint8_t>(command_view.GetAdvertisingFilterPolicy()));

  send_event_(
      bluetooth::hci::LeSetExtendedAdvertisingParametersCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS, 0xa5));
}

void DualModeController::LeSetExtendedAdvertisingData(
    CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedAdvertisingDataView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto raw_command_view = gd_hci::LeSetExtendedAdvertisingDataRawView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(raw_command_view.IsValid());
  properties_.SetLeAdvertisement(raw_command_view.GetAdvertisingData());
  auto packet =
      bluetooth::hci::LeSetExtendedAdvertisingDataCompleteBuilder::Create(
          kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::LeSetExtendedAdvertisingScanResponse(
    CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedAdvertisingScanResponseView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  properties_.SetLeScanResponse(std::vector<uint8_t>(
      command_view.GetPayload().begin() + 1, command_view.GetPayload().end()));
  send_event_(
      bluetooth::hci::LeSetExtendedAdvertisingScanResponseCompleteBuilder::
          Create(kNumCommandPackets, ErrorCode::SUCCESS));
}

void DualModeController::LeSetExtendedAdvertisingEnable(
    CommandPacketView command) {
  auto command_view = gd_hci::LeSetExtendedAdvertisingEnableView::Create(
      gd_hci::LeAdvertisingCommandView::Create(command));
  ASSERT(command_view.IsValid());
  auto status = link_layer_controller_.SetLeAdvertisingEnable(
      command_view.GetEnable() == gd_hci::Enable::ENABLED);
  send_event_(
      bluetooth::hci::LeSetExtendedAdvertisingEnableCompleteBuilder::Create(
          kNumCommandPackets, status));
}

void DualModeController::LeExtendedScanParams(CommandPacketView command) {
  auto command_view = gd_hci::LeExtendedScanParamsView::Create(
      gd_hci::LeScanningCommandView::Create(command));
  ASSERT(command_view.IsValid());
  SendCommandCompleteUnknownOpCodeEvent(
      static_cast<uint16_t>(OpCode::LE_EXTENDED_SCAN_PARAMS));
}

void DualModeController::LeStartEncryption(CommandPacketView command) {
  auto command_view = gd_hci::LeStartEncryptionView::Create(
      gd_hci::LeSecurityCommandView::Create(command));
  ASSERT(command_view.IsValid());

  uint16_t handle = command_view.GetConnectionHandle();

  auto status_packet = bluetooth::hci::LeStartEncryptionStatusBuilder::Create(
      ErrorCode::SUCCESS, kNumCommandPackets);
  send_event_(std::move(status_packet));

  auto complete_packet = bluetooth::hci::EncryptionChangeBuilder::Create(
      ErrorCode::SUCCESS, handle, bluetooth::hci::EncryptionEnabled::OFF);
  send_event_(std::move(complete_packet));
}

void DualModeController::ReadLoopbackMode(CommandPacketView command) {
  auto command_view = gd_hci::ReadLoopbackModeView::Create(command);
  ASSERT(command_view.IsValid());
  auto packet = bluetooth::hci::ReadLoopbackModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS,
      static_cast<LoopbackMode>(loopback_mode_));
  send_event_(std::move(packet));
}

void DualModeController::WriteLoopbackMode(CommandPacketView command) {
  auto command_view = gd_hci::WriteLoopbackModeView::Create(command);
  ASSERT(command_view.IsValid());
  loopback_mode_ = command_view.GetLoopbackMode();
  // ACL channel
  uint16_t acl_handle = 0x123;
  auto packet_acl = bluetooth::hci::ConnectionCompleteBuilder::Create(
      ErrorCode::SUCCESS, acl_handle, properties_.GetAddress(),
      bluetooth::hci::LinkType::ACL, bluetooth::hci::Enable::DISABLED);
  send_event_(std::move(packet_acl));
  // SCO channel
  uint16_t sco_handle = 0x345;
  auto packet_sco = bluetooth::hci::ConnectionCompleteBuilder::Create(
      ErrorCode::SUCCESS, sco_handle, properties_.GetAddress(),
      bluetooth::hci::LinkType::SCO, bluetooth::hci::Enable::DISABLED);
  send_event_(std::move(packet_sco));
  auto packet = bluetooth::hci::WriteLoopbackModeCompleteBuilder::Create(
      kNumCommandPackets, ErrorCode::SUCCESS);
  send_event_(std::move(packet));
}

void DualModeController::SetAddress(Address address) {
  properties_.SetAddress(address);
}

}  // namespace test_vendor_lib
