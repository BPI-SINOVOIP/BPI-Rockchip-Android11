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

#include "hci/hci_layer.h"

#include <gtest/gtest.h>
#include <list>
#include <memory>

#include "hal/hci_hal.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "os/log.h"
#include "os/thread.h"
#include "packet/bit_inserter.h"
#include "packet/raw_builder.h"

using bluetooth::os::Thread;
using bluetooth::packet::BitInserter;
using bluetooth::packet::RawBuilder;
using std::vector;

namespace {
vector<uint8_t> information_request = {
    0xfe, 0x2e, 0x0a, 0x00, 0x06, 0x00, 0x01, 0x00, 0x0a, 0x02, 0x02, 0x00, 0x02, 0x00,
};
// 0x00, 0x01, 0x02, 0x03, ...
vector<uint8_t> counting_bytes;
// 0xFF, 0xFE, 0xFD, 0xFC, ...
vector<uint8_t> counting_down_bytes;
const size_t count_size = 0x8;

}  // namespace

namespace bluetooth {
namespace hci {

constexpr std::chrono::milliseconds kTimeout = HciLayer::kHciTimeoutMs / 2;
constexpr std::chrono::milliseconds kAclTimeout = std::chrono::milliseconds(1000);

class TestHciHal : public hal::HciHal {
 public:
  TestHciHal() : hal::HciHal() {}

  ~TestHciHal() {
    ASSERT_LOG(callbacks == nullptr, "unregisterIncomingPacketCallback() must be called");
  }

  void registerIncomingPacketCallback(hal::HciHalCallbacks* callback) override {
    callbacks = callback;
  }

  void unregisterIncomingPacketCallback() override {
    callbacks = nullptr;
  }

  void sendHciCommand(hal::HciPacket command) override {
    outgoing_commands_.push_back(std::move(command));
    if (sent_command_promise_ != nullptr) {
      auto promise = std::move(sent_command_promise_);
      sent_command_promise_.reset();
      promise->set_value();
    }
  }

  void sendAclData(hal::HciPacket data) override {
    outgoing_acl_.push_front(std::move(data));
    if (sent_acl_promise_ != nullptr) {
      auto promise = std::move(sent_acl_promise_);
      sent_acl_promise_.reset();
      promise->set_value();
    }
  }

  void sendScoData(hal::HciPacket data) override {
    outgoing_sco_.push_front(std::move(data));
  }

  hal::HciHalCallbacks* callbacks = nullptr;

  PacketView<kLittleEndian> GetPacketView(hal::HciPacket data) {
    auto shared = std::make_shared<std::vector<uint8_t>>(data);
    return PacketView<kLittleEndian>(shared);
  }

  size_t GetNumSentCommands() {
    return outgoing_commands_.size();
  }

  std::future<void> GetSentCommandFuture() {
    ASSERT_LOG(sent_command_promise_ == nullptr, "Promises promises ... Only one at a time");
    sent_command_promise_ = std::make_unique<std::promise<void>>();
    return sent_command_promise_->get_future();
  }

  CommandPacketView GetSentCommand() {
    auto packetview = GetPacketView(std::move(outgoing_commands_.front()));
    outgoing_commands_.pop_front();
    return CommandPacketView::Create(packetview);
  }

  std::future<void> GetSentAclFuture() {
    ASSERT_LOG(sent_acl_promise_ == nullptr, "Promises promises ... Only one at a time");
    sent_acl_promise_ = std::make_unique<std::promise<void>>();
    return sent_acl_promise_->get_future();
  }

  PacketView<kLittleEndian> GetSentAcl() {
    auto packetview = GetPacketView(std::move(outgoing_acl_.front()));
    outgoing_acl_.pop_front();
    return packetview;
  }

  void Start() {}

  void Stop() {}

  void ListDependencies(ModuleList*) {}

  static const ModuleFactory Factory;

