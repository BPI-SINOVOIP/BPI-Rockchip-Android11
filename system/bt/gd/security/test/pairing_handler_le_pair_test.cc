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

#include "common/testing/wired_pair_of_bidi_queues.h"
#include "hci/le_security_interface.h"
#include "packet/raw_builder.h"
#include "security/pairing_handler_le.h"
#include "security/test/mocks.h"

using namespace std::chrono_literals;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Matcher;
using testing::SaveArg;

using bluetooth::hci::Address;
using bluetooth::hci::AddressType;
using bluetooth::hci::CommandCompleteView;
using bluetooth::hci::CommandStatusView;
using bluetooth::hci::EncryptionChangeBuilder;
using bluetooth::hci::EncryptionEnabled;
using bluetooth::hci::ErrorCode;
using bluetooth::hci::EventPacketBuilder;
using bluetooth::hci::EventPacketView;
using bluetooth::hci::LeSecurityCommandBuilder;

// run:
// out/host/linux-x86/nativetest/bluetooth_test_gd/bluetooth_test_gd --gtest_filter=Pairing*
// adb shell /data/nativetest/bluetooth_test_gd/bluetooth_test_gd  --gtest_filter=PairingHandlerPairTest.*
// --gtest_repeat=10 --gtest_shuffle

namespace bluetooth {
namespace security {
CommandView CommandBuilderToView(std::unique_ptr<BasePacketBuilder> builder) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  builder->Serialize(it);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto temp_cmd_view = CommandView::Create(packet_bytes_view);
  return CommandView::Create(temp_cmd_view);
}

EventPacketView EventBuilderToView(std::unique_ptr<EventPacketBuilder> builder) {
  std::shared_ptr<std::vector<uint8_t>> packet_bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter it(*packet_bytes);
  builder->Serialize(it);
  PacketView<kLittleEndian> packet_bytes_view(packet_bytes);
  auto temp_evt_view = EventPacketView::Create(packet_bytes_view);
  return EventPacketView::Create(temp_evt_view);
}
}  // namespace security
}  // namespace bluetooth

namespace {

constexpr uint16_t CONN_HANDLE_MASTER = 0x31, CONN_HANDLE_SLAVE = 0x32;
std::unique_ptr<bluetooth::security::PairingHandlerLe> pairing_handler_a, pairing_handler_b;

}  // namespace

namespace bluetooth {
namespace security {

namespace {
Address ADDRESS_MASTER{{0x26, 0x64, 0x76, 0x86, 0xab, 0xba}};
AddressType ADDRESS_TYPE_MASTER = AddressType::RANDOM_DEVICE_ADDRESS;

Address ADDRESS_SLAVE{{0x33, 0x58, 0x24, 0x76, 0x11, 0x89}};
AddressType ADDRESS_TYPE_SLAVE = AddressType::RANDOM_DEVICE_ADDRESS;

std::optional<PairingResultOrFailure> pairing_result_master;
std::optional<PairingResultOrFailure> pairing_result_slave;

void OnPairingFinishedMaster(PairingResultOrFailure r) {
  pairing_result_master = r;
  if (std::holds_alternative<PairingResult>(r)) {
    LOG_INFO("pairing finished successfully with %s", std::get<PairingResult>(r).connection_address.ToString().c_str());
  } else {
    LOG_INFO("pairing with ... failed: %s", std::get<PairingFailure>(r).message.c_str());
  }
}

void OnPairingFinishedSlave(PairingResultOrFailure r) {
  pairing_result_slave = r;
  if (std::holds_alternative<PairingResult>(r)) {
    LOG_INFO("pairing finished successfully with %s", std::get<PairingResult>(r).connection_address.ToString().c_str());
  } else {
    LOG_INFO("pairing with ... failed: %s", std::get<PairingFailure>(r).message.c_str());
  }
}

};  // namespace

// We obtain this mutex when we start initializing the handlers, and relese it when both handlers are initialized
std::mutex handlers_initialization_guard;

class PairingHandlerPairTest : public testing::Test {
  void dequeue_callback_master() {
    auto packet_bytes_view = l2cap_->GetQueueAUpEnd()->TryDequeue();
    if (!packet_bytes_view) LOG_ERROR("Received dequeue, but no data ready...");

    auto temp_cmd_view = CommandView::Create(*packet_bytes_view);
    if (!first_command_sent) {
      first_command = std::make_unique<CommandView>(CommandView::Create(temp_cmd_view));
      first_command_sent = true;
      return;
    }

    if (!pairing_handler_a) LOG_ALWAYS_FATAL("Slave handler not initlized yet!");

    pairing_handler_a->OnCommandView(CommandView::Create(temp_cmd_view));
  }

