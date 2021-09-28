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

#include "neighbor/inquiry.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <map>
#include <memory>

#include <unistd.h>

#include <gtest/gtest.h>

#include "common/bind.h"
#include "common/callback.h"
#include "hci/address.h"
#include "hci/class_of_device.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "os/thread.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace neighbor {
namespace {

static const uint8_t kNumberPacketsReadyToReceive = 1;

/**
 * This structure reflects the current state of the bluetooth chip
 * at any given time.
 */
static const int8_t kInitialInquiryResponseTransmitPowerLevel = 123;
static const uint16_t kInitialInquiryScanInterval = 1111;
static const uint16_t kInitialInquiryScanWindow = 2222;

struct HciRegister {
  bool one_shot_inquiry_active;
  bool periodic_inquiry_active;
  int8_t inquiry_response_transmit_power_level;
  uint16_t inquiry_scan_interval;
  uint16_t inquiry_scan_window;
  hci::InquiryScanType inquiry_scan_type;
  hci::InquiryMode inquiry_mode;
  uint8_t inquiry_length;
  uint8_t num_responses;
  uint16_t min_period_length;
  uint16_t max_period_length;
} hci_register_{
    .one_shot_inquiry_active = false,
    .periodic_inquiry_active = false,
    .inquiry_response_transmit_power_level = kInitialInquiryResponseTransmitPowerLevel,
    .inquiry_scan_interval = kInitialInquiryScanInterval,
    .inquiry_scan_window = kInitialInquiryScanWindow,
    .inquiry_scan_type = hci::InquiryScanType::STANDARD,
    .inquiry_mode = hci::InquiryMode::STANDARD,
    .inquiry_length = 0,
    .num_responses = 0,
    .min_period_length = 0,
    .max_period_length = 0,
};

hci::PacketView<hci::kLittleEndian> GetPacketView(std::unique_ptr<packet::BasePacketBuilder> packet) {
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  hci::BitInserter i(*bytes);
  bytes->reserve(packet->size());
  packet->Serialize(i);
  return packet::PacketView<packet::kLittleEndian>(bytes);
}

class TestHciLayer : public hci::HciLayer {
 public:
  void EnqueueCommand(std::unique_ptr<hci::CommandPacketBuilder> command,
                      common::OnceCallback<void(hci::CommandCompleteView)> on_complete, os::Handler* handler) override {
    GetHandler()->Post(common::BindOnce(&TestHciLayer::HandleCommand, common::Unretained(this), std::move(command),
                                        std::move(on_complete), common::Unretained(handler)));
  }

  void EnqueueCommand(std::unique_ptr<hci::CommandPacketBuilder> command,
                      common::OnceCallback<void(hci::CommandStatusView)> on_status, os::Handler* handler) override {
    GetHandler()->Post(common::BindOnce(&TestHciLayer::HandleStatus, common::Unretained(this), std::move(command),
                                        std::move(on_status), common::Unretained(handler)));
  }