 private:
  std::list<hal::HciPacket> outgoing_commands_;
  std::list<hal::HciPacket> outgoing_acl_;
  std::list<hal::HciPacket> outgoing_sco_;
  std::unique_ptr<std::promise<void>> sent_command_promise_;
  std::unique_ptr<std::promise<void>> sent_acl_promise_;
};

const ModuleFactory TestHciHal::Factory = ModuleFactory([]() { return new TestHciHal(); });

class DependsOnHci : public Module {
 public:
  DependsOnHci() : Module() {}

  void SendHciCommandExpectingStatus(std::unique_ptr<CommandPacketBuilder> command) {
    hci_->EnqueueCommand(std::move(command),
                         common::Bind(&DependsOnHci::handle_event<CommandStatusView>, common::Unretained(this)),
                         GetHandler());
  }

  void SendHciCommandExpectingComplete(std::unique_ptr<CommandPacketBuilder> command) {
    hci_->EnqueueCommand(std::move(command),
                         common::Bind(&DependsOnHci::handle_event<CommandCompleteView>, common::Unretained(this)),
                         GetHandler());
  }

  void SendSecurityCommandExpectingComplete(std::unique_ptr<SecurityCommandBuilder> command) {
    if (security_interface_ == nullptr) {
      security_interface_ = hci_->GetSecurityInterface(
          common::Bind(&DependsOnHci::handle_event<EventPacketView>, common::Unretained(this)), GetHandler());
    }
    hci_->EnqueueCommand(std::move(command),
                         common::Bind(&DependsOnHci::handle_event<CommandCompleteView>, common::Unretained(this)),
                         GetHandler());
  }

  void SendLeSecurityCommandExpectingComplete(std::unique_ptr<LeSecurityCommandBuilder> command) {
    if (le_security_interface_ == nullptr) {
      le_security_interface_ = hci_->GetLeSecurityInterface(
          common::Bind(&DependsOnHci::handle_event<LeMetaEventView>, common::Unretained(this)), GetHandler());
    }
    hci_->EnqueueCommand(std::move(command),
                         common::Bind(&DependsOnHci::handle_event<CommandCompleteView>, common::Unretained(this)),
                         GetHandler());
  }

  void SendAclData(std::unique_ptr<AclPacketBuilder> acl) {
    outgoing_acl_.push(std::move(acl));
    auto queue_end = hci_->GetAclQueueEnd();
    queue_end->RegisterEnqueue(GetHandler(), common::Bind(&DependsOnHci::handle_enqueue, common::Unretained(this)));
  }

  std::future<void> GetReceivedEventFuture() {
    ASSERT_LOG(event_promise_ == nullptr, "Promises promises ... Only one at a time");
    event_promise_ = std::make_unique<std::promise<void>>();
    return event_promise_->get_future();
  }

  EventPacketView GetReceivedEvent() {
    EventPacketView packetview = incoming_events_.front();
    incoming_events_.pop_front();
    return packetview;
  }

  std::future<void> GetReceivedAclFuture() {
    ASSERT_LOG(acl_promise_ == nullptr, "Promises promises ... Only one at a time");
    acl_promise_ = std::make_unique<std::promise<void>>();
    return acl_promise_->get_future();
  }

  size_t GetNumReceivedAclPackets() {
    return incoming_acl_packets_.size();
  }

  AclPacketView GetReceivedAcl() {
    AclPacketView packetview = incoming_acl_packets_.front();
    incoming_acl_packets_.pop_front();
    return packetview;
  }

  void Start() {
    hci_ = GetDependency<HciLayer>();
    hci_->RegisterEventHandler(EventCode::CONNECTION_COMPLETE,
                               common::Bind(&DependsOnHci::handle_event<EventPacketView>, common::Unretained(this)),
                               GetHandler());
    hci_->RegisterLeEventHandler(SubeventCode::CONNECTION_COMPLETE,
                                 common::Bind(&DependsOnHci::handle_event<LeMetaEventView>, common::Unretained(this)),
                                 GetHandler());
    hci_->GetAclQueueEnd()->RegisterDequeue(GetHandler(),
                                            common::Bind(&DependsOnHci::handle_acl, common::Unretained(this)));
  }

