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
#include "security/pairing/classic_pairing_handler.h"

#include <gtest/gtest.h>
#include <memory>

#include "hci/hci_packets.h"
#include "l2cap/classic/fixed_channel_manager_mock.h"
#include "packet/raw_builder.h"
#include "security/channel/security_manager_channel.h"
#include "security/initial_informations.h"
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
  explicit SecurityManagerChannelCallback(pairing::ClassicPairingHandler* pairing_handler)
      : pairing_handler_(pairing_handler) {}
  void OnHciEventReceived(hci::EventPacketView packet) override {
    auto event = hci::EventPacketView::Create(packet);
    ASSERT_LOG(event.IsValid(), "Received invalid packet");
    const hci::EventCode code = event.GetEventCode();
    switch (code) {
      case hci::EventCode::PIN_CODE_REQUEST:
        pairing_handler_->OnReceive(hci::PinCodeRequestView::Create(event));
        break;
      case hci::EventCode::LINK_KEY_REQUEST:
        pairing_handler_->OnReceive(hci::LinkKeyRequestView::Create(event));
        break;
      case hci::EventCode::LINK_KEY_NOTIFICATION:
        pairing_handler_->OnReceive(hci::LinkKeyNotificationView::Create(event));
        break;
      case hci::EventCode::IO_CAPABILITY_REQUEST:
        pairing_handler_->OnReceive(hci::IoCapabilityRequestView::Create(event));
        break;
      case hci::EventCode::IO_CAPABILITY_RESPONSE:
        pairing_handler_->OnReceive(hci::IoCapabilityResponseView::Create(event));
        break;
      case hci::EventCode::SIMPLE_PAIRING_COMPLETE:
        pairing_handler_->OnReceive(hci::SimplePairingCompleteView::Create(event));
        break;
      case hci::EventCode::RETURN_LINK_KEYS:
        pairing_handler_->OnReceive(hci::ReturnLinkKeysView::Create(event));
        break;
      case hci::EventCode::REMOTE_OOB_DATA_REQUEST:
        pairing_handler_->OnReceive(hci::RemoteOobDataRequestView::Create(event));
        break;
      case hci::EventCode::USER_PASSKEY_NOTIFICATION:
        pairing_handler_->OnReceive(hci::UserPasskeyNotificationView::Create(event));
        break;
      case hci::EventCode::KEYPRESS_NOTIFICATION:
        pairing_handler_->OnReceive(hci::KeypressNotificationView::Create(event));
        break;
      case hci::EventCode::USER_CONFIRMATION_REQUEST:
        pairing_handler_->OnReceive(hci::UserConfirmationRequestView::Create(event));
        break;
      case hci::EventCode::USER_PASSKEY_REQUEST:
        pairing_handler_->OnReceive(hci::UserPasskeyRequestView::Create(event));
        break;
      default:
        ASSERT_LOG(false, "Cannot handle received packet: %s", hci::EventCodeText(code).c_str());
        break;
    }
  }

 private:
  pairing::ClassicPairingHandler* pairing_handler_ = nullptr;
};

static void pairing_complete_callback(bluetooth::hci::Address address, PairingResultOrFailure status) {
  ASSERT(std::holds_alternative<PairingResult>(status));
  // auto result = std::get<PairingResult>(status);
  //  if (std::holds_alternative<PairingResult>(status)) {
  //    auto result = status::get<PairingResult>(status);
  //  }
  //  if (std::holds_alternative<PairingFailure>(status)) {
  //    auto failure = status::get<PairingFailure>(status);
  //  }
}

class ClassicPairingHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    handler_ = new Handler(&thread_);
    hci_layer_ = new FakeHciLayer();
    channel_ = new channel::SecurityManagerChannel(handler_, hci_layer_);
    fake_registry_.InjectTestModule(&FakeHciLayer::Factory, hci_layer_);
    fake_registry_.Start<FakeHciLayer>(&thread_);
    security_record_ = std::make_shared<record::SecurityRecord>(device_);
    std::shared_ptr<l2cap::classic::testing::MockFixedChannelManager> sptr =
        std::shared_ptr<l2cap::classic::testing::MockFixedChannelManager>(
            new l2cap::classic::testing::MockFixedChannelManager());
    EXPECT_CALL(*sptr, RegisterService(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber());
    pairing_handler_ = new pairing::ClassicPairingHandler(sptr, channel_, security_record_, handler_,
                                                          common::Bind(&pairing_complete_callback), user_interface_,
                                                          user_interface_handler_, "Fake name");
    channel_callback_ = new SecurityManagerChannelCallback(pairing_handler_);
    channel_->SetChannelListener(channel_callback_);
  }

  void TearDown() override {
    channel_->SetChannelListener(nullptr);
    handler_->Clear();
    fake_registry_.SynchronizeModuleHandler(&FakeHciLayer::Factory, std::chrono::milliseconds(20));
    fake_registry_.StopAll();
    delete pairing_handler_;
    delete handler_;
    delete channel_;
    delete channel_callback_;
  }

  TestModuleRegistry fake_registry_;
  Thread& thread_ = fake_registry_.GetTestThread();
  Handler* handler_ = nullptr;
  FakeHciLayer* hci_layer_ = nullptr;
  hci::AddressWithType device_;
  SecurityManagerChannelCallback* channel_callback_ = nullptr;
  channel::SecurityManagerChannel* channel_ = nullptr;
  pairing::ClassicPairingHandler* pairing_handler_ = nullptr;
  std::shared_ptr<record::SecurityRecord> security_record_ = nullptr;
  UI* user_interface_;
  os::Handler* user_interface_handler_;
};

