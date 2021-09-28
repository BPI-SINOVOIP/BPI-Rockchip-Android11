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
#define LOG_TAG "bt_gd_neigh"

#include "neighbor/inquiry.h"

#include <memory>

#include "common/bind.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace neighbor {

static constexpr uint8_t kGeneralInquiryAccessCode = 0x33;
static constexpr uint8_t kLimitedInquiryAccessCode = 0x00;

struct InquiryModule::impl {
  void RegisterCallbacks(InquiryCallbacks inquiry_callbacks);
  void UnregisterCallbacks();

  void StartOneShotInquiry(bool limited, InquiryLength inquiry_length, NumResponses num_responses);
  void StopOneShotInquiry();

  void StartPeriodicInquiry(bool limited, InquiryLength inquiry_length, NumResponses num_responses,
                            PeriodLength max_delay, PeriodLength min_delay);
  void StopPeriodicInquiry();

  void SetScanActivity(ScanParameters params);

  void SetScanType(hci::InquiryScanType scan_type);

  void SetInquiryMode(hci::InquiryMode mode);

  void Start();
  void Stop();

  bool HasCallbacks() const;

  impl(InquiryModule& inquiry_module);

 private:
  InquiryCallbacks inquiry_callbacks_;

  InquiryModule& module_;

  bool active_general_one_shot_{false};
  bool active_limited_one_shot_{false};
  bool active_general_periodic_{false};
  bool active_limited_periodic_{false};

  ScanParameters inquiry_scan_;
  hci::InquiryMode inquiry_mode_;
  hci::InquiryScanType inquiry_scan_type_;
  int8_t inquiry_response_tx_power_;

  bool IsInquiryActive() const;

  void EnqueueCommandComplete(std::unique_ptr<hci::CommandPacketBuilder> command);
  void EnqueueCommandStatus(std::unique_ptr<hci::CommandPacketBuilder> command);
  void OnCommandComplete(hci::CommandCompleteView view);
  void OnCommandStatus(hci::CommandStatusView status);

  void EnqueueCommandCompleteSync(std::unique_ptr<hci::CommandPacketBuilder> command);
  void OnCommandCompleteSync(hci::CommandCompleteView view);

  void OnEvent(hci::EventPacketView view);

  std::promise<void>* command_sync_{nullptr};

  hci::HciLayer* hci_layer_;
  os::Handler* handler_;
};

const ModuleFactory neighbor::InquiryModule::Factory = ModuleFactory([]() { return new neighbor::InquiryModule(); });

neighbor::InquiryModule::impl::impl(neighbor::InquiryModule& module) : module_(module) {}

void neighbor::InquiryModule::impl::OnCommandCompleteSync(hci::CommandCompleteView view) {
  OnCommandComplete(view);
  ASSERT(command_sync_ != nullptr);
  command_sync_->set_value();
}

