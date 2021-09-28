/******************************************************************************
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
 ******************************************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

#include "os/log.h"
#include "security/pairing_handler_le.h"
#include "security/test/mocks.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::VariantWith;

using bluetooth::security::CommandView;

namespace bluetooth {
namespace security {

namespace {

CommandView BuilderToView(std::unique_ptr<BasePacketBuilder> builder) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  builder->Serialize(it);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto temp_cmd_view = CommandView::Create(packet_bytes_view);
  return CommandView::Create(temp_cmd_view);
}

std::condition_variable outgoing_l2cap_blocker_;
std::optional<bluetooth::security::CommandView> outgoing_l2cap_packet_;

bool WaitForOutgoingL2capPacket() {
  std::mutex mutex;
  std::unique_lock<std::mutex> lock(mutex);
  if (outgoing_l2cap_blocker_.wait_for(lock, std::chrono::seconds(5)) == std::cv_status::timeout) {
    return false;
  }
  return true;
}

class PairingResultHandlerMock {
 public:
  MOCK_CONST_METHOD1(OnPairingFinished, void(PairingResultOrFailure));
};

std::unique_ptr<PairingResultHandlerMock> pairingResult;
LeSecurityInterfaceMock leSecurityMock;
UIMock uiMock;

void OnPairingFinished(PairingResultOrFailure r) {
  if (std::holds_alternative<PairingResult>(r)) {
    LOG(INFO) << "pairing with " << std::get<PairingResult>(r).connection_address << " finished successfully!";
  } else {
    LOG(INFO) << "pairing with ... failed!";
  }
  pairingResult->OnPairingFinished(r);
}
}  // namespace

class PairingHandlerUnitTest : public testing::Test {
 protected:
  void SetUp() {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    handler_ = new os::Handler(thread_);

    bidi_queue_ =
        std::make_unique<common::BidiQueue<packet::PacketView<packet::kLittleEndian>, packet::BasePacketBuilder>>(10);
    up_buffer_ = std::make_unique<os::EnqueueBuffer<packet::BasePacketBuilder>>(bidi_queue_->GetUpEnd());

    bidi_queue_->GetDownEnd()->RegisterDequeue(
        handler_, common::Bind(&PairingHandlerUnitTest::L2CAP_SendSmp, common::Unretained(this)));

    pairingResult.reset(new PairingResultHandlerMock);
  }
  void TearDown() {
    pairingResult.reset();
    bidi_queue_->GetDownEnd()->UnregisterDequeue();
    handler_->Clear();
    delete handler_;
    delete thread_;

    ::testing::Mock::VerifyAndClearExpectations(&leSecurityMock);
    ::testing::Mock::VerifyAndClearExpectations(&uiMock);
  }

  void L2CAP_SendSmp() {
    std::unique_ptr<packet::BasePacketBuilder> builder = bidi_queue_->GetDownEnd()->TryDequeue();

    outgoing_l2cap_packet_ = BuilderToView(std::move(builder));
    outgoing_l2cap_packet_->IsValid();

    outgoing_l2cap_blocker_.notify_one();
  }

 public:
  os::Thread* thread_;
  os::Handler* handler_;
  std::unique_ptr<common::BidiQueue<packet::PacketView<packet::kLittleEndian>, packet::BasePacketBuilder>> bidi_queue_;
  std::unique_ptr<os::EnqueueBuffer<packet::BasePacketBuilder>> up_buffer_;
};

InitialInformations initial_informations{
    .my_role = hci::Role::MASTER,
    .my_connection_address = {{}, hci::AddressType::PUBLIC_DEVICE_ADDRESS},

    .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                              .oob_data_flag = OobDataFlag::NOT_PRESENT,
                              .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                              .maximum_encryption_key_size = 16,
                              .initiator_key_distribution = 0x03,
                              .responder_key_distribution = 0x03},

    .remotely_initiated = false,
    .remote_connection_address = {{}, hci::AddressType::RANDOM_DEVICE_ADDRESS},
    .user_interface = &uiMock,
    .le_security_interface = &leSecurityMock,
    .OnPairingFinished = OnPairingFinished,
};

TEST_F(PairingHandlerUnitTest, test_phase_1_failure) {
  initial_informations.proper_l2cap_interface = up_buffer_.get();
  initial_informations.l2cap_handler = handler_;
  initial_informations.user_interface_handler = handler_;

  std::unique_ptr<PairingHandlerLe> pairing_handler =
      std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, initial_informations);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(outgoing_l2cap_packet_->GetCode(), Code::PAIRING_REQUEST);

  EXPECT_CALL(*pairingResult, OnPairingFinished(VariantWith<PairingFailure>(_))).Times(1);

  // SMP will waith for Pairing Response, once bad packet is received, it should stop the Pairing
  CommandView bad_pairing_response = BuilderToView(PairingRandomBuilder::Create({}));
  bad_pairing_response.IsValid();
  pairing_handler->OnCommandView(bad_pairing_response);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(outgoing_l2cap_packet_->GetCode(), Code::PAIRING_FAILED);
}

TEST_F(PairingHandlerUnitTest, test_secure_connections_just_works) {
  initial_informations.proper_l2cap_interface = up_buffer_.get();
  initial_informations.l2cap_handler = handler_;
  initial_informations.user_interface_handler = handler_;

  // we keep the pairing_handler as unique_ptr to better mimick how it's used
  // in the real world
  std::unique_ptr<PairingHandlerLe> pairing_handler =
      std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, initial_informations);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(outgoing_l2cap_packet_->GetCode(), Code::PAIRING_REQUEST);
  CommandView pairing_request = outgoing_l2cap_packet_.value();

  auto pairing_response = BuilderToView(
      PairingResponseBuilder::Create(IoCapability::KEYBOARD_DISPLAY, OobDataFlag::NOT_PRESENT,
                                     AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc, 16, 0x03, 0x03));
  pairing_handler->OnCommandView(pairing_response);
  // Phase 1 finished.

  // pairing public key
  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(Code::PAIRING_PUBLIC_KEY, outgoing_l2cap_packet_->GetCode());
  EcdhPublicKey my_public_key;
  auto ppkv = PairingPublicKeyView::Create(outgoing_l2cap_packet_.value());
  ppkv.IsValid();
  my_public_key.x = ppkv.GetPublicKeyX();
  my_public_key.y = ppkv.GetPublicKeyY();

  const auto [private_key, public_key] = GenerateECDHKeyPair();

  pairing_handler->OnCommandView(BuilderToView(PairingPublicKeyBuilder::Create(public_key.x, public_key.y)));
  // DHKey exchange finished
  std::array<uint8_t, 32> dhkey = ComputeDHKey(private_key, my_public_key);

  // Phasae 2 Stage 1 start
  Octet16 ra, rb;
  ra = rb = {0};

  Octet16 Nb = PairingHandlerLe::GenerateRandom<16>();

  // Compute confirm
  Octet16 Cb = crypto_toolbox::f4((uint8_t*)public_key.x.data(), (uint8_t*)my_public_key.x.data(), Nb, 0);

  pairing_handler->OnCommandView(BuilderToView(PairingConfirmBuilder::Create(Cb)));

  // random
  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(Code::PAIRING_RANDOM, outgoing_l2cap_packet_->GetCode());
  auto prv = PairingRandomView::Create(outgoing_l2cap_packet_.value());
  prv.IsValid();
  Octet16 Na = prv.GetRandomValue();

  pairing_handler->OnCommandView(BuilderToView(PairingRandomBuilder::Create(Nb)));

  // Start of authentication stage 2
  uint8_t a[7];
  uint8_t b[7];
  memcpy(b, initial_informations.remote_connection_address.GetAddress().address, 6);
  b[6] = (uint8_t)initial_informations.remote_connection_address.GetAddressType();
  memcpy(a, initial_informations.my_connection_address.GetAddress().address, 6);
  a[6] = (uint8_t)initial_informations.my_connection_address.GetAddressType();

  Octet16 ltk, mac_key;
  crypto_toolbox::f5(dhkey.data(), Na, Nb, a, b, &mac_key, &ltk);

  PairingRequestView preqv = PairingRequestView::Create(pairing_request);
  PairingResponseView prspv = PairingResponseView::Create(pairing_response);

  preqv.IsValid();
  prspv.IsValid();
  std::array<uint8_t, 3> iocapA{static_cast<uint8_t>(preqv.GetIoCapability()),
                                static_cast<uint8_t>(preqv.GetOobDataFlag()), preqv.GetAuthReq()};
  std::array<uint8_t, 3> iocapB{static_cast<uint8_t>(prspv.GetIoCapability()),
                                static_cast<uint8_t>(prspv.GetOobDataFlag()), prspv.GetAuthReq()};

  Octet16 Ea = crypto_toolbox::f6(mac_key, Na, Nb, rb, iocapA.data(), a, b);
  Octet16 Eb = crypto_toolbox::f6(mac_key, Nb, Na, ra, iocapB.data(), b, a);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(Code::PAIRING_DH_KEY_CHECK, outgoing_l2cap_packet_->GetCode());
  auto pdhkcv = PairingDhKeyCheckView::Create(outgoing_l2cap_packet_.value());
  pdhkcv.IsValid();
  EXPECT_EQ(pdhkcv.GetDhKeyCheck(), Ea);

  pairing_handler->OnCommandView(BuilderToView(PairingDhKeyCheckBuilder::Create(Eb)));

  // Phase 2 finished
  // We don't care for the rest of the flow, let it die.
}

InitialInformations initial_informations_trsi{
    .my_role = hci::Role::MASTER,
    .my_connection_address = hci::AddressWithType(),

    .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                              .oob_data_flag = OobDataFlag::NOT_PRESENT,
                              .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                              .maximum_encryption_key_size = 16,
                              .initiator_key_distribution = 0x03,
                              .responder_key_distribution = 0x03},

    .remotely_initiated = true,
    .remote_connection_address = hci::AddressWithType(),
    .user_interface = &uiMock,
    .le_security_interface = &leSecurityMock,
    .OnPairingFinished = OnPairingFinished,
};

/* This test verifies that when remote slave device sends security request , and user
 * does accept the prompt, we do send pairing request */
