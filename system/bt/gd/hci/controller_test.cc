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

#include "hci/controller.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <map>

#include <gtest/gtest.h>

#include "common/bind.h"
#include "common/callback.h"
#include "hci/address.h"
#include "hci/hci_layer.h"
#include "os/thread.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace hci {
namespace {

using common::BidiQueue;
using common::BidiQueueEnd;
using packet::kLittleEndian;
using packet::PacketView;
using packet::RawBuilder;

constexpr uint16_t kHandle1 = 0x123;
constexpr uint16_t kCredits1 = 0x78;
constexpr uint16_t kHandle2 = 0x456;
constexpr uint16_t kCredits2 = 0x9a;
uint16_t feature_spec_version = 55;

PacketView<kLittleEndian> GetPacketView(std::unique_ptr<packet::BasePacketBuilder> packet) {
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter i(*bytes);
  bytes->reserve(packet->size());
  packet->Serialize(i);
  return packet::PacketView<packet::kLittleEndian>(bytes);
}

class TestHciLayer : public HciLayer {
 public:
  void EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command,
                      common::OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) override {
    GetHandler()->Post(common::BindOnce(&TestHciLayer::HandleCommand, common::Unretained(this), std::move(command),
                                        std::move(on_complete), common::Unretained(handler)));
  }

  void EnqueueCommand(std::unique_ptr<CommandPacketBuilder> command,
                      common::OnceCallback<void(CommandStatusView)> on_status, os::Handler* handler) override {
    EXPECT_TRUE(false) << "Controller properties should not generate Command Status";
  }