void neighbor::InquiryModule::impl::OnCommandComplete(hci::CommandCompleteView view) {
  switch (view.GetCommandOpCode()) {
    case hci::OpCode::INQUIRY_CANCEL: {
      auto packet = hci::InquiryCancelCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::PERIODIC_INQUIRY_MODE: {
      auto packet = hci::PeriodicInquiryModeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::EXIT_PERIODIC_INQUIRY_MODE: {
      auto packet = hci::ExitPeriodicInquiryModeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::WRITE_INQUIRY_MODE: {
      auto packet = hci::WriteInquiryModeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::READ_INQUIRY_MODE: {
      auto packet = hci::ReadInquiryModeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      inquiry_mode_ = packet.GetInquiryMode();
    } break;

    case hci::OpCode::READ_INQUIRY_RESPONSE_TRANSMIT_POWER_LEVEL: {
      auto packet = hci::ReadInquiryResponseTransmitPowerLevelCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      inquiry_response_tx_power_ = packet.GetTxPower();
    } break;

    case hci::OpCode::WRITE_INQUIRY_SCAN_ACTIVITY: {
      auto packet = hci::WriteInquiryScanActivityCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::READ_INQUIRY_SCAN_ACTIVITY: {
      auto packet = hci::ReadInquiryScanActivityCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      inquiry_scan_.interval = packet.GetInquiryScanInterval();
      inquiry_scan_.window = packet.GetInquiryScanWindow();
    } break;

    case hci::OpCode::WRITE_INQUIRY_SCAN_TYPE: {
      auto packet = hci::WriteInquiryScanTypeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    case hci::OpCode::READ_INQUIRY_SCAN_TYPE: {
      auto packet = hci::ReadInquiryScanTypeCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      inquiry_scan_type_ = packet.GetInquiryScanType();
    } break;

    default:
      LOG_WARN("Unhandled command:%s", hci::OpCodeText(view.GetCommandOpCode()).c_str());
      break;
  }
}

void neighbor::InquiryModule::impl::OnCommandStatus(hci::CommandStatusView status) {
  ASSERT(status.GetStatus() == hci::ErrorCode::SUCCESS);

  switch (status.GetCommandOpCode()) {
    case hci::OpCode::INQUIRY: {
      auto packet = hci::InquiryStatusView::Create(status);
      ASSERT(packet.IsValid());
      if (active_limited_one_shot_ || active_general_one_shot_) {
        LOG_DEBUG("Inquiry started lap: %s", active_limited_one_shot_ ? "Limited" : "General");
      }
    } break;

    default:
      LOG_WARN("Unhandled command:%s", hci::OpCodeText(status.GetCommandOpCode()).c_str());
      break;
  }
}

void neighbor::InquiryModule::impl::OnEvent(hci::EventPacketView view) {
  switch (view.GetEventCode()) {
    case hci::EventCode::INQUIRY_COMPLETE: {
      auto packet = hci::InquiryCompleteView::Create(view);
      ASSERT(packet.IsValid());
      LOG_DEBUG("inquiry complete");
      active_limited_one_shot_ = false;
      active_general_one_shot_ = false;
      inquiry_callbacks_.complete(packet.GetStatus());
    } break;

    case hci::EventCode::INQUIRY_RESULT: {
      auto packet = hci::InquiryResultView::Create(view);
      ASSERT(packet.IsValid());
      LOG_DEBUG("Inquiry result size:%zd num_responses:%zu", packet.size(), packet.GetInquiryResults().size());
      inquiry_callbacks_.result(packet);
    } break;

    case hci::EventCode::INQUIRY_RESULT_WITH_RSSI: {
      auto packet = hci::InquiryResultWithRssiView::Create(view);
      ASSERT(packet.IsValid());
      LOG_DEBUG("Inquiry result with rssi num_responses:%zu", packet.GetInquiryResults().size());
      inquiry_callbacks_.result_with_rssi(packet);
    } break;

    case hci::EventCode::EXTENDED_INQUIRY_RESULT: {
      auto packet = hci::ExtendedInquiryResultView::Create(view);
      ASSERT(packet.IsValid());
      LOG_DEBUG("Extended inquiry result addr:%s repetition_mode:%s cod:%s clock_offset:%d rssi:%hhd",
                packet.GetAddress().ToString().c_str(),
                hci::PageScanRepetitionModeText(packet.GetPageScanRepetitionMode()).c_str(),
                packet.GetClassOfDevice().ToString().c_str(), packet.GetClockOffset(), packet.GetRssi());
      inquiry_callbacks_.extended_result(packet);
    } break;

    default:
      LOG_ERROR("Unhandled event:%s", hci::EventCodeText(view.GetEventCode()).c_str());
      break;
  }
}

/**
 * impl
 */
void neighbor::InquiryModule::impl::RegisterCallbacks(InquiryCallbacks callbacks) {
  inquiry_callbacks_ = callbacks;

  hci_layer_->RegisterEventHandler(hci::EventCode::INQUIRY_RESULT,
                                   common::Bind(&InquiryModule::impl::OnEvent, common::Unretained(this)), handler_);
  hci_layer_->RegisterEventHandler(hci::EventCode::INQUIRY_RESULT_WITH_RSSI,
                                   common::Bind(&InquiryModule::impl::OnEvent, common::Unretained(this)), handler_);
  hci_layer_->RegisterEventHandler(hci::EventCode::EXTENDED_INQUIRY_RESULT,
                                   common::Bind(&InquiryModule::impl::OnEvent, common::Unretained(this)), handler_);
  hci_layer_->RegisterEventHandler(hci::EventCode::INQUIRY_COMPLETE,
                                   common::Bind(&InquiryModule::impl::OnEvent, common::Unretained(this)), handler_);
}

void neighbor::InquiryModule::impl::UnregisterCallbacks() {
  hci_layer_->UnregisterEventHandler(hci::EventCode::INQUIRY_COMPLETE);
  hci_layer_->UnregisterEventHandler(hci::EventCode::EXTENDED_INQUIRY_RESULT);
  hci_layer_->UnregisterEventHandler(hci::EventCode::INQUIRY_RESULT_WITH_RSSI);
  hci_layer_->UnregisterEventHandler(hci::EventCode::INQUIRY_RESULT);

  inquiry_callbacks_ = {nullptr, nullptr, nullptr, nullptr};
}

void neighbor::InquiryModule::impl::EnqueueCommandComplete(std::unique_ptr<hci::CommandPacketBuilder> command) {
  hci_layer_->EnqueueCommand(std::move(command), common::BindOnce(&impl::OnCommandComplete, common::Unretained(this)),
                             handler_);
}

void neighbor::InquiryModule::impl::EnqueueCommandStatus(std::unique_ptr<hci::CommandPacketBuilder> command) {
  hci_layer_->EnqueueCommand(std::move(command), common::BindOnce(&impl::OnCommandStatus, common::Unretained(this)),
                             handler_);
}

void neighbor::InquiryModule::impl::EnqueueCommandCompleteSync(std::unique_ptr<hci::CommandPacketBuilder> command) {
  ASSERT(command_sync_ == nullptr);
  command_sync_ = new std::promise<void>();
  auto command_received = command_sync_->get_future();
  hci_layer_->EnqueueCommand(std::move(command),
                             common::BindOnce(&impl::OnCommandCompleteSync, common::Unretained(this)), handler_);
  command_received.wait();
  delete command_sync_;
  command_sync_ = nullptr;
}

void neighbor::InquiryModule::impl::StartOneShotInquiry(bool limited, InquiryLength inquiry_length,
                                                        NumResponses num_responses) {
  ASSERT(HasCallbacks());
  ASSERT(!IsInquiryActive());
  hci::Lap lap;
  if (limited) {
    active_limited_one_shot_ = true;
    lap.lap_ = kLimitedInquiryAccessCode;
  } else {
    active_general_one_shot_ = true;
    lap.lap_ = kGeneralInquiryAccessCode;
  }
  EnqueueCommandStatus(hci::InquiryBuilder::Create(lap, inquiry_length, num_responses));
}

void neighbor::InquiryModule::impl::StopOneShotInquiry() {
  ASSERT(active_general_one_shot_ || active_limited_one_shot_);
  active_general_one_shot_ = false;
  active_limited_one_shot_ = false;
  EnqueueCommandComplete(hci::InquiryCancelBuilder::Create());
}

void neighbor::InquiryModule::impl::StartPeriodicInquiry(bool limited, InquiryLength inquiry_length,
                                                         NumResponses num_responses, PeriodLength max_delay,
                                                         PeriodLength min_delay) {
  ASSERT(HasCallbacks());
  ASSERT(!IsInquiryActive());
  hci::Lap lap;
  if (limited) {
    active_limited_periodic_ = true;
    lap.lap_ = kLimitedInquiryAccessCode;
  } else {
    active_general_periodic_ = true;
    lap.lap_ = kGeneralInquiryAccessCode;
  }
  EnqueueCommandComplete(
      hci::PeriodicInquiryModeBuilder::Create(max_delay, min_delay, lap, inquiry_length, num_responses));
}

void neighbor::InquiryModule::impl::StopPeriodicInquiry() {
  ASSERT(active_general_periodic_ || active_limited_periodic_);
  active_general_periodic_ = false;
  active_limited_periodic_ = false;
  EnqueueCommandComplete(hci::ExitPeriodicInquiryModeBuilder::Create());
}

bool neighbor::InquiryModule::impl::IsInquiryActive() const {
  return active_general_one_shot_ || active_limited_one_shot_ || active_limited_periodic_ || active_general_periodic_;
}

void neighbor::InquiryModule::impl::Start() {
  hci_layer_ = module_.GetDependency<hci::HciLayer>();
  handler_ = module_.GetHandler();

  EnqueueCommandComplete(hci::ReadInquiryResponseTransmitPowerLevelBuilder::Create());
  EnqueueCommandComplete(hci::ReadInquiryScanActivityBuilder::Create());
  EnqueueCommandComplete(hci::ReadInquiryScanTypeBuilder::Create());
  EnqueueCommandCompleteSync(hci::ReadInquiryModeBuilder::Create());

  LOG_DEBUG("Started inquiry module");
}

void neighbor::InquiryModule::impl::Stop() {
  LOG_INFO("Inquiry scan interval:%hu window:%hu", inquiry_scan_.interval, inquiry_scan_.window);
  LOG_INFO("Inquiry mode:%s scan_type:%s", hci::InquiryModeText(inquiry_mode_).c_str(),
           hci::InquiryScanTypeText(inquiry_scan_type_).c_str());
  LOG_INFO("Inquiry response tx power:%hhd", inquiry_response_tx_power_);
  LOG_DEBUG("Stopped inquiry module");
}

void neighbor::InquiryModule::impl::SetInquiryMode(hci::InquiryMode mode) {
  EnqueueCommandComplete(hci::WriteInquiryModeBuilder::Create(mode));
  inquiry_mode_ = mode;
  LOG_DEBUG("Set inquiry mode:%s", hci::InquiryModeText(mode).c_str());
}

void neighbor::InquiryModule::impl::SetScanActivity(ScanParameters params) {
  EnqueueCommandComplete(hci::WriteInquiryScanActivityBuilder::Create(params.interval, params.window));
  inquiry_scan_ = params;
  LOG_DEBUG("Set scan activity interval:0x%x/%.02fms window:0x%x/%.02fms", params.interval,
            ScanIntervalTimeMs(params.interval), params.window, ScanWindowTimeMs(params.window));
}

void neighbor::InquiryModule::impl::SetScanType(hci::InquiryScanType scan_type) {
  EnqueueCommandComplete(hci::WriteInquiryScanTypeBuilder::Create(scan_type));
  LOG_DEBUG("Set scan type:%s", hci::InquiryScanTypeText(scan_type).c_str());
}

bool neighbor::InquiryModule::impl::HasCallbacks() const {
  return inquiry_callbacks_.result != nullptr && inquiry_callbacks_.result_with_rssi != nullptr &&
         inquiry_callbacks_.extended_result != nullptr && inquiry_callbacks_.complete != nullptr;
}

/**
 * General API here
 */
neighbor::InquiryModule::InquiryModule() : pimpl_(std::make_unique<impl>(*this)) {}

neighbor::InquiryModule::~InquiryModule() {
  pimpl_.reset();
}

void neighbor::InquiryModule::RegisterCallbacks(InquiryCallbacks callbacks) {
  pimpl_->RegisterCallbacks(callbacks);
}

void neighbor::InquiryModule::UnregisterCallbacks() {
  pimpl_->UnregisterCallbacks();
}

void neighbor::InquiryModule::StartGeneralInquiry(InquiryLength inquiry_length, NumResponses num_responses) {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::StartOneShotInquiry,
                                      common::Unretained(pimpl_.get()), false, inquiry_length, num_responses));
}

void neighbor::InquiryModule::StartLimitedInquiry(InquiryLength inquiry_length, NumResponses num_responses) {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::StartOneShotInquiry,
                                      common::Unretained(pimpl_.get()), true, inquiry_length, num_responses));
}

void neighbor::InquiryModule::StopInquiry() {
  GetHandler()->Post(
      common::BindOnce(&neighbor::InquiryModule::impl::StopOneShotInquiry, common::Unretained(pimpl_.get())));
}

void neighbor::InquiryModule::StartGeneralPeriodicInquiry(InquiryLength inquiry_length, NumResponses num_responses,
                                                          PeriodLength max_delay, PeriodLength min_delay) {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::StartPeriodicInquiry,
                                      common::Unretained(pimpl_.get()), false, inquiry_length, num_responses, max_delay,
                                      min_delay));
}