  void Stop() {
    hci_->GetAclQueueEnd()->UnregisterDequeue();
  }

  void ListDependencies(ModuleList* list) {
    list->add<HciLayer>();
  }

  static const ModuleFactory Factory;

 private:
  HciLayer* hci_ = nullptr;
  const SecurityInterface* security_interface_;
  const LeSecurityInterface* le_security_interface_;
  std::list<EventPacketView> incoming_events_;
  std::list<AclPacketView> incoming_acl_packets_;
  std::unique_ptr<std::promise<void>> event_promise_;
  std::unique_ptr<std::promise<void>> acl_promise_;

  void handle_acl() {
    auto acl_ptr = hci_->GetAclQueueEnd()->TryDequeue();
    incoming_acl_packets_.push_back(*acl_ptr);
    if (acl_promise_ != nullptr) {
      auto promise = std::move(acl_promise_);
      acl_promise_.reset();
      promise->set_value();
    }
  }

  template <typename T>
  void handle_event(T event) {
    incoming_events_.push_back(event);
    if (event_promise_ != nullptr) {
      auto promise = std::move(event_promise_);
      event_promise_.reset();
      promise->set_value();
    }
  }

  std::queue<std::unique_ptr<AclPacketBuilder>> outgoing_acl_;

  std::unique_ptr<AclPacketBuilder> handle_enqueue() {
    hci_->GetAclQueueEnd()->UnregisterEnqueue();
    auto acl = std::move(outgoing_acl_.front());
    outgoing_acl_.pop();
    return acl;
  }
};

const ModuleFactory DependsOnHci::Factory = ModuleFactory([]() { return new DependsOnHci(); });

class HciTest : public ::testing::Test {
 public:
  void SetUp() override {
    counting_bytes.reserve(count_size);
    counting_down_bytes.reserve(count_size);
    for (size_t i = 0; i < count_size; i++) {
      counting_bytes.push_back(i);
      counting_down_bytes.push_back(~i);
    }
    hal = new TestHciHal();

    auto command_future = hal->GetSentCommandFuture();

    fake_registry_.InjectTestModule(&hal::HciHal::Factory, hal);
    fake_registry_.Start<DependsOnHci>(&fake_registry_.GetTestThread());
    hci = static_cast<HciLayer*>(fake_registry_.GetModuleUnderTest(&HciLayer::Factory));
    upper = static_cast<DependsOnHci*>(fake_registry_.GetModuleUnderTest(&DependsOnHci::Factory));
    ASSERT(fake_registry_.IsStarted<HciLayer>());

    auto reset_sent_status = command_future.wait_for(kTimeout);
    ASSERT_EQ(reset_sent_status, std::future_status::ready);

    // Verify that reset was received
    ASSERT_EQ(1, hal->GetNumSentCommands());

    auto sent_command = hal->GetSentCommand();
    auto reset_view = ResetView::Create(CommandPacketView::Create(sent_command));
    ASSERT_TRUE(reset_view.IsValid());

    // Verify that only one was sent
    ASSERT_EQ(0, hal->GetNumSentCommands());

    // Send the response event
    uint8_t num_packets = 1;
    ErrorCode error_code = ErrorCode::SUCCESS;
    hal->callbacks->hciEventReceived(GetPacketBytes(ResetCompleteBuilder::Create(num_packets, error_code)));
  }

  void TearDown() override {
    fake_registry_.StopAll();
  }

  std::vector<uint8_t> GetPacketBytes(std::unique_ptr<packet::BasePacketBuilder> packet) {
    std::vector<uint8_t> bytes;
    BitInserter i(bytes);
    bytes.reserve(packet->size());
    packet->Serialize(i);
    return bytes;
  }