TEST_F(PairingHandlerUnitTest, test_remote_slave_initiating) {
  initial_informations_trsi.proper_l2cap_interface = up_buffer_.get();
  initial_informations_trsi.l2cap_handler = handler_;
  initial_informations_trsi.user_interface_handler = handler_;

  std::unique_ptr<PairingHandlerLe> pairing_handler =
      std::make_unique<PairingHandlerLe>(PairingHandlerLe::ACCEPT_PROMPT, initial_informations_trsi);

  // Simulate user accepting the pairing in UI
  pairing_handler->OnUiAction(PairingEvent::PAIRING_ACCEPTED, 0x01 /* Non-zero value means success */);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(Code::PAIRING_REQUEST, outgoing_l2cap_packet_->GetCode());

  // We don't care for the rest of the flow, let it die.
  pairing_handler.reset();
}

InitialInformations initial_informations_trmi{
    .my_role = hci::Role::SLAVE,
    .my_connection_address = hci::AddressWithType(),

    .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                              .oob_data_flag = OobDataFlag::NOT_PRESENT,
                              .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                              .maximum_encryption_key_size = 16,
                              .initiator_key_distribution = 0x03,
                              .responder_key_distribution = 0x03},

    .remotely_initiated = true,
    .remote_connection_address = hci::AddressWithType(),
    .pairing_request = PairingRequestView::Create(BuilderToView(
        PairingRequestBuilder::Create(IoCapability::NO_INPUT_NO_OUTPUT, OobDataFlag::NOT_PRESENT,
                                      AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc, 16, 0x03, 0x03))),
    .user_interface = &uiMock,
    .le_security_interface = &leSecurityMock,

    .OnPairingFinished = OnPairingFinished,
};

/* This test verifies that when remote device sends pairing request, and user does accept the prompt, we do send proper
 * reply back */
TEST_F(PairingHandlerUnitTest, test_remote_master_initiating) {
  initial_informations_trmi.proper_l2cap_interface = up_buffer_.get();
  initial_informations_trmi.l2cap_handler = handler_;
  initial_informations_trmi.user_interface_handler = handler_;

  std::unique_ptr<PairingHandlerLe> pairing_handler =
      std::make_unique<PairingHandlerLe>(PairingHandlerLe::ACCEPT_PROMPT, initial_informations_trmi);

  // Simulate user accepting the pairing in UI
  pairing_handler->OnUiAction(PairingEvent::PAIRING_ACCEPTED, 0x01 /* Non-zero value means success */);

  EXPECT_TRUE(WaitForOutgoingL2capPacket());
  EXPECT_EQ(Code::PAIRING_RESPONSE, outgoing_l2cap_packet_->GetCode());
  // Phase 1 finished.

  // We don't care for the rest of the flow, it's handled in in other tests. let it die.
  pairing_handler.reset();
}

}  // namespace security
}  // namespace bluetooth