  void HandleCommand(std::unique_ptr<CommandPacketBuilder> command_builder,
                     common::OnceCallback<void(CommandCompleteView)> on_complete, os::Handler* handler) {
    auto packet_view = GetPacketView(std::move(command_builder));
    CommandPacketView command = CommandPacketView::Create(packet_view);
    ASSERT(command.IsValid());

    uint8_t num_packets = 1;
    std::unique_ptr<packet::BasePacketBuilder> event_builder;
    switch (command.GetOpCode()) {
      case (OpCode::READ_LOCAL_NAME): {
        std::array<uint8_t, 248> local_name = {'D', 'U', 'T', '\0'};
        event_builder = ReadLocalNameCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, local_name);
      } break;
      case (OpCode::READ_LOCAL_VERSION_INFORMATION): {
        LocalVersionInformation local_version_information;
        local_version_information.hci_version_ = HciVersion::V_5_0;
        local_version_information.hci_revision_ = 0x1234;
        local_version_information.lmp_version_ = LmpVersion::V_4_2;
        local_version_information.manufacturer_name_ = 0xBAD;
        local_version_information.lmp_subversion_ = 0x5678;
        event_builder = ReadLocalVersionInformationCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS,
                                                                           local_version_information);
      } break;
      case (OpCode::READ_LOCAL_SUPPORTED_COMMANDS): {
        std::array<uint8_t, 64> supported_commands;
        for (int i = 0; i < 37; i++) {
          supported_commands[i] = 0xff;
        }
        for (int i = 37; i < 64; i++) {
          supported_commands[i] = 0x00;
        }
        event_builder =
            ReadLocalSupportedCommandsCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, supported_commands);
      } break;
      case (OpCode::READ_LOCAL_SUPPORTED_FEATURES): {
        uint64_t lmp_features = 0x012345678abcdef;
        event_builder =
            ReadLocalSupportedFeaturesCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, lmp_features);
      } break;
      case (OpCode::READ_LOCAL_EXTENDED_FEATURES): {
        ReadLocalExtendedFeaturesView read_command = ReadLocalExtendedFeaturesView::Create(command);
        ASSERT(read_command.IsValid());
        uint8_t page_bumber = read_command.GetPageNumber();
        uint64_t lmp_features = 0x012345678abcdef;
        lmp_features += page_bumber;
        event_builder = ReadLocalExtendedFeaturesCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, page_bumber,
                                                                         0x02, lmp_features);
      } break;
      case (OpCode::READ_BUFFER_SIZE): {
        event_builder = ReadBufferSizeCompleteBuilder::Create(
            num_packets, ErrorCode::SUCCESS, acl_data_packet_length, synchronous_data_packet_length,
            total_num_acl_data_packets, total_num_synchronous_data_packets);
      } break;
      case (OpCode::READ_BD_ADDR): {
        event_builder = ReadBdAddrCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, Address::kAny);
      } break;
      case (OpCode::LE_READ_BUFFER_SIZE): {
        LeBufferSize le_buffer_size;
        le_buffer_size.le_data_packet_length_ = 0x16;
        le_buffer_size.total_num_le_packets_ = 0x08;
        event_builder = LeReadBufferSizeCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, le_buffer_size);
      } break;
      case (OpCode::LE_READ_LOCAL_SUPPORTED_FEATURES): {
        event_builder =
            LeReadLocalSupportedFeaturesCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, 0x001f123456789abc);
      } break;
      case (OpCode::LE_READ_SUPPORTED_STATES): {
        event_builder =
            LeReadSupportedStatesCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, 0x001f123456789abe);
      } break;
      case (OpCode::LE_READ_MAXIMUM_DATA_LENGTH): {
        LeMaximumDataLength le_maximum_data_length;
        le_maximum_data_length.supported_max_tx_octets_ = 0x12;
        le_maximum_data_length.supported_max_tx_time_ = 0x34;
        le_maximum_data_length.supported_max_rx_octets_ = 0x56;
        le_maximum_data_length.supported_max_rx_time_ = 0x78;
        event_builder =
            LeReadMaximumDataLengthCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, le_maximum_data_length);
      } break;
      case (OpCode::LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH): {
        event_builder =
            LeReadMaximumAdvertisingDataLengthCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, 0x0672);
      } break;
      case (OpCode::LE_READ_NUMBER_OF_SUPPORTED_ADVERTISING_SETS): {
        event_builder =
            LeReadNumberOfSupportedAdvertisingSetsCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS, 0xF0);
      } break;
      case (OpCode::LE_GET_VENDOR_CAPABILITIES): {
        BaseVendorCapabilities base_vendor_capabilities;
        base_vendor_capabilities.max_advt_instances_ = 0x10;
        base_vendor_capabilities.offloaded_resolution_of_private_address_ = 0x01;
        base_vendor_capabilities.total_scan_results_storage_ = 0x2800;
        base_vendor_capabilities.max_irk_list_sz_ = 0x20;
        base_vendor_capabilities.filtering_support_ = 0x01;
        base_vendor_capabilities.max_filter_ = 0x10;
        base_vendor_capabilities.activity_energy_info_support_ = 0x01;

        auto payload = std::make_unique<RawBuilder>();
        if (feature_spec_version > 55) {
          std::vector<uint8_t> payload_bytes = {0x20, 0x00, 0x01, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00};
          payload->AddOctets2(feature_spec_version);
          payload->AddOctets(payload_bytes);
        }
        event_builder = LeGetVendorCapabilitiesCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS,
                                                                       base_vendor_capabilities, std::move(payload));
      } break;
      case (OpCode::SET_EVENT_MASK): {
        auto view = SetEventMaskView::Create(command);
        ASSERT(view.IsValid());
        event_mask = view.GetEventMask();
        event_builder = SetEventMaskCompleteBuilder::Create(num_packets, ErrorCode::SUCCESS);
      } break;
      case (OpCode::RESET):
      case (OpCode::SET_EVENT_FILTER):
      case (OpCode::HOST_BUFFER_SIZE):
      case (OpCode::LE_SET_EVENT_MASK):
        command_queue_.push(command);
        not_empty_.notify_all();
        return;
      default:
        LOG_INFO("Dropping unhandled packet");
        return;
    }
    auto packet = GetPacketView(std::move(event_builder));
    EventPacketView event = EventPacketView::Create(packet);
    ASSERT(event.IsValid());
    CommandCompleteView command_complete = CommandCompleteView::Create(event);
    ASSERT(command_complete.IsValid());
    handler->Post(common::BindOnce(std::move(on_complete), std::move(command_complete)));
  }

  void RegisterEventHandler(EventCode event_code, common::Callback<void(EventPacketView)> event_handler,
                            os::Handler* handler) override {
    EXPECT_EQ(event_code, EventCode::NUMBER_OF_COMPLETED_PACKETS) << "Only NUMBER_OF_COMPLETED_PACKETS is needed";
    number_of_completed_packets_callback_ = event_handler;
    client_handler_ = handler;
  }

  void UnregisterEventHandler(EventCode event_code) override {
    EXPECT_EQ(event_code, EventCode::NUMBER_OF_COMPLETED_PACKETS) << "Only NUMBER_OF_COMPLETED_PACKETS is needed";
    number_of_completed_packets_callback_ = {};
    client_handler_ = nullptr;
  }

  void IncomingCredit() {
    std::vector<CompletedPackets> completed_packets;
    CompletedPackets cp;
    cp.host_num_of_completed_packets_ = kCredits1;
    cp.connection_handle_ = kHandle1;
    completed_packets.push_back(cp);
    cp.host_num_of_completed_packets_ = kCredits2;
    cp.connection_handle_ = kHandle2;
    completed_packets.push_back(cp);
    auto event_builder = NumberOfCompletedPacketsBuilder::Create(completed_packets);
    auto packet = GetPacketView(std::move(event_builder));
    EventPacketView event = EventPacketView::Create(packet);
    ASSERT(event.IsValid());
    client_handler_->Post(common::BindOnce(number_of_completed_packets_callback_, event));
  }

  CommandPacketView GetCommand(OpCode op_code) {
    std::unique_lock<std::mutex> lock(mutex_);
    std::chrono::milliseconds time = std::chrono::milliseconds(3000);

    // wait for command
    while (command_queue_.size() == 0) {
      if (not_empty_.wait_for(lock, time) == std::cv_status::timeout) {
        break;
      }
    }
    ASSERT(command_queue_.size() > 0);
    CommandPacketView command = command_queue_.front();
    EXPECT_EQ(command.GetOpCode(), op_code);
    command_queue_.pop();
    return command;
  }

  void ListDependencies(ModuleList* list) override {}
  void Start() override {}
  void Stop() override {}

  constexpr static uint16_t acl_data_packet_length = 1024;
  constexpr static uint8_t synchronous_data_packet_length = 60;
  constexpr static uint16_t total_num_acl_data_packets = 10;
  constexpr static uint16_t total_num_synchronous_data_packets = 12;
  uint64_t event_mask = 0;

 private:
  common::Callback<void(EventPacketView)> number_of_completed_packets_callback_;
  os::Handler* client_handler_;
  std::queue<CommandPacketView> command_queue_;
  mutable std::mutex mutex_;
  std::condition_variable not_empty_;
};

class ControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_hci_layer_ = new TestHciLayer;
    fake_registry_.InjectTestModule(&HciLayer::Factory, test_hci_layer_);
    client_handler_ = fake_registry_.GetTestModuleHandler(&HciLayer::Factory);
    fake_registry_.Start<Controller>(&thread_);
    controller_ = static_cast<Controller*>(fake_registry_.GetModuleUnderTest(&Controller::Factory));
  }

  void TearDown() override {
    fake_registry_.StopAll();
  }

  TestModuleRegistry fake_registry_;
  TestHciLayer* test_hci_layer_ = nullptr;
  os::Thread& thread_ = fake_registry_.GetTestThread();
  Controller* controller_ = nullptr;
  os::Handler* client_handler_ = nullptr;
};

TEST_F(ControllerTest, startup_teardown) {}

TEST_F(ControllerTest, read_controller_info) {
  ASSERT_EQ(controller_->GetControllerAclPacketLength(), test_hci_layer_->acl_data_packet_length);
  ASSERT_EQ(controller_->GetControllerNumAclPacketBuffers(), test_hci_layer_->total_num_acl_data_packets);
  ASSERT_EQ(controller_->GetControllerScoPacketLength(), test_hci_layer_->synchronous_data_packet_length);
  ASSERT_EQ(controller_->GetControllerNumScoPacketBuffers(), test_hci_layer_->total_num_synchronous_data_packets);
  ASSERT_EQ(controller_->GetControllerMacAddress(), Address::kAny);
  LocalVersionInformation local_version_information = controller_->GetControllerLocalVersionInformation();
  ASSERT_EQ(local_version_information.hci_version_, HciVersion::V_5_0);
  ASSERT_EQ(local_version_information.hci_revision_, 0x1234);
  ASSERT_EQ(local_version_information.lmp_version_, LmpVersion::V_4_2);
  ASSERT_EQ(local_version_information.manufacturer_name_, 0xBAD);
  ASSERT_EQ(local_version_information.lmp_subversion_, 0x5678);
  std::array<uint8_t, 64> supported_commands;
  for (int i = 0; i < 37; i++) {
    supported_commands[i] = 0xff;
  }
  for (int i = 37; i < 64; i++) {
    supported_commands[i] = 0x00;
  }
  ASSERT_EQ(controller_->GetControllerLocalSupportedCommands(), supported_commands);
  ASSERT_EQ(controller_->GetControllerLocalSupportedFeatures(), 0x012345678abcdef);
  ASSERT_EQ(controller_->GetControllerLocalExtendedFeaturesMaxPageNumber(), 0x02);
  ASSERT_EQ(controller_->GetControllerLocalExtendedFeatures(0), 0x012345678abcdef);
  ASSERT_EQ(controller_->GetControllerLocalExtendedFeatures(1), 0x012345678abcdf0);
  ASSERT_EQ(controller_->GetControllerLocalExtendedFeatures(2), 0x012345678abcdf1);
  ASSERT_EQ(controller_->GetControllerLocalExtendedFeatures(100), 0x00);
  ASSERT_EQ(controller_->GetControllerLeBufferSize().le_data_packet_length_, 0x16);
  ASSERT_EQ(controller_->GetControllerLeBufferSize().total_num_le_packets_, 0x08);
  ASSERT_EQ(controller_->GetControllerLeLocalSupportedFeatures(), 0x001f123456789abc);
  ASSERT_EQ(controller_->GetControllerLeSupportedStates(), 0x001f123456789abe);
  ASSERT_EQ(controller_->GetControllerLeMaximumDataLength().supported_max_tx_octets_, 0x12);
  ASSERT_EQ(controller_->GetControllerLeMaximumDataLength().supported_max_rx_octets_, 0x56);
  ASSERT_EQ(controller_->GetControllerLeMaximumAdvertisingDataLength(), 0x0672);
  ASSERT_EQ(controller_->GetControllerLeNumberOfSupportedAdverisingSets(), 0xF0);
}