  void HandleCommand(std::unique_ptr<hci::CommandPacketBuilder> command_builder,
                     common::OnceCallback<void(hci::CommandCompleteView)> on_complete, os::Handler* handler) {
    hci::CommandPacketView command = hci::CommandPacketView::Create(GetPacketView(std::move(command_builder)));
    ASSERT(command.IsValid());

    std::unique_ptr<packet::BasePacketBuilder> event_builder;
    switch (command.GetOpCode()) {
      case hci::OpCode::INQUIRY_CANCEL:
        event_builder =
            hci::InquiryCancelCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        hci_register_.one_shot_inquiry_active = false;
        break;

      case hci::OpCode::PERIODIC_INQUIRY_MODE: {
        auto inquiry = hci::PeriodicInquiryModeView::Create(hci::DiscoveryCommandView::Create(command));
        ASSERT(inquiry.IsValid());
        event_builder =
            hci::PeriodicInquiryModeCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        hci_register_.periodic_inquiry_active = true;
        hci_register_.inquiry_length = inquiry.GetInquiryLength();
        hci_register_.num_responses = inquiry.GetNumResponses();
        hci_register_.max_period_length = inquiry.GetMaxPeriodLength();
        hci_register_.min_period_length = inquiry.GetMinPeriodLength();
      } break;

      case hci::OpCode::EXIT_PERIODIC_INQUIRY_MODE:
        event_builder =
            hci::ExitPeriodicInquiryModeCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        hci_register_.periodic_inquiry_active = false;
        break;

      case hci::OpCode::WRITE_INQUIRY_MODE:
        event_builder =
            hci::WriteInquiryModeCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        {
          auto view = hci::WriteInquiryModeView::Create(hci::DiscoveryCommandView::Create(command));
          ASSERT(view.IsValid());
          hci_register_.inquiry_mode = view.GetInquiryMode();
        }
        break;

      case hci::OpCode::READ_INQUIRY_MODE:
        event_builder = hci::ReadInquiryModeCompleteBuilder::Create(
            kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS, hci_register_.inquiry_mode);
        break;

      case hci::OpCode::WRITE_INQUIRY_SCAN_ACTIVITY:
        event_builder =
            hci::WriteInquiryScanActivityCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        {
          auto view = hci::WriteInquiryScanActivityView::Create(hci::DiscoveryCommandView::Create(command));
          ASSERT(view.IsValid());
          hci_register_.inquiry_scan_interval = view.GetInquiryScanInterval();
          hci_register_.inquiry_scan_window = view.GetInquiryScanWindow();
        }
        break;

      case hci::OpCode::READ_INQUIRY_SCAN_ACTIVITY:
        event_builder = hci::ReadInquiryScanActivityCompleteBuilder::Create(
            kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS, hci_register_.inquiry_scan_interval,
            hci_register_.inquiry_scan_window);
        break;

      case hci::OpCode::WRITE_INQUIRY_SCAN_TYPE:
        event_builder =
            hci::WriteInquiryScanTypeCompleteBuilder::Create(kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS);
        {
          auto view = hci::WriteInquiryScanTypeView::Create(hci::DiscoveryCommandView::Create(command));
          ASSERT(view.IsValid());
          hci_register_.inquiry_scan_type = view.GetInquiryScanType();
        }
        break;

      case hci::OpCode::READ_INQUIRY_SCAN_TYPE:
        event_builder = hci::ReadInquiryScanTypeCompleteBuilder::Create(
            kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS, hci_register_.inquiry_scan_type);
        break;

      case hci::OpCode::READ_INQUIRY_RESPONSE_TRANSMIT_POWER_LEVEL:
        event_builder = hci::ReadInquiryResponseTransmitPowerLevelCompleteBuilder::Create(
            kNumberPacketsReadyToReceive, hci::ErrorCode::SUCCESS, hci_register_.inquiry_response_transmit_power_level);
        break;

      default:
        LOG_INFO("Dropping unhandled command:%s", hci::OpCodeText(command.GetOpCode()).c_str());
        return;
    }
    hci::EventPacketView event = hci::EventPacketView::Create(GetPacketView(std::move(event_builder)));
    ASSERT(event.IsValid());
    hci::CommandCompleteView command_complete = hci::CommandCompleteView::Create(event);
    ASSERT(command_complete.IsValid());
    handler->Post(common::BindOnce(std::move(on_complete), std::move(command_complete)));

    if (promise_sync_complete_ != nullptr) {
      promise_sync_complete_->set_value(command.GetOpCode());
    }
  }

  void HandleStatus(std::unique_ptr<hci::CommandPacketBuilder> command_builder,
                    common::OnceCallback<void(hci::CommandStatusView)> on_status, os::Handler* handler) {
    hci::CommandPacketView command = hci::CommandPacketView::Create(GetPacketView(std::move(command_builder)));
    ASSERT(command.IsValid());

    std::unique_ptr<packet::BasePacketBuilder> event_builder;
    switch (command.GetOpCode()) {
      case hci::OpCode::INQUIRY: {
        auto inquiry = hci::InquiryView::Create(hci::DiscoveryCommandView::Create(command));
        ASSERT(inquiry.IsValid());
        event_builder = hci::InquiryStatusBuilder::Create(hci::ErrorCode::SUCCESS, kNumberPacketsReadyToReceive);
        hci_register_.one_shot_inquiry_active = true;
        hci_register_.num_responses = inquiry.GetNumResponses();
        hci_register_.inquiry_length = inquiry.GetInquiryLength();
      } break;
      default:
        LOG_INFO("Dropping unhandled status expecting command:%s", hci::OpCodeText(command.GetOpCode()).c_str());
        return;
    }
    hci::EventPacketView event = hci::EventPacketView::Create(GetPacketView(std::move(event_builder)));
    ASSERT(event.IsValid());
    hci::CommandStatusView command_status = hci::CommandStatusView::Create(event);
    ASSERT(command_status.IsValid());
    handler->Post(common::BindOnce(std::move(on_status), std::move(command_status)));

    if (promise_sync_complete_ != nullptr) {
      promise_sync_complete_->set_value(command.GetOpCode());
    }
  }