  void dequeue_callback_slave() {
    auto packet_bytes_view = l2cap_->GetQueueBUpEnd()->TryDequeue();
    if (!packet_bytes_view) LOG_ERROR("Received dequeue, but no data ready...");

    auto temp_cmd_view = CommandView::Create(*packet_bytes_view);
    if (!first_command_sent) {
      first_command = std::make_unique<CommandView>(CommandView::Create(temp_cmd_view));
      first_command_sent = true;
      return;
    }

    if (!pairing_handler_b) LOG_ALWAYS_FATAL("Master handler not initlized yet!");

    pairing_handler_b->OnCommandView(CommandView::Create(temp_cmd_view));
  }

 protected:
  void SetUp() {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    handler_ = new os::Handler(thread_);

    l2cap_ = new common::testing::WiredPairOfL2capQueues(handler_);
    // master sends it's packet into l2cap->down_buffer_b_
    // slave sends it's packet into l2cap->down_buffer_a_
    l2cap_->GetQueueAUpEnd()->RegisterDequeue(
        handler_, common::Bind(&PairingHandlerPairTest::dequeue_callback_master, common::Unretained(this)));
    l2cap_->GetQueueBUpEnd()->RegisterDequeue(
        handler_, common::Bind(&PairingHandlerPairTest::dequeue_callback_slave, common::Unretained(this)));

    up_buffer_a_ = std::make_unique<os::EnqueueBuffer<packet::BasePacketBuilder>>(l2cap_->GetQueueAUpEnd());
    up_buffer_b_ = std::make_unique<os::EnqueueBuffer<packet::BasePacketBuilder>>(l2cap_->GetQueueBUpEnd());

    master_setup = {
        .my_role = hci::Role::MASTER,
        .my_connection_address = {ADDRESS_MASTER, ADDRESS_TYPE_MASTER},

        .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                                  .oob_data_flag = OobDataFlag::NOT_PRESENT,
                                  .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                                  .maximum_encryption_key_size = 16,
                                  .initiator_key_distribution = KeyMaskId | KeyMaskSign,
                                  .responder_key_distribution = KeyMaskId | KeyMaskSign},

        .remotely_initiated = false,
        .connection_handle = CONN_HANDLE_MASTER,
        .remote_connection_address = {ADDRESS_SLAVE, ADDRESS_TYPE_SLAVE},
        .user_interface = &master_user_interface,
        .user_interface_handler = handler_,
        .le_security_interface = &master_le_security_mock,
        .proper_l2cap_interface = up_buffer_a_.get(),
        .l2cap_handler = handler_,
        .OnPairingFinished = OnPairingFinishedMaster,
    };

    slave_setup = {
        .my_role = hci::Role::SLAVE,

        .my_connection_address = {ADDRESS_SLAVE, ADDRESS_TYPE_SLAVE},
        .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                                  .oob_data_flag = OobDataFlag::NOT_PRESENT,
                                  .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                                  .maximum_encryption_key_size = 16,
                                  .initiator_key_distribution = KeyMaskId | KeyMaskSign,
                                  .responder_key_distribution = KeyMaskId | KeyMaskSign},
        .remotely_initiated = true,
        .connection_handle = CONN_HANDLE_SLAVE,
        .remote_connection_address = {ADDRESS_MASTER, ADDRESS_TYPE_MASTER},
        .user_interface = &slave_user_interface,
        .user_interface_handler = handler_,
        .le_security_interface = &slave_le_security_mock,
        .proper_l2cap_interface = up_buffer_b_.get(),
        .l2cap_handler = handler_,
        .OnPairingFinished = OnPairingFinishedSlave,
    };

    RecordSuccessfulEncryptionComplete();
  }

  void TearDown() {
    ::testing::Mock::VerifyAndClearExpectations(&slave_user_interface);
    ::testing::Mock::VerifyAndClearExpectations(&master_user_interface);
    ::testing::Mock::VerifyAndClearExpectations(&slave_le_security_mock);
    ::testing::Mock::VerifyAndClearExpectations(&master_le_security_mock);

    pairing_handler_a.reset();
    pairing_handler_b.reset();
    pairing_result_master.reset();
    pairing_result_slave.reset();

    first_command_sent = false;
    first_command.reset();

    l2cap_->GetQueueAUpEnd()->UnregisterDequeue();
    l2cap_->GetQueueBUpEnd()->UnregisterDequeue();

    delete l2cap_;
    handler_->Clear();
    delete handler_;
    delete thread_;
  }