  DependsOnHci* upper = nullptr;
  TestHciHal* hal = nullptr;
  HciLayer* hci = nullptr;
  TestModuleRegistry fake_registry_;
};

TEST_F(HciTest, initAndClose) {}

TEST_F(HciTest, leMetaEvent) {
  auto event_future = upper->GetReceivedEventFuture();

  // Send an LE event
  ErrorCode status = ErrorCode::SUCCESS;
  uint16_t handle = 0x123;
  Role role = Role::MASTER;
  AddressType peer_address_type = AddressType::PUBLIC_DEVICE_ADDRESS;
  Address peer_address = Address::kAny;
  uint16_t conn_interval = 0x0ABC;
  uint16_t conn_latency = 0x0123;
  uint16_t supervision_timeout = 0x0B05;
  MasterClockAccuracy master_clock_accuracy = MasterClockAccuracy::PPM_50;
  hal->callbacks->hciEventReceived(GetPacketBytes(
      LeConnectionCompleteBuilder::Create(status, handle, role, peer_address_type, peer_address, conn_interval,
                                          conn_latency, supervision_timeout, master_clock_accuracy)));

  // Wait for the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);

  auto event = upper->GetReceivedEvent();
  ASSERT(LeConnectionCompleteView::Create(LeMetaEventView::Create(EventPacketView::Create(event))).IsValid());
}

TEST_F(HciTest, noOpCredits) {
  ASSERT_EQ(0, hal->GetNumSentCommands());

  // Send 0 credits
  uint8_t num_packets = 0;
  hal->callbacks->hciEventReceived(GetPacketBytes(NoCommandCompleteBuilder::Create(num_packets)));

  auto command_future = hal->GetSentCommandFuture();
  upper->SendHciCommandExpectingComplete(ReadLocalVersionInformationBuilder::Create());

  // Verify that nothing was sent
  ASSERT_EQ(0, hal->GetNumSentCommands());

  num_packets = 1;
  hal->callbacks->hciEventReceived(GetPacketBytes(NoCommandCompleteBuilder::Create(num_packets)));

  auto command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);

  // Verify that one was sent
  ASSERT_EQ(1, hal->GetNumSentCommands());

  auto event_future = upper->GetReceivedEventFuture();

  // Send the response event
  ErrorCode error_code = ErrorCode::SUCCESS;
  LocalVersionInformation local_version_information;
  local_version_information.hci_version_ = HciVersion::V_5_0;
  local_version_information.hci_revision_ = 0x1234;
  local_version_information.lmp_version_ = LmpVersion::V_4_2;
  local_version_information.manufacturer_name_ = 0xBAD;
  local_version_information.lmp_subversion_ = 0x5678;
  hal->callbacks->hciEventReceived(GetPacketBytes(
      ReadLocalVersionInformationCompleteBuilder::Create(num_packets, error_code, local_version_information)));

  // Wait for the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);

  auto event = upper->GetReceivedEvent();
  ASSERT(ReadLocalVersionInformationCompleteView::Create(CommandCompleteView::Create(EventPacketView::Create(event)))
             .IsValid());
}

TEST_F(HciTest, creditsTest) {
  ASSERT_EQ(0, hal->GetNumSentCommands());

  auto command_future = hal->GetSentCommandFuture();

  // Send all three commands
  upper->SendHciCommandExpectingComplete(ReadLocalVersionInformationBuilder::Create());
  upper->SendHciCommandExpectingComplete(ReadLocalSupportedCommandsBuilder::Create());
  upper->SendHciCommandExpectingComplete(ReadLocalSupportedFeaturesBuilder::Create());

  auto command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);

  // Verify that the first one is sent
  ASSERT_EQ(1, hal->GetNumSentCommands());