  void RegisterEventHandler(hci::EventCode event_code, common::Callback<void(hci::EventPacketView)> event_handler,
                            os::Handler* handler) override {
    switch (event_code) {
      case hci::EventCode::INQUIRY_RESULT:
        inquiry_result_handler_ = handler;
        inquiry_result_callback_ = event_handler;
        break;
      case hci::EventCode::INQUIRY_RESULT_WITH_RSSI:
        inquiry_result_with_rssi_handler_ = handler;
        inquiry_result_with_rssi_callback_ = event_handler;
        break;
      case hci::EventCode::EXTENDED_INQUIRY_RESULT:
        extended_inquiry_result_handler_ = handler;
        extended_inquiry_result_callback_ = event_handler;
        break;
      case hci::EventCode::INQUIRY_COMPLETE:
        inquiry_complete_handler_ = handler;
        inquiry_complete_callback_ = event_handler;
        break;
      default:
        ASSERT_TRUE(false) << "Unexpected event handler being registered";
        break;
    }
  }

  void UnregisterEventHandler(hci::EventCode event_code) override {
    if (hci_register_.one_shot_inquiry_active || hci_register_.periodic_inquiry_active) {
      LOG_ERROR("Event handlers may not be unregistered until inquiry is stopped");
      return;
    }

    switch (event_code) {
      case hci::EventCode::INQUIRY_RESULT:
        inquiry_result_handler_ = nullptr;
        inquiry_result_callback_ = {};
        break;
      case hci::EventCode::INQUIRY_RESULT_WITH_RSSI:
        inquiry_result_with_rssi_handler_ = nullptr;
        inquiry_result_with_rssi_callback_ = {};
        break;
      case hci::EventCode::EXTENDED_INQUIRY_RESULT:
        extended_inquiry_result_handler_ = nullptr;
        extended_inquiry_result_callback_ = {};
        break;
      case hci::EventCode::INQUIRY_COMPLETE:
        inquiry_complete_handler_ = nullptr;
        inquiry_complete_callback_ = {};
        break;
      default:
        ASSERT_TRUE(false) << "Unexpected event handler being unregistered";
        break;
    }
  }

  void Synchronize(std::function<void()> func, hci::OpCode op_code) {
    ASSERT(promise_sync_complete_ == nullptr);
    promise_sync_complete_ = new std::promise<hci::OpCode>();
    auto future = promise_sync_complete_->get_future();
    func();
    auto result = future.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(std::future_status::ready, result);
    ASSERT_EQ(op_code, future.get());
    delete promise_sync_complete_;
    promise_sync_complete_ = nullptr;
  }

  void InjectInquiryResult(std::unique_ptr<hci::InquiryResultBuilder> result) {
    if (inquiry_result_handler_ != nullptr) {
      hci::EventPacketView view = hci::EventPacketView::Create(GetPacketView(std::move(result)));
      ASSERT(view.IsValid());
      inquiry_result_handler_->Post(common::BindOnce(inquiry_result_callback_, std::move(view)));
    }
  }

  void ListDependencies(ModuleList* list) override {}
  void Start() override {}
  void Stop() override {}

 private:
  std::promise<hci::OpCode>* promise_sync_complete_{nullptr};