  void RecordPairingPromptHandling(UIMock& ui_mock, std::unique_ptr<PairingHandlerLe>* handler) {
    EXPECT_CALL(ui_mock, DisplayPairingPrompt(_, _)).Times(1).WillOnce(InvokeWithoutArgs([handler]() {
      LOG_INFO("UI mock received pairing prompt");

      {
        // By grabbing the lock, we ensure initialization of both pairing handlers is finished.
        std::lock_guard<std::mutex> lock(handlers_initialization_guard);
      }

      if (!(*handler)) LOG_ALWAYS_FATAL("handler not initalized yet!");
      // Simulate user accepting the pairing in UI
      (*handler)->OnUiAction(PairingEvent::PAIRING_ACCEPTED, 0x01 /* Non-zero value means success */);
    }));
  }

  void RecordSuccessfulEncryptionComplete() {
    // For now, all tests are succeeding to go through Encryption. Record that in the setup.
    //  Once we test failure cases, move this to each test
    EXPECT_CALL(master_le_security_mock,
                EnqueueCommand(_, Matcher<common::OnceCallback<void(CommandStatusView)>>(_), _))
        .Times(1)
        .WillOnce([](std::unique_ptr<LeSecurityCommandBuilder> command,
                     common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) {
          // TODO: on_status.Run();

          pairing_handler_a->OnHciEvent(EventBuilderToView(
              EncryptionChangeBuilder::Create(ErrorCode::SUCCESS, CONN_HANDLE_MASTER, EncryptionEnabled::ON)));

          pairing_handler_b->OnHciEvent(EventBuilderToView(
              EncryptionChangeBuilder::Create(ErrorCode::SUCCESS, CONN_HANDLE_SLAVE, EncryptionEnabled::ON)));
        });
  }

 public:
  std::unique_ptr<bluetooth::security::CommandView> WaitFirstL2capCommand() {
    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }

    return std::move(first_command);
  }

  InitialInformations master_setup;
  InitialInformations slave_setup;
  UIMock master_user_interface;
  UIMock slave_user_interface;
  LeSecurityInterfaceMock master_le_security_mock;
  LeSecurityInterfaceMock slave_le_security_mock;

  uint16_t first_command_sent = false;
  std::unique_ptr<bluetooth::security::CommandView> first_command;

  os::Thread* thread_;
  os::Handler* handler_;
  common::testing::WiredPairOfL2capQueues* l2cap_;

  std::unique_ptr<os::EnqueueBuffer<packet::BasePacketBuilder>> up_buffer_a_;
  std::unique_ptr<os::EnqueueBuffer<packet::BasePacketBuilder>> up_buffer_b_;
};

/* This test verifies that Just Works pairing flow works.
 * Both simulated devices specify capabilities as NO_INPUT_NO_OUTPUT, and secure connecitons support */