// Security Manager Boot Sequence (Required for SSP, these are already set at boot time)
//  - WriteSimplePairingMode
//  - WriteSecureConnectionsHostSupport
//  - WriteAuthenticatedPayloadTimeout

/*** Locally initiated ***/
// Security Pairing Sequence (JustWorks)
//  -> *Establish L2CAP connection*
//  -> AuthenticationRequested (L2CAP handles this)
//  <- LinkKeyRequest   // This is entry point for remote initiated
//  -> LinkKeyRequestNegativeReply
//  <- IoCapabilityRequest
//  -> IoCapabilityRequestReply
//  <- IoCapabilityResponse
//  <- UserConfirmationRequest
//  -> UserConfirmationRequestReply (auto)
//  <- SimplePairingComplete
//  <- LinkKeyNotification
//  <- AuthenticationComplete
//  -> SetConnectionEncryption
//  <- EncryptionChange
//  -> L2capConnectionResponse (if triggered by L2cap connection request)

void ReceiveLinkKeyRequest(FakeHciLayer* hci_layer, hci::AddressWithType device) {
  hci_layer->IncomingEvent(hci::LinkKeyRequestBuilder::Create(device.GetAddress()));
}

void ReceiveIoCapabilityRequest(FakeHciLayer* hci_layer, hci::AddressWithType device) {
  hci_layer->IncomingEvent(hci::IoCapabilityRequestBuilder::Create(device.GetAddress()));
}

void ReceiveIoCapabilityResponse(FakeHciLayer* hci_layer, hci::AddressWithType device, hci::IoCapability io_cap,
                                 hci::OobDataPresent oob_present, hci::AuthenticationRequirements auth_reqs) {
  hci_layer->IncomingEvent(
      hci::IoCapabilityResponseBuilder::Create(device.GetAddress(), io_cap, oob_present, auth_reqs));
}

void ReceiveUserConfirmationRequest(FakeHciLayer* hci_layer, hci::AddressWithType device, uint32_t numeric_value) {
  hci_layer->IncomingEvent(hci::UserConfirmationRequestBuilder::Create(device.GetAddress(), numeric_value));
}

void ReceiveSimplePairingComplete(FakeHciLayer* hci_layer, hci::ErrorCode status, hci::AddressWithType device) {
  hci_layer->IncomingEvent(hci::SimplePairingCompleteBuilder::Create(status, device.GetAddress()));
}

void ReceiveLinkKeyNotification(FakeHciLayer* hci_layer, hci::AddressWithType device, std::array<uint8_t, 16> link_key,
                                hci::KeyType key_type) {
  hci_layer->IncomingEvent(hci::LinkKeyNotificationBuilder::Create(device.GetAddress(), link_key, key_type));
}

hci::SecurityCommandView GetLastCommand(FakeHciLayer* hci_layer) {
  auto last_command = std::move(hci_layer->GetLastCommand()->command);
  auto command_packet = GetPacketView(std::move(last_command));
  auto command_packet_view = hci::CommandPacketView::Create(command_packet);
  ASSERT(command_packet_view.IsValid());
  auto security_command_view = hci::SecurityCommandView::Create(command_packet_view);
  ASSERT(security_command_view.IsValid());
  return security_command_view;
}

TEST_F(ClassicPairingHandlerTest, setup_teardown) {}