TEST_F(ControllerTest, read_write_local_name) {
  ASSERT_EQ(controller_->GetControllerLocalName(), "DUT");
  controller_->WriteLocalName("New name");
  ASSERT_EQ(controller_->GetControllerLocalName(), "New name");
}

TEST_F(ControllerTest, send_set_event_mask_command) {
  uint64_t new_event_mask = test_hci_layer_->event_mask - 1;
  controller_->SetEventMask(new_event_mask);
  // Send another command to make sure it was applied
  controller_->Reset();
  auto packet = test_hci_layer_->GetCommand(OpCode::RESET);
  ASSERT_EQ(new_event_mask, test_hci_layer_->event_mask);
}

TEST_F(ControllerTest, send_reset_command) {
  controller_->Reset();
  auto packet = test_hci_layer_->GetCommand(OpCode::RESET);
  auto command = ResetView::Create(packet);
  ASSERT(command.IsValid());
}

TEST_F(ControllerTest, send_set_event_filter_command) {
  controller_->SetEventFilterInquiryResultAllDevices();
  auto packet = test_hci_layer_->GetCommand(OpCode::SET_EVENT_FILTER);
  auto set_event_filter_view1 = SetEventFilterView::Create(packet);
  auto set_event_filter_inquiry_result_view1 = SetEventFilterInquiryResultView::Create(set_event_filter_view1);
  auto command1 = SetEventFilterInquiryResultAllDevicesView::Create(set_event_filter_inquiry_result_view1);
  ASSERT(command1.IsValid());

  ClassOfDevice class_of_device({0xab, 0xcd, 0xef});
  ClassOfDevice class_of_device_mask({0x12, 0x34, 0x56});
  controller_->SetEventFilterInquiryResultClassOfDevice(class_of_device, class_of_device_mask);
  packet = test_hci_layer_->GetCommand(OpCode::SET_EVENT_FILTER);
  auto set_event_filter_view2 = SetEventFilterView::Create(packet);
  auto set_event_filter_inquiry_result_view2 = SetEventFilterInquiryResultView::Create(set_event_filter_view2);
  auto command2 = SetEventFilterInquiryResultClassOfDeviceView::Create(set_event_filter_inquiry_result_view2);
  ASSERT(command2.IsValid());
  ASSERT_EQ(command2.GetClassOfDevice(), class_of_device);

  Address bdaddr({0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc});
  controller_->SetEventFilterConnectionSetupAddress(bdaddr, AutoAcceptFlag::AUTO_ACCEPT_ON_ROLE_SWITCH_ENABLED);
  packet = test_hci_layer_->GetCommand(OpCode::SET_EVENT_FILTER);
  auto set_event_filter_view3 = SetEventFilterView::Create(packet);
  auto set_event_filter_connection_setup_view = SetEventFilterConnectionSetupView::Create(set_event_filter_view3);
  auto command3 = SetEventFilterConnectionSetupAddressView::Create(set_event_filter_connection_setup_view);
  ASSERT(command3.IsValid());
  ASSERT_EQ(command3.GetAddress(), bdaddr);
}

