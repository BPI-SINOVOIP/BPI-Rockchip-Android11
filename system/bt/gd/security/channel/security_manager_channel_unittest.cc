/*
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#include "security_manager_channel.h"

#include <gtest/gtest.h>

#include "hci/hci_packets.h"
#include "packet/raw_builder.h"
#include "security/smp_packets.h"
#include "security/test/fake_hci_layer.h"

namespace bluetooth {
namespace security {
namespace channel {
namespace {

using bluetooth::security::channel::SecurityManagerChannel;
using hci::Address;
using hci::AuthenticationRequirements;
using hci::CommandCompleteBuilder;
using hci::IoCapabilityRequestReplyBuilder;
using hci::IoCapabilityRequestView;
using hci::OobDataPresent;
using hci::OpCode;
using os::Handler;
using os::Thread;
using packet::RawBuilder;

class SecurityManagerChannelCallback : public ISecurityManagerChannelListener {
 public:
  // HCI
  bool receivedChangeConnectionLinkKeyComplete = false;
  bool receivedMasterLinkKeyComplete = false;
  bool receivedPinCodeRequest = false;
  bool receivedLinkKeyRequest = false;
  bool receivedLinkKeyNotification = false;
  bool receivedIoCapabilityRequest = false;
  bool receivedIoCapabilityResponse = false;
  bool receivedSimplePairingComplete = false;
  bool receivedReturnLinkKeys = false;
  bool receivedEncryptionChange = false;
  bool receivedEncryptionKeyRefreshComplete = false;
  bool receivedRemoteOobDataRequest = false;
  bool receivedUserPasskeyNotification = false;
  bool receivedUserPasskeyRequest = false;
  bool receivedKeypressNotification = false;
  bool receivedUserConfirmationRequest = false;

  void OnReceive(hci::AddressWithType device, hci::ChangeConnectionLinkKeyCompleteView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedChangeConnectionLinkKeyComplete = true;
  }
  void OnReceive(hci::AddressWithType device, hci::MasterLinkKeyCompleteView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedMasterLinkKeyComplete = true;
  }
  void OnReceive(hci::AddressWithType device, hci::PinCodeRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedPinCodeRequest = true;
  }
  void OnReceive(hci::AddressWithType device, hci::LinkKeyRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedLinkKeyRequest = true;
  }
  void OnReceive(hci::AddressWithType device, hci::LinkKeyNotificationView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedLinkKeyNotification = true;
  }
  void OnReceive(hci::AddressWithType device, hci::IoCapabilityRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedIoCapabilityRequest = true;
  }
  void OnReceive(hci::AddressWithType device, hci::IoCapabilityResponseView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedIoCapabilityResponse = true;
  }
  void OnReceive(hci::AddressWithType device, hci::SimplePairingCompleteView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedSimplePairingComplete = true;
  }
  void OnReceive(hci::AddressWithType device, hci::ReturnLinkKeysView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedReturnLinkKeys = true;
  }
  void OnReceive(hci::AddressWithType device, hci::EncryptionChangeView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedEncryptionChange = true;
  }
  void OnReceive(hci::AddressWithType device, hci::EncryptionKeyRefreshCompleteView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedEncryptionKeyRefreshComplete = true;
  }
  void OnReceive(hci::AddressWithType device, hci::RemoteOobDataRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedRemoteOobDataRequest = true;
  }
  void OnReceive(hci::AddressWithType device, hci::UserPasskeyNotificationView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedUserPasskeyNotification = true;
  }
  void OnReceive(hci::AddressWithType device, hci::KeypressNotificationView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedKeypressNotification = true;
  }
  void OnReceive(hci::AddressWithType device, hci::UserConfirmationRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedUserConfirmationRequest = true;
  }
  void OnReceive(hci::AddressWithType device, hci::UserPasskeyRequestView packet) {
    ASSERT_TRUE(packet.IsValid());
    receivedUserPasskeyRequest = true;
  }

  void OnHciEventReceived(EventPacketView packet) override {
    auto event = EventPacketView::Create(packet);
    ASSERT_LOG(event.IsValid(), "Received invalid packet");
    const hci::EventCode code = event.GetEventCode();
    switch (code) {
      case hci::EventCode::CHANGE_CONNECTION_LINK_KEY_COMPLETE:
        OnReceive(hci::AddressWithType(), hci::ChangeConnectionLinkKeyCompleteView::Create(event));
        break;
      case hci::EventCode::MASTER_LINK_KEY_COMPLETE:
        OnReceive(hci::AddressWithType(), hci::MasterLinkKeyCompleteView::Create(event));
        break;
      case hci::EventCode::PIN_CODE_REQUEST:
        OnReceive(hci::AddressWithType(), hci::PinCodeRequestView::Create(event));
        break;
      case hci::EventCode::LINK_KEY_REQUEST:
        OnReceive(hci::AddressWithType(), hci::LinkKeyRequestView::Create(event));
        break;
      case hci::EventCode::LINK_KEY_NOTIFICATION:
        OnReceive(hci::AddressWithType(), hci::LinkKeyNotificationView::Create(event));
        break;
      case hci::EventCode::IO_CAPABILITY_REQUEST:
        OnReceive(hci::AddressWithType(), hci::IoCapabilityRequestView::Create(event));
        break;
      case hci::EventCode::IO_CAPABILITY_RESPONSE:
        OnReceive(hci::AddressWithType(), hci::IoCapabilityResponseView::Create(event));
        break;
      case hci::EventCode::SIMPLE_PAIRING_COMPLETE:
        OnReceive(hci::AddressWithType(), hci::SimplePairingCompleteView::Create(event));
        break;
      case hci::EventCode::RETURN_LINK_KEYS:
        OnReceive(hci::AddressWithType(), hci::ReturnLinkKeysView::Create(event));
        break;
      case hci::EventCode::ENCRYPTION_CHANGE:
        OnReceive(hci::AddressWithType(), hci::EncryptionChangeView::Create(event));
        break;
      case hci::EventCode::ENCRYPTION_KEY_REFRESH_COMPLETE:
        OnReceive(hci::AddressWithType(), hci::EncryptionKeyRefreshCompleteView::Create(event));
        break;
      case hci::EventCode::REMOTE_OOB_DATA_REQUEST:
        OnReceive(hci::AddressWithType(), hci::RemoteOobDataRequestView::Create(event));
        break;
      case hci::EventCode::USER_PASSKEY_NOTIFICATION:
        OnReceive(hci::AddressWithType(), hci::UserPasskeyNotificationView::Create(event));
        break;
      case hci::EventCode::KEYPRESS_NOTIFICATION:
        OnReceive(hci::AddressWithType(), hci::KeypressNotificationView::Create(event));
        break;
      case hci::EventCode::USER_CONFIRMATION_REQUEST:
        OnReceive(hci::AddressWithType(), hci::UserConfirmationRequestView::Create(event));
        break;
      case hci::EventCode::USER_PASSKEY_REQUEST:
        OnReceive(hci::AddressWithType(), hci::UserPasskeyRequestView::Create(event));
        break;
      default:
        ASSERT_LOG(false, "Cannot handle received packet: %s", hci::EventCodeText(code).c_str());
        break;
    }
  }
};

class SecurityManagerChannelTest : public ::testing::Test {
 protected:
  void SetUp() override {
    handler_ = new Handler(&thread_);
    callback_ = new SecurityManagerChannelCallback();
    hci_layer_ = new FakeHciLayer();
    fake_registry_.InjectTestModule(&FakeHciLayer::Factory, hci_layer_);
    fake_registry_.Start<FakeHciLayer>(&thread_);
    channel_ = new SecurityManagerChannel(handler_, hci_layer_);
    channel_->SetChannelListener(callback_);
  }

  void TearDown() override {
    channel_->SetChannelListener(nullptr);
    handler_->Clear();
    fake_registry_.SynchronizeModuleHandler(&FakeHciLayer::Factory, std::chrono::milliseconds(20));
    fake_registry_.StopAll();
    delete handler_;
    delete channel_;
    delete callback_;
  }

  TestModuleRegistry fake_registry_;
  Thread& thread_ = fake_registry_.GetTestThread();
  Handler* handler_ = nullptr;
  FakeHciLayer* hci_layer_ = nullptr;
  SecurityManagerChannel* channel_ = nullptr;
  SecurityManagerChannelCallback* callback_ = nullptr;
  hci::AddressWithType device_;
};

TEST_F(SecurityManagerChannelTest, setup_teardown) {}

TEST_F(SecurityManagerChannelTest, recv_io_cap_request) {
  hci_layer_->IncomingEvent(hci::IoCapabilityRequestBuilder::Create(device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedIoCapabilityRequest);
}

TEST_F(SecurityManagerChannelTest, send_io_cap_request_reply) {
  // Arrange
  hci::IoCapability io_capability = (hci::IoCapability)0x00;
  OobDataPresent oob_present = (OobDataPresent)0x00;
  AuthenticationRequirements authentication_requirements = (AuthenticationRequirements)0x00;
  auto packet = hci::IoCapabilityRequestReplyBuilder::Create(device_.GetAddress(), io_capability, oob_present,
                                                             authentication_requirements);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_io_cap_request_neg_reply) {
  // Arrange
  auto packet =
      hci::IoCapabilityRequestNegativeReplyBuilder::Create(device_.GetAddress(), hci::ErrorCode::COMMAND_DISALLOWED);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_NEGATIVE_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_io_cap_response) {
  hci::IoCapability io_capability = (hci::IoCapability)0x00;
  OobDataPresent oob_present = (OobDataPresent)0x00;
  AuthenticationRequirements authentication_requirements = (AuthenticationRequirements)0x00;
  hci_layer_->IncomingEvent(hci::IoCapabilityResponseBuilder::Create(device_.GetAddress(), io_capability, oob_present,
                                                                     authentication_requirements));
  ASSERT_TRUE(callback_->receivedIoCapabilityResponse);
}

TEST_F(SecurityManagerChannelTest, recv_pin_code_request) {
  hci_layer_->IncomingEvent(hci::PinCodeRequestBuilder::Create(device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedPinCodeRequest);
}

TEST_F(SecurityManagerChannelTest, send_pin_code_request_reply) {
  // Arrange
  uint8_t pin_code_length = 6;
  std::array<uint8_t, 16> pin_code = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  auto packet = hci::PinCodeRequestReplyBuilder::Create(device_.GetAddress(), pin_code_length, pin_code);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::PIN_CODE_REQUEST_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_pin_code_request_neg_reply) {
  // Arrange
  auto packet = hci::PinCodeRequestNegativeReplyBuilder::Create(device_.GetAddress());

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::PIN_CODE_REQUEST_NEGATIVE_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_user_passkey_notification) {
  uint32_t passkey = 0x00;
  hci_layer_->IncomingEvent(hci::UserPasskeyNotificationBuilder::Create(device_.GetAddress(), passkey));
  ASSERT_TRUE(callback_->receivedUserPasskeyNotification);
}

TEST_F(SecurityManagerChannelTest, recv_user_confirmation_request) {
  uint32_t numeric_value = 0x0;
  hci_layer_->IncomingEvent(hci::UserConfirmationRequestBuilder::Create(device_.GetAddress(), numeric_value));
  ASSERT_TRUE(callback_->receivedUserConfirmationRequest);
}

TEST_F(SecurityManagerChannelTest, send_user_confirmation_request_reply) {
  // Arrange
  auto packet = hci::UserConfirmationRequestReplyBuilder::Create(device_.GetAddress());

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_user_confirmation_request_negative_reply) {
  // Arrange
  auto packet = hci::UserConfirmationRequestNegativeReplyBuilder::Create(device_.GetAddress());

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_remote_oob_data_request) {
  hci_layer_->IncomingEvent(hci::RemoteOobDataRequestBuilder::Create(device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedRemoteOobDataRequest);
}

TEST_F(SecurityManagerChannelTest, send_remote_oob_data_request_reply) {
  // Arrange
  std::array<uint8_t, 16> c = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  std::array<uint8_t, 16> r = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  auto packet = hci::RemoteOobDataRequestReplyBuilder::Create(device_.GetAddress(), c, r);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::REMOTE_OOB_DATA_REQUEST_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_remote_oob_data_request_neg_reply) {
  // Arrange
  auto packet = hci::RemoteOobDataRequestNegativeReplyBuilder::Create(device_.GetAddress());

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::REMOTE_OOB_DATA_REQUEST_NEGATIVE_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_read_local_oob_data) {
  // Arrange
  auto packet = hci::ReadLocalOobDataBuilder::Create();

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::READ_LOCAL_OOB_DATA, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_read_local_oob_extended_data) {
  // Arrange
  auto packet = hci::ReadLocalOobExtendedDataBuilder::Create();

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::READ_LOCAL_OOB_EXTENDED_DATA, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_link_key_request) {
  hci_layer_->IncomingEvent(hci::LinkKeyRequestBuilder::Create(device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedLinkKeyRequest);
}

TEST_F(SecurityManagerChannelTest, recv_link_key_notification) {
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci_layer_->IncomingEvent(
      hci::LinkKeyNotificationBuilder::Create(device_.GetAddress(), link_key, hci::KeyType::DEBUG_COMBINATION));
  ASSERT_TRUE(callback_->receivedLinkKeyNotification);
}

TEST_F(SecurityManagerChannelTest, recv_master_link_key_complete) {
  uint16_t connection_handle = 0x0;
  hci_layer_->IncomingEvent(
      hci::MasterLinkKeyCompleteBuilder::Create(hci::ErrorCode::SUCCESS, connection_handle, hci::KeyFlag::TEMPORARY));
  ASSERT_TRUE(callback_->receivedMasterLinkKeyComplete);
}

TEST_F(SecurityManagerChannelTest, recv_change_connection_link_key_complete) {
  uint16_t connection_handle = 0x0;
  hci_layer_->IncomingEvent(
      hci::ChangeConnectionLinkKeyCompleteBuilder::Create(hci::ErrorCode::SUCCESS, connection_handle));
  ASSERT_TRUE(callback_->receivedChangeConnectionLinkKeyComplete);
}

TEST_F(SecurityManagerChannelTest, recv_return_link_keys) {
  std::vector<hci::ZeroKeyAndAddress> keys;
  hci_layer_->IncomingEvent(hci::ReturnLinkKeysBuilder::Create(keys));
  ASSERT_TRUE(callback_->receivedReturnLinkKeys);
}

TEST_F(SecurityManagerChannelTest, send_link_key_request_reply) {
  // Arrange
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  auto packet = hci::LinkKeyRequestReplyBuilder::Create(device_.GetAddress(), link_key);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_link_key_request_neg_reply) {
  // Arrange
  auto packet = hci::LinkKeyRequestNegativeReplyBuilder::Create(device_.GetAddress());

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_read_stored_link_key) {
  // Arrange
  auto packet = hci::ReadStoredLinkKeyBuilder::Create(device_.GetAddress(), hci::ReadStoredLinkKeyReadAllFlag::ALL);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::READ_STORED_LINK_KEY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_write_stored_link_key) {
  // Arrange
  std::vector<hci::KeyAndAddress> keys_to_write;
  auto packet = hci::WriteStoredLinkKeyBuilder::Create(keys_to_write);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::WRITE_STORED_LINK_KEY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_delete_stored_link_key) {
  // Arrange
  auto packet =
      hci::DeleteStoredLinkKeyBuilder::Create(device_.GetAddress(), hci::DeleteStoredLinkKeyDeleteAllFlag::ALL);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::DELETE_STORED_LINK_KEY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_encryption_key_refresh) {
  uint16_t connection_handle = 0x0;
  hci_layer_->IncomingEvent(
      hci::EncryptionKeyRefreshCompleteBuilder::Create(hci::ErrorCode::SUCCESS, connection_handle));
  ASSERT_TRUE(callback_->receivedEncryptionKeyRefreshComplete);
}

TEST_F(SecurityManagerChannelTest, send_refresh_encryption_key) {
  // Arrange
  uint16_t connection_handle = 0x0;
  auto packet = hci::RefreshEncryptionKeyBuilder::Create(connection_handle);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::REFRESH_ENCRYPTION_KEY, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_read_encryption_key_size) {
  // Arrange
  uint16_t connection_handle = 0x0;
  auto packet = hci::ReadEncryptionKeySizeBuilder::Create(connection_handle);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::READ_ENCRYPTION_KEY_SIZE, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_simple_pairing_complete) {
  hci_layer_->IncomingEvent(hci::SimplePairingCompleteBuilder::Create(hci::ErrorCode::SUCCESS, device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedSimplePairingComplete);
}

TEST_F(SecurityManagerChannelTest, send_read_simple_pairing_mode) {
  // Arrange
  auto packet = hci::ReadSimplePairingModeBuilder::Create();

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::READ_SIMPLE_PAIRING_MODE, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, send_write_simple_pairing_mode) {
  // Arrange
  auto packet = hci::WriteSimplePairingModeBuilder::Create(hci::Enable::ENABLED);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::WRITE_SIMPLE_PAIRING_MODE, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_keypress_notification) {
  hci_layer_->IncomingEvent(
      hci::KeypressNotificationBuilder::Create(device_.GetAddress(), hci::KeypressNotificationType::ENTRY_COMPLETED));
  ASSERT_TRUE(callback_->receivedKeypressNotification);
}

TEST_F(SecurityManagerChannelTest, send_keypress_notification) {
  // Arrange
  auto packet =
      hci::SendKeypressNotificationBuilder::Create(device_.GetAddress(), hci::KeypressNotificationType::ENTRY_STARTED);

  // Act
  channel_->SendCommand(std::move(packet));
  auto last_command = std::move(hci_layer_->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  hci::CommandPacketView packet_view = hci::CommandPacketView::Create(command_packet);

  // Assert
  ASSERT_TRUE(packet_view.IsValid());
  ASSERT_EQ(OpCode::SEND_KEYPRESS_NOTIFICATION, packet_view.GetOpCode());
}

TEST_F(SecurityManagerChannelTest, recv_user_passkey_request) {
  hci_layer_->IncomingEvent(hci::UserPasskeyRequestBuilder::Create(device_.GetAddress()));
  ASSERT_TRUE(callback_->receivedUserPasskeyRequest);
}

}  // namespace
}  // namespace channel
}  // namespace security
}  // namespace bluetooth