void neighbor::InquiryModule::StartLimitedPeriodicInquiry(InquiryLength inquiry_length, NumResponses num_responses,
                                                          PeriodLength max_delay, PeriodLength min_delay) {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::StartPeriodicInquiry,
                                      common::Unretained(pimpl_.get()), true, inquiry_length, num_responses, max_delay,
                                      min_delay));
}

void neighbor::InquiryModule::StopPeriodicInquiry() {
  GetHandler()->Post(
      common::BindOnce(&neighbor::InquiryModule::impl::StopPeriodicInquiry, common::Unretained(pimpl_.get())));
}

void neighbor::InquiryModule::SetScanActivity(ScanParameters params) {
  GetHandler()->Post(
      common::BindOnce(&neighbor::InquiryModule::impl::SetScanActivity, common::Unretained(pimpl_.get()), params));
}

void neighbor::InquiryModule::SetInterlacedScan() {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::SetScanType, common::Unretained(pimpl_.get()),
                                      hci::InquiryScanType::INTERLACED));
}

void neighbor::InquiryModule::SetStandardScan() {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::SetScanType, common::Unretained(pimpl_.get()),
                                      hci::InquiryScanType::STANDARD));
}

void neighbor::InquiryModule::SetStandardInquiryResultMode() {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::SetInquiryMode, common::Unretained(pimpl_.get()),
                                      hci::InquiryMode::STANDARD));
}

void neighbor::InquiryModule::SetInquiryWithRssiResultMode() {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::SetInquiryMode, common::Unretained(pimpl_.get()),
                                      hci::InquiryMode::RSSI));
}

void neighbor::InquiryModule::SetExtendedInquiryResultMode() {
  GetHandler()->Post(common::BindOnce(&neighbor::InquiryModule::impl::SetInquiryMode, common::Unretained(pimpl_.get()),
                                      hci::InquiryMode::RSSI_OR_EXTENDED));
}

/**
 * Module methods here
 */
void neighbor::InquiryModule::ListDependencies(ModuleList* list) {
  list->add<hci::HciLayer>();
}

void neighbor::InquiryModule::Start() {
  pimpl_->Start();
}

void neighbor::InquiryModule::Stop() {
  pimpl_->Stop();
}

}  // namespace neighbor
}  // namespace bluetooth