  auto sent_command = hal->GetSentCommand();
  auto version_view = ReadLocalVersionInformationView::Create(CommandPacketView::Create(sent_command));
  ASSERT_TRUE(version_view.IsValid());

  // Verify that only one was sent
  ASSERT_EQ(0, hal->GetNumSentCommands());

  // Get a new future
  auto event_future = upper->GetReceivedEventFuture();

  // Send the response event
  uint8_t num_packets = 1;
  ErrorCode error_code = ErrorCode::SUCCESS;
  LocalVersionInformation local_version_information;
  local_version_information.hci_version_ = HciVersion::V_5_0;
  local_version_information.hci_revision_ = 0x1234;
  local_version_information.lmp_version_ = LmpVersion::V_4_2;
  local_version_information.manufacturer_name_ = 0xBAD;
  local_version_information.lmp_subversion_ = 0x5678;
  hal->callbacks->hciEventReceived(GetPacketBytes(
      ReadLocalVersionInformationCompleteBuilder::Create(num_packets, error_code, local_version_information)));

  // Wait for the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);

  auto event = upper->GetReceivedEvent();
  ASSERT(ReadLocalVersionInformationCompleteView::Create(CommandCompleteView::Create(EventPacketView::Create(event)))
             .IsValid());

  // Verify that the second one is sent
  command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);
  ASSERT_EQ(1, hal->GetNumSentCommands());

  sent_command = hal->GetSentCommand();
  auto supported_commands_view = ReadLocalSupportedCommandsView::Create(CommandPacketView::Create(sent_command));
  ASSERT_TRUE(supported_commands_view.IsValid());

  // Verify that only one was sent
  ASSERT_EQ(0, hal->GetNumSentCommands());
  event_future = upper->GetReceivedEventFuture();
  command_future = hal->GetSentCommandFuture();

  // Send the response event
  std::array<uint8_t, 64> supported_commands;
  for (uint8_t i = 0; i < 64; i++) {
    supported_commands[i] = i;
  }
  hal->callbacks->hciEventReceived(
      GetPacketBytes(ReadLocalSupportedCommandsCompleteBuilder::Create(num_packets, error_code, supported_commands)));
  // Wait for the event
  event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);

  event = upper->GetReceivedEvent();
  ASSERT(ReadLocalSupportedCommandsCompleteView::Create(CommandCompleteView::Create(EventPacketView::Create(event)))
             .IsValid());
  // Verify that the third one is sent
  command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);
  ASSERT_EQ(1, hal->GetNumSentCommands());

  sent_command = hal->GetSentCommand();
  auto supported_features_view = ReadLocalSupportedFeaturesView::Create(CommandPacketView::Create(sent_command));
  ASSERT_TRUE(supported_features_view.IsValid());

  // Verify that only one was sent
  ASSERT_EQ(0, hal->GetNumSentCommands());
  event_future = upper->GetReceivedEventFuture();

  // Send the response event
  uint64_t lmp_features = 0x012345678abcdef;
  hal->callbacks->hciEventReceived(
      GetPacketBytes(ReadLocalSupportedFeaturesCompleteBuilder::Create(num_packets, error_code, lmp_features)));

  // Wait for the event
  event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);
  event = upper->GetReceivedEvent();
  ASSERT(ReadLocalSupportedFeaturesCompleteView::Create(CommandCompleteView::Create(EventPacketView::Create(event)))
             .IsValid());
}