TEST_F(ControllerTest, send_host_buffer_size_command) {
  controller_->HostBufferSize(0xFF00, 0xF1, 0xFF02, 0xFF03);
  auto packet = test_hci_layer_->GetCommand(OpCode::HOST_BUFFER_SIZE);
  auto command = HostBufferSizeView::Create(packet);
  ASSERT(command.IsValid());
  ASSERT_EQ(command.GetHostAclDataPacketLength(), 0xFF00);
  ASSERT_EQ(command.GetHostSynchronousDataPacketLength(), 0xF1);
  ASSERT_EQ(command.GetHostTotalNumAclDataPackets(), 0xFF02);
  ASSERT_EQ(command.GetHostTotalNumSynchronousDataPackets(), 0xFF03);
}

TEST_F(ControllerTest, send_le_set_event_mask_command) {
  controller_->LeSetEventMask(0x000000000000001F);
  auto packet = test_hci_layer_->GetCommand(OpCode::LE_SET_EVENT_MASK);
  auto command = LeSetEventMaskView::Create(packet);
  ASSERT(command.IsValid());
  ASSERT_EQ(command.GetLeEventMask(), 0x000000000000001F);
}

TEST_F(ControllerTest, is_supported_test) {
  ASSERT_TRUE(controller_->IsSupported(OpCode::INQUIRY));
  ASSERT_TRUE(controller_->IsSupported(OpCode::REJECT_CONNECTION_REQUEST));
  ASSERT_TRUE(controller_->IsSupported(OpCode::ACCEPT_CONNECTION_REQUEST));
  ASSERT_FALSE(controller_->IsSupported(OpCode::LE_REMOVE_ADVERTISING_SET));
  ASSERT_FALSE(controller_->IsSupported(OpCode::LE_CLEAR_ADVERTISING_SETS));
  ASSERT_FALSE(controller_->IsSupported(OpCode::LE_SET_PERIODIC_ADVERTISING_PARAM));
}

TEST_F(ControllerTest, feature_spec_version_055_test) {
  EXPECT_EQ(controller_->GetControllerVendorCapabilities().version_supported_, 55);
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_MULTI_ADVT));
  EXPECT_FALSE(controller_->IsSupported(OpCode::LE_TRACK_ADV));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_DEBUG_INFO));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_A2DP_OPCODE));
  feature_spec_version = 95;
}

TEST_F(ControllerTest, feature_spec_version_095_test) {
  EXPECT_EQ(controller_->GetControllerVendorCapabilities().version_supported_, 95);
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_MULTI_ADVT));
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_TRACK_ADV));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_DEBUG_INFO));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_A2DP_OPCODE));
  feature_spec_version = 96;
}

TEST_F(ControllerTest, feature_spec_version_096_test) {
  EXPECT_EQ(controller_->GetControllerVendorCapabilities().version_supported_, 96);
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_MULTI_ADVT));
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_TRACK_ADV));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_DEBUG_INFO));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_A2DP_OPCODE));
  feature_spec_version = 98;
}

TEST_F(ControllerTest, feature_spec_version_098_test) {
  EXPECT_EQ(controller_->GetControllerVendorCapabilities().version_supported_, 98);
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_MULTI_ADVT));
  EXPECT_TRUE(controller_->IsSupported(OpCode::LE_TRACK_ADV));
  EXPECT_FALSE(controller_->IsSupported(OpCode::CONTROLLER_DEBUG_INFO));
  EXPECT_TRUE(controller_->IsSupported(OpCode::CONTROLLER_A2DP_OPCODE));
}

std::promise<void> credits1_set;
std::promise<void> credits2_set;

void CheckReceivedCredits(uint16_t handle, uint16_t credits) {
  switch (handle) {
    case (kHandle1):
      ASSERT_EQ(kCredits1, credits);
      credits1_set.set_value();
      break;
    case (kHandle2):
      ASSERT_EQ(kCredits2, credits);
      credits2_set.set_value();
      break;
    default:
      ASSERT_LOG(false, "Unknown handle 0x%0hx with 0x%0hx credits", handle, credits);
  }
}

TEST_F(ControllerTest, aclCreditCallbacksTest) {
  controller_->RegisterCompletedAclPacketsCallback(common::Bind(&CheckReceivedCredits), client_handler_);

  test_hci_layer_->IncomingCredit();

  credits1_set.get_future().wait();
  credits2_set.get_future().wait();
}
}  // namespace
}  // namespace hci
}  // namespace bluetooth