TEST_F(PairingHandlerPairTest, test_secure_connections_just_works) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::NO_INPUT_NO_OUTPUT;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  slave_setup.myPairingCapabilities.io_capability = IoCapability::NO_INPUT_NO_OUTPUT;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;

  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);

    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);

    auto first_pkt = WaitFirstL2capCommand();
    slave_setup.pairing_request = PairingRequestView::Create(*first_pkt);

    EXPECT_CALL(slave_user_interface, DisplayPairingPrompt(_, _)).Times(1).WillOnce(InvokeWithoutArgs([] {
      LOG_INFO("UI mock received pairing prompt");

      {
        // By grabbing the lock, we ensure initialization of both pairing handlers is finished.
        std::lock_guard<std::mutex> lock(handlers_initialization_guard);
      }

      if (!pairing_handler_b) LOG_ALWAYS_FATAL("handler not initalized yet!");

      // Simulate user accepting the pairing in UI
      pairing_handler_b->OnUiAction(PairingEvent::PAIRING_ACCEPTED, 0x01 /* Non-zero value means success */);
    }));

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }

  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_secure_connections_just_works_slave_initiated) {
  master_setup = {
      .my_role = hci::Role::MASTER,
      .my_connection_address = {ADDRESS_MASTER, ADDRESS_TYPE_MASTER},
      .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                                .oob_data_flag = OobDataFlag::NOT_PRESENT,
                                .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                                .maximum_encryption_key_size = 16,
                                .initiator_key_distribution = KeyMaskId | KeyMaskSign,
                                .responder_key_distribution = KeyMaskId | KeyMaskSign},
      .remotely_initiated = true,
      .connection_handle = CONN_HANDLE_MASTER,
      .remote_connection_address = {ADDRESS_SLAVE, ADDRESS_TYPE_SLAVE},
      .user_interface = &master_user_interface,
      .user_interface_handler = handler_,
      .le_security_interface = &master_le_security_mock,
      .proper_l2cap_interface = up_buffer_a_.get(),
      .l2cap_handler = handler_,
      .OnPairingFinished = OnPairingFinishedMaster,
  };

  slave_setup = {
      .my_role = hci::Role::SLAVE,
      .my_connection_address = {ADDRESS_SLAVE, ADDRESS_TYPE_SLAVE},
      .myPairingCapabilities = {.io_capability = IoCapability::NO_INPUT_NO_OUTPUT,
                                .oob_data_flag = OobDataFlag::NOT_PRESENT,
                                .auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,
                                .maximum_encryption_key_size = 16,
                                .initiator_key_distribution = KeyMaskId | KeyMaskSign,
                                .responder_key_distribution = KeyMaskId | KeyMaskSign},
      .remotely_initiated = false,
      .connection_handle = CONN_HANDLE_SLAVE,
      .remote_connection_address = {ADDRESS_MASTER, ADDRESS_TYPE_MASTER},
      .user_interface = &slave_user_interface,
      .user_interface_handler = handler_,
      .le_security_interface = &slave_le_security_mock,
      .proper_l2cap_interface = up_buffer_b_.get(),
      .l2cap_handler = handler_,
      .OnPairingFinished = OnPairingFinishedSlave,
  };

  std::unique_ptr<bluetooth::security::CommandView> first_pkt;
  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);

    first_pkt = WaitFirstL2capCommand();

    EXPECT_CALL(master_user_interface, DisplayPairingPrompt(_, _))
        .Times(1)
        .WillOnce(InvokeWithoutArgs([&first_pkt, this] {
          LOG_INFO("UI mock received pairing prompt");

          {
            // By grabbing the lock, we ensure initialization of both pairing handlers is finished.
            std::lock_guard<std::mutex> lock(handlers_initialization_guard);
          }
          if (!pairing_handler_a) LOG_ALWAYS_FATAL("handler not initalized yet!");
          // Simulate user accepting the pairing in UI
          pairing_handler_a->OnUiAction(PairingEvent::PAIRING_ACCEPTED, 0x01 /* Non-zero value means success */);

          // Send the first packet from the slave to master
          auto view_to_packet = std::make_unique<packet::RawBuilder>();
          view_to_packet->AddOctets(std::vector(first_pkt->begin(), first_pkt->end()));
          up_buffer_b_->Enqueue(std::move(view_to_packet), handler_);
        }));
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);
  }

  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_secure_connections_numeric_comparison) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::DISPLAY_YES_NO;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc;

  slave_setup.myPairingCapabilities.io_capability = IoCapability::DISPLAY_YES_NO;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc;

  uint32_t num_value_slave = 0;
  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    // Initiator must be initialized after the responder.
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);

    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    EXPECT_CALL(slave_user_interface, DisplayConfirmValue(_, _, _)).WillOnce(SaveArg<2>(&num_value_slave));
    EXPECT_CALL(master_user_interface, DisplayConfirmValue(_, _, _))
        .WillOnce(Invoke([&](const bluetooth::hci::AddressWithType&, std::string, uint32_t num_value) {
          EXPECT_EQ(num_value_slave, num_value);
          if (num_value_slave == num_value) {
            pairing_handler_a->OnUiAction(PairingEvent::CONFIRM_YESNO, 0x01);
            pairing_handler_b->OnUiAction(PairingEvent::CONFIRM_YESNO, 0x01);
          }
        }));

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_secure_connections_passkey_entry) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::KEYBOARD_ONLY;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc;

  slave_setup.myPairingCapabilities.io_capability = IoCapability::DISPLAY_ONLY;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc;

  // In this test either master or slave display the UI prompt first. This variable makes sure both prompts are
  // displayed before passkey is confirmed. Since both UI handlers are same thread, it's safe.
  int ui_prompts_count = 0;
  uint32_t passkey_ = std::numeric_limits<uint32_t>::max();
  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);

    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    EXPECT_CALL(slave_user_interface, DisplayPasskey(_, _, _))
        .WillOnce(Invoke([&](const bluetooth::hci::AddressWithType& address, std::string name, uint32_t passkey) {
          passkey_ = passkey;
          ui_prompts_count++;
          if (ui_prompts_count == 2) {
            pairing_handler_a->OnUiAction(PairingEvent::PASSKEY, passkey);
          }
        }));

    EXPECT_CALL(master_user_interface, DisplayEnterPasskeyDialog(_, _))
        .WillOnce(Invoke([&](const bluetooth::hci::AddressWithType& address, std::string name) {
          ui_prompts_count++;
          if (ui_prompts_count == 2) {
            pairing_handler_a->OnUiAction(PairingEvent::PASSKEY, passkey_);
          }
        }));

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  // Initiator must be initialized after the responder.
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_secure_connections_out_of_band) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::KEYBOARD_ONLY;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,

  slave_setup.myPairingCapabilities.io_capability = IoCapability::DISPLAY_ONLY;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,

  master_setup.my_oob_data = std::make_optional<MyOobData>(PairingHandlerLe::GenerateOobData());
  slave_setup.remote_oob_data =
      std::make_optional<InitialInformations::out_of_band_data>(InitialInformations::out_of_band_data{
          .le_sc_c = master_setup.my_oob_data->c,
          .le_sc_r = master_setup.my_oob_data->r,
      });

  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);
    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_secure_connections_out_of_band_two_way) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::KEYBOARD_ONLY;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,

  slave_setup.myPairingCapabilities.io_capability = IoCapability::DISPLAY_ONLY;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm | AuthReqMaskSc,

  master_setup.my_oob_data = std::make_optional<MyOobData>(PairingHandlerLe::GenerateOobData());
  slave_setup.remote_oob_data =
      std::make_optional<InitialInformations::out_of_band_data>(InitialInformations::out_of_band_data{
          .le_sc_c = master_setup.my_oob_data->c,
          .le_sc_r = master_setup.my_oob_data->r,
      });

  slave_setup.my_oob_data = std::make_optional<MyOobData>(PairingHandlerLe::GenerateOobData());
  master_setup.remote_oob_data =
      std::make_optional<InitialInformations::out_of_band_data>(InitialInformations::out_of_band_data{
          .le_sc_c = slave_setup.my_oob_data->c,
          .le_sc_r = slave_setup.my_oob_data->r,
      });

  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);
    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_legacy_just_works) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::NO_INPUT_NO_OUTPUT;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm,

  slave_setup.myPairingCapabilities.io_capability = IoCapability::NO_INPUT_NO_OUTPUT;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm;

  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);
    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