TEST_F(HciTest, leSecurityInterfaceTest) {
  // Send LeRand to the controller
  auto command_future = hal->GetSentCommandFuture();
  upper->SendLeSecurityCommandExpectingComplete(LeRandBuilder::Create());

  auto command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);

  // Check the command
  auto sent_command = hal->GetSentCommand();
  ASSERT_LT(0, sent_command.size());
  LeRandView view = LeRandView::Create(LeSecurityCommandView::Create(CommandPacketView::Create(sent_command)));
  ASSERT_TRUE(view.IsValid());

  // Send a Command Complete to the host
  auto event_future = upper->GetReceivedEventFuture();
  uint8_t num_packets = 1;
  ErrorCode status = ErrorCode::SUCCESS;
  uint64_t rand = 0x0123456789abcdef;
  hal->callbacks->hciEventReceived(GetPacketBytes(LeRandCompleteBuilder::Create(num_packets, status, rand)));

  // Verify the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);
  auto event = upper->GetReceivedEvent();
  ASSERT_TRUE(event.IsValid());
  ASSERT_EQ(EventCode::COMMAND_COMPLETE, event.GetEventCode());
  ASSERT_TRUE(LeRandCompleteView::Create(CommandCompleteView::Create(event)).IsValid());
}

TEST_F(HciTest, securityInterfacesTest) {
  // Send WriteSimplePairingMode to the controller
  auto command_future = hal->GetSentCommandFuture();
  Enable enable = Enable::ENABLED;
  upper->SendSecurityCommandExpectingComplete(WriteSimplePairingModeBuilder::Create(enable));

  auto command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);

  // Check the command
  auto sent_command = hal->GetSentCommand();
  ASSERT_LT(0, sent_command.size());
  auto view = WriteSimplePairingModeView::Create(SecurityCommandView::Create(CommandPacketView::Create(sent_command)));
  ASSERT_TRUE(view.IsValid());

  // Send a Command Complete to the host
  auto event_future = upper->GetReceivedEventFuture();
  uint8_t num_packets = 1;
  ErrorCode status = ErrorCode::SUCCESS;
  hal->callbacks->hciEventReceived(GetPacketBytes(WriteSimplePairingModeCompleteBuilder::Create(num_packets, status)));

  // Verify the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);
  auto event = upper->GetReceivedEvent();
  ASSERT_TRUE(event.IsValid());
  ASSERT_EQ(EventCode::COMMAND_COMPLETE, event.GetEventCode());
  ASSERT_TRUE(WriteSimplePairingModeCompleteView::Create(CommandCompleteView::Create(event)).IsValid());
}