/*** JustWorks (Numeric Comparison w/ no UI) ***/
// display_only + display_only is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_display_only_display_only_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::DISPLAY_ONLY;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::DISPLAY_ONLY, hci::OobDataPresent::NOT_PRESENT,
                              hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// display_only + display_yes_no is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_display_only_display_yes_no_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::DISPLAY_ONLY;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::DISPLAY_YES_NO, hci::OobDataPresent::NOT_PRESENT,
                              hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// display_only + no_input_no_output is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_display_only_no_input_no_output_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::DISPLAY_ONLY;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::NO_INPUT_NO_OUTPUT,
                              hci::OobDataPresent::NOT_PRESENT, hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// keyboard_only + no_input_no_output is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_keyboard_only_no_input_no_output_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::KEYBOARD_ONLY;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::NO_INPUT_NO_OUTPUT,
                              hci::OobDataPresent::NOT_PRESENT, hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// no_input_no_output + display_only is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_no_input_no_output_display_only_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::NO_INPUT_NO_OUTPUT;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::DISPLAY_ONLY, hci::OobDataPresent::NOT_PRESENT,
                              hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// no_input_no_output + display_yes_no is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_no_input_no_output_display_yes_no_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::NO_INPUT_NO_OUTPUT;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::DISPLAY_YES_NO, hci::OobDataPresent::NOT_PRESENT,
                              hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// no_input_no_output + keyboard_only is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_no_input_no_output_keyboard_only_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::NO_INPUT_NO_OUTPUT;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::KEYBOARD_ONLY, hci::OobDataPresent::NOT_PRESENT,
                              hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

// no_input_no_output + no_input_no_output is JustWorks no confirmation
// Needs dialog as per security a bug unless pairing is temporary
TEST_F(ClassicPairingHandlerTest, locally_initiatied_no_input_no_output_no_input_no_output_temp) {
  hci::IoCapability injected_io_capability = hci::IoCapability::NO_INPUT_NO_OUTPUT;
  hci::AuthenticationRequirements injected_authentication_requirements = hci::AuthenticationRequirements::NO_BONDING;
  pairing_handler_->Initiate(true, injected_io_capability, hci::OobDataPresent::NOT_PRESENT,
                             injected_authentication_requirements);
  ReceiveLinkKeyRequest(hci_layer_, device_);
  auto security_command_view = GetLastCommand(hci_layer_);
  auto link_key_neg_reply = hci::LinkKeyRequestNegativeReplyView::Create(security_command_view);
  ASSERT_TRUE(link_key_neg_reply.IsValid());
  ASSERT_EQ(OpCode::LINK_KEY_REQUEST_NEGATIVE_REPLY, link_key_neg_reply.GetOpCode());
  ReceiveIoCapabilityRequest(hci_layer_, device_);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::IO_CAPABILITY_REQUEST_REPLY, security_command_view.GetOpCode());
  auto io_cap_request_reply = hci::IoCapabilityRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(io_cap_request_reply.IsValid());
  ASSERT_EQ(injected_io_capability, io_cap_request_reply.GetIoCapability());
  ASSERT_EQ(hci::OobDataPresent::NOT_PRESENT, io_cap_request_reply.GetOobPresent());
  ASSERT_EQ(injected_authentication_requirements, io_cap_request_reply.GetAuthenticationRequirements());
  ReceiveIoCapabilityResponse(hci_layer_, device_, hci::IoCapability::NO_INPUT_NO_OUTPUT,
                              hci::OobDataPresent::NOT_PRESENT, hci::AuthenticationRequirements::NO_BONDING);
  uint32_t numeric_value = 0x123;
  ReceiveUserConfirmationRequest(hci_layer_, device_, numeric_value);
  security_command_view = GetLastCommand(hci_layer_);
  ASSERT_EQ(OpCode::USER_CONFIRMATION_REQUEST_REPLY, security_command_view.GetOpCode());
  auto user_conf_request_reply = hci::UserConfirmationRequestReplyView::Create(security_command_view);
  ASSERT_TRUE(user_conf_request_reply.IsValid());
  ReceiveSimplePairingComplete(hci_layer_, hci::ErrorCode::SUCCESS, device_);
  std::array<uint8_t, 16> link_key = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5};
  hci::KeyType key_type = hci::KeyType::DEBUG_COMBINATION;
  ReceiveLinkKeyNotification(hci_layer_, device_, link_key, key_type);
  ASSERT_EQ(link_key, security_record_->GetLinkKey());
  ASSERT_EQ(key_type, security_record_->GetKeyType());
}

/*** Numeric Comparison ***/
// display_yes_no + display_only

// display_yes_no + display_yes_no
// display_yes_no + keyboard_only
// display_yes_no + no_input_no_output

// keyboard_only + display_only
// keyboard_only + display_yes_no

// keyboard_only + keyboard_only  (a just works I missed)

// Remotely initiated

// Collisions

}  // namespace
}  // namespace channel
}  // namespace security
}  // namespace bluetooth
