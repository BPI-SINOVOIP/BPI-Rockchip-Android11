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

#include "neighbor/scan.h"
#include <memory>
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace neighbor {

struct ScanModule::impl {
  impl(ScanModule& module);

  void SetInquiryScan(bool enabled);
  bool IsInquiryEnabled() const;

  void SetPageScan(bool enabled);
  bool IsPageEnabled() const;

  void Start();
  void Stop();

 private:
  ScanModule& module_;

  bool inquiry_scan_enabled_;
  bool page_scan_enabled_;

  void WriteScanEnable();
  void ReadScanEnable(hci::ScanEnable);

  void OnCommandComplete(hci::CommandCompleteView status);

  hci::HciLayer* hci_layer_;
  os::Handler* handler_;
};

const ModuleFactory neighbor::ScanModule::Factory = ModuleFactory([]() { return new neighbor::ScanModule(); });

neighbor::ScanModule::impl::impl(neighbor::ScanModule& module)
    : module_(module), inquiry_scan_enabled_(false), page_scan_enabled_(false) {}

void neighbor::ScanModule::impl::OnCommandComplete(hci::CommandCompleteView view) {
  switch (view.GetCommandOpCode()) {
    case hci::OpCode::READ_SCAN_ENABLE: {
      auto packet = hci::ReadScanEnableCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
      ReadScanEnable(packet.GetScanEnable());
    } break;

    case hci::OpCode::WRITE_SCAN_ENABLE: {
      auto packet = hci::WriteScanEnableCompleteView::Create(view);
      ASSERT(packet.IsValid());
      ASSERT(packet.GetStatus() == hci::ErrorCode::SUCCESS);
    } break;

    default:
      LOG_ERROR("Unhandled command %s", hci::OpCodeText(view.GetCommandOpCode()).c_str());
      break;
  }
}

void neighbor::ScanModule::impl::WriteScanEnable() {
  hci::ScanEnable scan_enable;

  if (inquiry_scan_enabled_ && !page_scan_enabled_) {
    scan_enable = hci::ScanEnable::INQUIRY_SCAN_ONLY;
  } else if (!inquiry_scan_enabled_ && page_scan_enabled_) {
    scan_enable = hci::ScanEnable::PAGE_SCAN_ONLY;
  } else if (inquiry_scan_enabled_ && page_scan_enabled_) {
    scan_enable = hci::ScanEnable::INQUIRY_AND_PAGE_SCAN;
  } else {
    scan_enable = hci::ScanEnable::NO_SCANS;
  }

  {
    std::unique_ptr<hci::WriteScanEnableBuilder> packet = hci::WriteScanEnableBuilder::Create(scan_enable);
    hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce(&impl::OnCommandComplete, common::Unretained(this)),
                               handler_);
  }

  {
    std::unique_ptr<hci::ReadScanEnableBuilder> packet = hci::ReadScanEnableBuilder::Create();
    hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce(&impl::OnCommandComplete, common::Unretained(this)),
                               handler_);
  }
}

void neighbor::ScanModule::impl::ReadScanEnable(hci::ScanEnable scan_enable) {
  switch (scan_enable) {
    case hci::ScanEnable::INQUIRY_SCAN_ONLY:
      inquiry_scan_enabled_ = true;
      page_scan_enabled_ = false;
      break;

    case hci::ScanEnable::PAGE_SCAN_ONLY:
      inquiry_scan_enabled_ = false;
      page_scan_enabled_ = true;
      break;

    case hci::ScanEnable::INQUIRY_AND_PAGE_SCAN:
      inquiry_scan_enabled_ = true;
      page_scan_enabled_ = true;
      break;

    default:
      inquiry_scan_enabled_ = false;
      page_scan_enabled_ = false;
      break;
  }
}

void neighbor::ScanModule::impl::SetInquiryScan(bool enabled) {
  inquiry_scan_enabled_ = enabled;
  WriteScanEnable();
}

void neighbor::ScanModule::impl::SetPageScan(bool enabled) {
  page_scan_enabled_ = enabled;
  WriteScanEnable();
}

bool neighbor::ScanModule::impl::IsInquiryEnabled() const {
  return inquiry_scan_enabled_;
}

bool neighbor::ScanModule::impl::IsPageEnabled() const {
  return page_scan_enabled_;
}

void neighbor::ScanModule::impl::Start() {
  hci_layer_ = module_.GetDependency<hci::HciLayer>();
  handler_ = module_.GetHandler();

  std::unique_ptr<hci::ReadScanEnableBuilder> packet = hci::ReadScanEnableBuilder::Create();
  hci_layer_->EnqueueCommand(std::move(packet), common::BindOnce(&impl::OnCommandComplete, common::Unretained(this)),
                             handler_);
}

void neighbor::ScanModule::impl::Stop() {
  LOG_DEBUG("inquiry scan enabled:%d page scan enabled:%d", inquiry_scan_enabled_, page_scan_enabled_);
}

neighbor::ScanModule::ScanModule() : pimpl_(std::make_unique<impl>(*this)) {}

neighbor::ScanModule::~ScanModule() {
  pimpl_.reset();
}

void neighbor::ScanModule::SetInquiryScan() {
  pimpl_->SetInquiryScan(true);
}

void neighbor::ScanModule::ClearInquiryScan() {
  pimpl_->SetInquiryScan(false);
}

void neighbor::ScanModule::SetPageScan() {
  pimpl_->SetPageScan(true);
}

void neighbor::ScanModule::ClearPageScan() {
  pimpl_->SetPageScan(false);
}

bool neighbor::ScanModule::IsInquiryEnabled() const {
  return pimpl_->IsInquiryEnabled();
}

bool neighbor::ScanModule::IsPageEnabled() const {
  return pimpl_->IsPageEnabled();
}

void neighbor::ScanModule::ListDependencies(ModuleList* list) {
  list->add<hci::HciLayer>();
}

void neighbor::ScanModule::Start() {
  pimpl_->Start();
}

void neighbor::ScanModule::Stop() {
  pimpl_->Stop();
}

}  // namespace neighbor
}  // namespace bluetooth