TEST_F(PairingHandlerPairTest, test_legacy_passkey_entry) {
  master_setup.myPairingCapabilities.io_capability = IoCapability::KEYBOARD_DISPLAY;
  master_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  master_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm,

  slave_setup.myPairingCapabilities.io_capability = IoCapability::KEYBOARD_ONLY;
  slave_setup.myPairingCapabilities.oob_data_flag = OobDataFlag::NOT_PRESENT;
  slave_setup.myPairingCapabilities.auth_req = AuthReqMaskBondingFlag | AuthReqMaskMitm;

  {
    std::unique_lock<std::mutex> lock(handlers_initialization_guard);
    pairing_handler_a = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, master_setup);
    while (!first_command_sent) {
      std::this_thread::sleep_for(1ms);
      LOG_INFO("waiting for first command...");
    }
    slave_setup.pairing_request = PairingRequestView::Create(*first_command);

    RecordPairingPromptHandling(slave_user_interface, &pairing_handler_b);

    EXPECT_CALL(slave_user_interface, DisplayEnterPasskeyDialog(_, _));
    EXPECT_CALL(master_user_interface, DisplayConfirmValue(_, _, _))
        .WillOnce(Invoke([&](const bluetooth::hci::AddressWithType&, std::string, uint32_t passkey) {
          LOG_INFO("Passkey prompt displayed entering passkey: %08x", passkey);
          std::this_thread::sleep_for(1ms);

          // TODO: handle case where prompts are displayed in different order in the test!
          pairing_handler_b->OnUiAction(PairingEvent::PASSKEY, passkey);
        }));

    pairing_handler_b = std::make_unique<PairingHandlerLe>(PairingHandlerLe::PHASE1, slave_setup);
  }
  pairing_handler_a->WaitUntilPairingFinished();
  pairing_handler_b->WaitUntilPairingFinished();

  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_master.value()));
  EXPECT_TRUE(std::holds_alternative<PairingResult>(pairing_result_slave.value()));
}

}  // namespace security
}  // namespace bluetooth