TEST_F(HciTest, createConnectionTest) {
  // Send CreateConnection to the controller
  auto command_future = hal->GetSentCommandFuture();
  Address bd_addr;
  ASSERT_TRUE(Address::FromString("A1:A2:A3:A4:A5:A6", bd_addr));
  uint16_t packet_type = 0x1234;
  PageScanRepetitionMode page_scan_repetition_mode = PageScanRepetitionMode::R0;
  uint16_t clock_offset = 0x3456;
  ClockOffsetValid clock_offset_valid = ClockOffsetValid::VALID;
  CreateConnectionRoleSwitch allow_role_switch = CreateConnectionRoleSwitch::ALLOW_ROLE_SWITCH;
  upper->SendHciCommandExpectingStatus(CreateConnectionBuilder::Create(
      bd_addr, packet_type, page_scan_repetition_mode, clock_offset, clock_offset_valid, allow_role_switch));

  auto command_sent_status = command_future.wait_for(kTimeout);
  ASSERT_EQ(command_sent_status, std::future_status::ready);

  // Check the command
  auto sent_command = hal->GetSentCommand();
  ASSERT_LT(0, sent_command.size());
  CreateConnectionView view =
      CreateConnectionView::Create(ConnectionManagementCommandView::Create(CommandPacketView::Create(sent_command)));
  ASSERT_TRUE(view.IsValid());
  ASSERT_EQ(bd_addr, view.GetBdAddr());
  ASSERT_EQ(packet_type, view.GetPacketType());
  ASSERT_EQ(page_scan_repetition_mode, view.GetPageScanRepetitionMode());
  ASSERT_EQ(clock_offset, view.GetClockOffset());
  ASSERT_EQ(clock_offset_valid, view.GetClockOffsetValid());
  ASSERT_EQ(allow_role_switch, view.GetAllowRoleSwitch());

  // Send a Command Status to the host
  auto event_future = upper->GetReceivedEventFuture();
  ErrorCode status = ErrorCode::SUCCESS;
  uint16_t handle = 0x123;
  LinkType link_type = LinkType::ACL;
  Enable encryption_enabled = Enable::DISABLED;
  hal->callbacks->hciEventReceived(GetPacketBytes(CreateConnectionStatusBuilder::Create(ErrorCode::SUCCESS, 1)));

  // Verify the event
  auto event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);
  auto event = upper->GetReceivedEvent();
  ASSERT_TRUE(event.IsValid());
  ASSERT_EQ(EventCode::COMMAND_STATUS, event.GetEventCode());

  // Send a ConnectionComplete to the host
  event_future = upper->GetReceivedEventFuture();
  hal->callbacks->hciEventReceived(
      GetPacketBytes(ConnectionCompleteBuilder::Create(status, handle, bd_addr, link_type, encryption_enabled)));

  // Verify the event
  event_status = event_future.wait_for(kTimeout);
  ASSERT_EQ(event_status, std::future_status::ready);
  event = upper->GetReceivedEvent();
  ASSERT_TRUE(event.IsValid());
  ASSERT_EQ(EventCode::CONNECTION_COMPLETE, event.GetEventCode());
  ConnectionCompleteView connection_complete_view = ConnectionCompleteView::Create(event);
  ASSERT_TRUE(connection_complete_view.IsValid());
  ASSERT_EQ(status, connection_complete_view.GetStatus());
  ASSERT_EQ(handle, connection_complete_view.GetConnectionHandle());
  ASSERT_EQ(link_type, connection_complete_view.GetLinkType());
  ASSERT_EQ(encryption_enabled, connection_complete_view.GetEncryptionEnabled());

  // Send an ACL packet from the remote
  PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
  BroadcastFlag broadcast_flag = BroadcastFlag::POINT_TO_POINT;
  auto acl_payload = std::make_unique<RawBuilder>();
  acl_payload->AddAddress(bd_addr);
  acl_payload->AddOctets2(handle);
  auto incoming_acl_future = upper->GetReceivedAclFuture();
  hal->callbacks->aclDataReceived(
      GetPacketBytes(AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(acl_payload))));

  // Verify the ACL packet
  auto incoming_acl_status = incoming_acl_future.wait_for(kAclTimeout);
  ASSERT_EQ(incoming_acl_status, std::future_status::ready);
  auto acl_view = upper->GetReceivedAcl();
  ASSERT_TRUE(acl_view.IsValid());
  ASSERT_EQ(sizeof(bd_addr) + sizeof(handle), acl_view.GetPayload().size());
  auto itr = acl_view.GetPayload().begin();
  ASSERT_EQ(bd_addr, itr.extract<Address>());
  ASSERT_EQ(handle, itr.extract<uint16_t>());

  // Send an ACL packet from DependsOnHci
  PacketBoundaryFlag packet_boundary_flag2 = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
  BroadcastFlag broadcast_flag2 = BroadcastFlag::POINT_TO_POINT;
  auto acl_payload2 = std::make_unique<RawBuilder>();
  acl_payload2->AddOctets2(handle);
  acl_payload2->AddAddress(bd_addr);
  auto sent_acl_future = hal->GetSentAclFuture();
  upper->SendAclData(AclPacketBuilder::Create(handle, packet_boundary_flag2, broadcast_flag2, std::move(acl_payload2)));

  // Verify the ACL packet
  auto sent_acl_status = sent_acl_future.wait_for(kAclTimeout);
  ASSERT_EQ(sent_acl_status, std::future_status::ready);
  auto sent_acl = hal->GetSentAcl();
  ASSERT_LT(0, sent_acl.size());
  AclPacketView sent_acl_view = AclPacketView::Create(sent_acl);
  ASSERT_TRUE(sent_acl_view.IsValid());
  ASSERT_EQ(sizeof(bd_addr) + sizeof(handle), sent_acl_view.GetPayload().size());
  auto sent_itr = sent_acl_view.GetPayload().begin();
  ASSERT_EQ(handle, sent_itr.extract<uint16_t>());
  ASSERT_EQ(bd_addr, sent_itr.extract<Address>());
}