  os::Handler* inquiry_result_handler_{nullptr};
  common::Callback<void(hci::EventPacketView)> inquiry_result_callback_;
  os::Handler* inquiry_result_with_rssi_handler_{nullptr};
  common::Callback<void(hci::EventPacketView)> inquiry_result_with_rssi_callback_;
  os::Handler* extended_inquiry_result_handler_{nullptr};
  common::Callback<void(hci::EventPacketView)> extended_inquiry_result_callback_;
  os::Handler* inquiry_complete_handler_{nullptr};
  common::Callback<void(hci::EventPacketView)> inquiry_complete_callback_;
};

class InquiryTest : public ::testing::Test {
 public:
  void Result(hci::InquiryResultView view) {
    ASSERT(view.size() >= sizeof(uint16_t));
    promise_result_complete_->set_value(true);
  }

  void WaitForInquiryResult(std::function<void()> func) {
    ASSERT(promise_result_complete_ == nullptr);
    promise_result_complete_ = new std::promise<bool>();
    auto future = promise_result_complete_->get_future();
    func();
    future.wait();
    delete promise_result_complete_;
    promise_result_complete_ = nullptr;
  }

  void ResultWithRssi(hci::InquiryResultWithRssiView view) {
    ASSERT(view.size() >= sizeof(uint16_t));
  }

  void ExtendedResult(hci::ExtendedInquiryResultView view) {
    ASSERT(view.size() >= sizeof(uint16_t));
  }

  void Complete(hci::ErrorCode status) {}

 protected:
  void SetUp() override {
    test_hci_layer_ = new TestHciLayer;
    fake_registry_.InjectTestModule(&hci::HciLayer::Factory, test_hci_layer_);
    client_handler_ = fake_registry_.GetTestModuleHandler(&hci::HciLayer::Factory);
    fake_registry_.Start<InquiryModule>(&thread_);

    inquiry_module_ = static_cast<InquiryModule*>(fake_registry_.GetModuleUnderTest(&InquiryModule::Factory));

    InquiryCallbacks inquiry_callbacks;
    inquiry_callbacks.result = std::bind(&InquiryTest::Result, this, std::placeholders::_1);
    inquiry_callbacks.result_with_rssi = std::bind(&InquiryTest::ResultWithRssi, this, std::placeholders::_1);
    inquiry_callbacks.extended_result = std::bind(&InquiryTest::ExtendedResult, this, std::placeholders::_1);
    inquiry_callbacks.complete = std::bind(&InquiryTest::Complete, this, std::placeholders::_1);
    inquiry_module_->RegisterCallbacks(inquiry_callbacks);
  }

  void TearDown() override {
    inquiry_module_->UnregisterCallbacks();
    fake_registry_.StopAll();
  }

  TestModuleRegistry fake_registry_;
  TestHciLayer* test_hci_layer_ = nullptr;
  os::Thread& thread_ = fake_registry_.GetTestThread();
  InquiryModule* inquiry_module_ = nullptr;
  os::Handler* client_handler_ = nullptr;