TEST_F(HciTest, receiveMultipleAclPackets) {
  Address bd_addr;
  ASSERT_TRUE(Address::FromString("A1:A2:A3:A4:A5:A6", bd_addr));
  uint16_t handle = 0x0001;
  uint16_t num_packets = 100;
  PacketBoundaryFlag packet_boundary_flag = PacketBoundaryFlag::FIRST_AUTOMATICALLY_FLUSHABLE;
  BroadcastFlag broadcast_flag = BroadcastFlag::POINT_TO_POINT;
  for (uint16_t i = 0; i < num_packets; i++) {
    auto acl_payload = std::make_unique<RawBuilder>();
    acl_payload->AddAddress(bd_addr);
    acl_payload->AddOctets2(handle);
    acl_payload->AddOctets2(i);
    hal->callbacks->aclDataReceived(
        GetPacketBytes(AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(acl_payload))));
  }
  auto incoming_acl_future = upper->GetReceivedAclFuture();
  uint16_t received_packets = 0;
  while (received_packets < num_packets - 1) {
    auto incoming_acl_status = incoming_acl_future.wait_for(kAclTimeout);
    // Get the next future.
    incoming_acl_future = upper->GetReceivedAclFuture();
    ASSERT_EQ(incoming_acl_status, std::future_status::ready);
    size_t num_packets = upper->GetNumReceivedAclPackets();
    for (size_t i = 0; i < num_packets; i++) {
      auto acl_view = upper->GetReceivedAcl();
      ASSERT_TRUE(acl_view.IsValid());
      ASSERT_EQ(sizeof(bd_addr) + sizeof(handle) + sizeof(received_packets), acl_view.GetPayload().size());
      auto itr = acl_view.GetPayload().begin();
      ASSERT_EQ(bd_addr, itr.extract<Address>());
      ASSERT_EQ(handle, itr.extract<uint16_t>());
      ASSERT_EQ(received_packets, itr.extract<uint16_t>());
      received_packets += 1;
    }
  }

  // Check to see if this future was already fulfilled.
  auto acl_race_status = incoming_acl_future.wait_for(std::chrono::milliseconds(1));
  if (acl_race_status == std::future_status::ready) {
    // Get the next future.
    incoming_acl_future = upper->GetReceivedAclFuture();
  }

  // One last packet to make sure they were all sent.  Already got the future.
  auto acl_payload = std::make_unique<RawBuilder>();
  acl_payload->AddAddress(bd_addr);
  acl_payload->AddOctets2(handle);
  acl_payload->AddOctets2(num_packets);
  hal->callbacks->aclDataReceived(
      GetPacketBytes(AclPacketBuilder::Create(handle, packet_boundary_flag, broadcast_flag, std::move(acl_payload))));
  auto incoming_acl_status = incoming_acl_future.wait_for(kAclTimeout);
  ASSERT_EQ(incoming_acl_status, std::future_status::ready);
  auto acl_view = upper->GetReceivedAcl();
  ASSERT_TRUE(acl_view.IsValid());
  ASSERT_EQ(sizeof(bd_addr) + sizeof(handle) + sizeof(received_packets), acl_view.GetPayload().size());
  auto itr = acl_view.GetPayload().begin();
  ASSERT_EQ(bd_addr, itr.extract<Address>());
  ASSERT_EQ(handle, itr.extract<uint16_t>());
  ASSERT_EQ(received_packets, itr.extract<uint16_t>());
}
}  // namespace hci
}  // namespace bluetooth