  std::promise<bool>* promise_result_complete_{nullptr};
};

TEST_F(InquiryTest, Module) {}

TEST_F(InquiryTest, SetInquiryModes) {
  test_hci_layer_->Synchronize([this] { inquiry_module_->SetInquiryWithRssiResultMode(); },
                               hci::OpCode::WRITE_INQUIRY_MODE);
  ASSERT_EQ(hci_register_.inquiry_mode, hci::InquiryMode::RSSI);

  test_hci_layer_->Synchronize([this] { inquiry_module_->SetExtendedInquiryResultMode(); },
                               hci::OpCode::WRITE_INQUIRY_MODE);
  ASSERT_EQ(hci_register_.inquiry_mode, hci::InquiryMode::RSSI_OR_EXTENDED);

  test_hci_layer_->Synchronize([this] { inquiry_module_->SetStandardInquiryResultMode(); },
                               hci::OpCode::WRITE_INQUIRY_MODE);
  ASSERT_EQ(hci_register_.inquiry_mode, hci::InquiryMode::STANDARD);
}

TEST_F(InquiryTest, SetScanType) {
  test_hci_layer_->Synchronize([this] { inquiry_module_->SetInterlacedScan(); }, hci::OpCode::WRITE_INQUIRY_SCAN_TYPE);
  ASSERT_EQ(hci_register_.inquiry_scan_type, hci::InquiryScanType::INTERLACED);

  test_hci_layer_->Synchronize([this] { inquiry_module_->SetStandardScan(); }, hci::OpCode::WRITE_INQUIRY_SCAN_TYPE);
  ASSERT_EQ(hci_register_.inquiry_scan_type, hci::InquiryScanType::STANDARD);
}

TEST_F(InquiryTest, ScanActivity) {
  ScanParameters params{
      .interval = 0x1234,
      .window = 0x5678,
  };

  test_hci_layer_->Synchronize([this, params] { inquiry_module_->SetScanActivity(params); },
                               hci::OpCode::WRITE_INQUIRY_SCAN_ACTIVITY);
  ASSERT_EQ(params.interval, hci_register_.inquiry_scan_interval);
  ASSERT_EQ(params.window, hci_register_.inquiry_scan_window);
}

TEST_F(InquiryTest, OneShotGeneralInquiry) {
  uint8_t inquiry_length = 128;
  uint8_t num_responses = 100;
  test_hci_layer_->Synchronize(
      [this, inquiry_length, num_responses] { inquiry_module_->StartGeneralInquiry(inquiry_length, num_responses); },
      hci::OpCode::INQUIRY);
  ASSERT_EQ(inquiry_length, hci_register_.inquiry_length);
  ASSERT_EQ(num_responses, hci_register_.num_responses);

  test_hci_layer_->Synchronize([this] { inquiry_module_->StopInquiry(); }, hci::OpCode::INQUIRY_CANCEL);
}

TEST_F(InquiryTest, OneShotLimitedInquiry) {
  test_hci_layer_->Synchronize([this] { inquiry_module_->StartLimitedInquiry(128, 100); }, hci::OpCode::INQUIRY);

  test_hci_layer_->Synchronize([this] { inquiry_module_->StopInquiry(); }, hci::OpCode::INQUIRY_CANCEL);
}

TEST_F(InquiryTest, GeneralPeriodicInquiry) {
  uint8_t inquiry_length = 128;
  uint8_t num_responses = 100;
  uint16_t max_delay = 1100;
  uint16_t min_delay = 200;
  test_hci_layer_->Synchronize(
      [this, inquiry_length, num_responses, max_delay, min_delay] {
        inquiry_module_->StartGeneralPeriodicInquiry(inquiry_length, num_responses, max_delay, min_delay);
      },
      hci::OpCode::PERIODIC_INQUIRY_MODE);
  ASSERT_EQ(inquiry_length, hci_register_.inquiry_length);
  ASSERT_EQ(num_responses, hci_register_.num_responses);
  ASSERT_EQ(max_delay, hci_register_.max_period_length);
  ASSERT_EQ(min_delay, hci_register_.min_period_length);

  test_hci_layer_->Synchronize([this] { inquiry_module_->StopPeriodicInquiry(); },
                               hci::OpCode::EXIT_PERIODIC_INQUIRY_MODE);
}

TEST_F(InquiryTest, LimitedPeriodicInquiry) {
  test_hci_layer_->Synchronize([this] { inquiry_module_->StartLimitedPeriodicInquiry(128, 100, 1100, 200); },
                               hci::OpCode::PERIODIC_INQUIRY_MODE);

  test_hci_layer_->Synchronize([this] { inquiry_module_->StopPeriodicInquiry(); },
                               hci::OpCode::EXIT_PERIODIC_INQUIRY_MODE);
}

TEST_F(InquiryTest, InjectInquiryResult) {
  test_hci_layer_->Synchronize([this] { inquiry_module_->StartGeneralInquiry(128, 100); }, hci::OpCode::INQUIRY);

  WaitForInquiryResult([this] {
    const std::vector<hci::InquiryResult> inquiry_results;
    auto packet = hci::InquiryResultBuilder::Create(inquiry_results);
    test_hci_layer_->InjectInquiryResult(std::move(packet));
  });
  test_hci_layer_->Synchronize([this] { inquiry_module_->StopInquiry(); }, hci::OpCode::INQUIRY_CANCEL);
}

}  // namespace
}  // namespace neighbor
}  // namespace bluetooth
