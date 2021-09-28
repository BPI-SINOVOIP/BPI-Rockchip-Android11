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
#include <memory>
#include <mutex>
#include <set>

#include "hci/controller.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "hci/le_scanning_interface.h"
#include "hci/le_scanning_manager.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace hci {

constexpr uint16_t kDefaultLeScanWindow = 4800;
constexpr uint16_t kDefaultLeScanInterval = 4800;

const ModuleFactory LeScanningManager::Factory = ModuleFactory([]() { return new LeScanningManager(); });

enum class ScanApiType {
  LE_4_0 = 1,
  ANDROID_HCI = 2,
  LE_5_0 = 3,
};

struct LeScanningManager::impl {
  impl(Module* module) : module_(module), le_scanning_interface_(nullptr) {}

  void start(os::Handler* handler, hci::HciLayer* hci_layer, hci::Controller* controller) {
    module_handler_ = handler;
    hci_layer_ = hci_layer;
    controller_ = controller;
    le_scanning_interface_ = hci_layer_->GetLeScanningInterface(
        common::Bind(&LeScanningManager::impl::handle_scan_results, common::Unretained(this)), module_handler_);
    if (controller_->IsSupported(OpCode::LE_SET_EXTENDED_SCAN_PARAMETERS)) {
      api_type_ = ScanApiType::LE_5_0;
    } else if (controller_->IsSupported(OpCode::LE_EXTENDED_SCAN_PARAMS)) {
      api_type_ = ScanApiType::ANDROID_HCI;
    } else {
      api_type_ = ScanApiType::LE_4_0;
    }
    configure_scan();
  }

  void handle_scan_results(LeMetaEventView event) {
    switch (event.GetSubeventCode()) {
      case hci::SubeventCode::ADVERTISING_REPORT:
        handle_advertising_report<LeAdvertisingReportView, LeAdvertisingReport, LeReport>(
            LeAdvertisingReportView::Create(event));
        break;
      case hci::SubeventCode::DIRECTED_ADVERTISING_REPORT:
        handle_advertising_report<LeDirectedAdvertisingReportView, LeDirectedAdvertisingReport, DirectedLeReport>(
            LeDirectedAdvertisingReportView::Create(event));
        break;
      case hci::SubeventCode::EXTENDED_ADVERTISING_REPORT:
        handle_advertising_report<LeExtendedAdvertisingReportView, LeExtendedAdvertisingReport, ExtendedLeReport>(
            LeExtendedAdvertisingReportView::Create(event));
        break;
      case hci::SubeventCode::SCAN_TIMEOUT:
        if (registered_callback_ != nullptr) {
          registered_callback_->Handler()->Post(
              common::BindOnce(&LeScanningManagerCallbacks::on_timeout, common::Unretained(registered_callback_)));
          registered_callback_ = nullptr;
        }
        break;
      default:
        LOG_ALWAYS_FATAL("Unknown advertising subevent %s", hci::SubeventCodeText(event.GetSubeventCode()).c_str());
    }
  }

  template <class EventType, class ReportStructType, class ReportType>
  void handle_advertising_report(EventType event_view) {
    if (registered_callback_ == nullptr) {
      LOG_INFO("Dropping advertising event (no registered handler)");
      return;
    }
    if (!event_view.IsValid()) {
      LOG_INFO("Dropping invalid advertising event");
      return;
    }
    std::vector<ReportStructType> report_vector = event_view.GetAdvertisingReports();
    if (report_vector.empty()) {
      LOG_INFO("Zero results in advertising event");
      return;
    }
    std::vector<std::shared_ptr<LeReport>> param;
    param.reserve(report_vector.size());
    for (const ReportStructType& report : report_vector) {
      param.push_back(std::shared_ptr<LeReport>(static_cast<LeReport*>(new ReportType(report))));
    }
    registered_callback_->Handler()->Post(common::BindOnce(&LeScanningManagerCallbacks::on_advertisements,
                                                           common::Unretained(registered_callback_), param));
  }

  void configure_scan() {
    std::vector<PhyScanParameters> parameter_vector;
    PhyScanParameters phy_scan_parameters;
    phy_scan_parameters.le_scan_window_ = kDefaultLeScanWindow;
    phy_scan_parameters.le_scan_interval_ = kDefaultLeScanInterval;
    phy_scan_parameters.le_scan_type_ = LeScanType::ACTIVE;
    parameter_vector.push_back(phy_scan_parameters);
    uint8_t phys_in_use = 1;

    switch (api_type_) {
      case ScanApiType::LE_5_0:
        le_scanning_interface_->EnqueueCommand(hci::LeSetExtendedScanParametersBuilder::Create(
                                                   own_address_type_, filter_policy_, phys_in_use, parameter_vector),
                                               common::BindOnce(impl::check_status), module_handler_);
        break;
      case ScanApiType::ANDROID_HCI:
        le_scanning_interface_->EnqueueCommand(
            hci::LeExtendedScanParamsBuilder::Create(LeScanType::ACTIVE, interval_ms_, window_ms_, own_address_type_,
                                                     filter_policy_),
            common::BindOnce(impl::check_status), module_handler_);

        break;
      case ScanApiType::LE_4_0:
        le_scanning_interface_->EnqueueCommand(
            hci::LeSetScanParametersBuilder::Create(LeScanType::ACTIVE, interval_ms_, window_ms_, own_address_type_,
                                                    filter_policy_),
            common::BindOnce(impl::check_status), module_handler_);
        break;
    }
  }

  void start_scan(LeScanningManagerCallbacks* le_scanning_manager_callbacks) {
    registered_callback_ = le_scanning_manager_callbacks;
    switch (api_type_) {
      case ScanApiType::LE_5_0:
        le_scanning_interface_->EnqueueCommand(
            hci::LeSetExtendedScanEnableBuilder::Create(Enable::ENABLED,
                                                        FilterDuplicates::DISABLED /* filter duplicates */, 0, 0),
            common::BindOnce(impl::check_status), module_handler_);
        break;
      case ScanApiType::ANDROID_HCI:
      case ScanApiType::LE_4_0:
        le_scanning_interface_->EnqueueCommand(
            hci::LeSetScanEnableBuilder::Create(Enable::ENABLED, Enable::DISABLED /* filter duplicates */),
            common::BindOnce(impl::check_status), module_handler_);
        break;
    }
  }

  void stop_scan(common::Callback<void()> on_stopped) {
    if (registered_callback_ == nullptr) {
      return;
    }
    registered_callback_->Handler()->Post(std::move(on_stopped));
    switch (api_type_) {
      case ScanApiType::LE_5_0:
        le_scanning_interface_->EnqueueCommand(
            hci::LeSetExtendedScanEnableBuilder::Create(Enable::DISABLED,
                                                        FilterDuplicates::DISABLED /* filter duplicates */, 0, 0),
            common::BindOnce(impl::check_status), module_handler_);
        registered_callback_ = nullptr;
        break;
      case ScanApiType::ANDROID_HCI:
      case ScanApiType::LE_4_0:
        le_scanning_interface_->EnqueueCommand(
            hci::LeSetScanEnableBuilder::Create(Enable::DISABLED, Enable::DISABLED /* filter duplicates */),
            common::BindOnce(impl::check_status), module_handler_);
        registered_callback_ = nullptr;
        break;
    }
  }

  ScanApiType api_type_;

  LeScanningManagerCallbacks* registered_callback_;
  Module* module_;
  os::Handler* module_handler_;
  hci::HciLayer* hci_layer_;
  hci::Controller* controller_;
  hci::LeScanningInterface* le_scanning_interface_;

  uint32_t interval_ms_{1000};
  uint16_t window_ms_{1000};
  AddressType own_address_type_{AddressType::PUBLIC_DEVICE_ADDRESS};
  LeSetScanningFilterPolicy filter_policy_{LeSetScanningFilterPolicy::ACCEPT_ALL};

  static void check_status(CommandCompleteView view) {
    switch (view.GetCommandOpCode()) {
      case (OpCode::LE_SET_SCAN_ENABLE): {
        auto status_view = LeSetScanEnableCompleteView::Create(view);
        ASSERT(status_view.IsValid());
        ASSERT(status_view.GetStatus() == ErrorCode::SUCCESS);
      } break;
      case (OpCode::LE_SET_EXTENDED_SCAN_ENABLE): {
        auto status_view = LeSetExtendedScanEnableCompleteView::Create(view);
        ASSERT(status_view.IsValid());
        ASSERT(status_view.GetStatus() == ErrorCode::SUCCESS);
      } break;
      case (OpCode::LE_SET_SCAN_PARAMETERS): {
        auto status_view = LeSetScanParametersCompleteView::Create(view);
        ASSERT(status_view.IsValid());
        ASSERT(status_view.GetStatus() == ErrorCode::SUCCESS);
      } break;
      case (OpCode::LE_EXTENDED_SCAN_PARAMS): {
        auto status_view = LeExtendedScanParamsCompleteView::Create(view);
        ASSERT(status_view.IsValid());
        ASSERT(status_view.GetStatus() == ErrorCode::SUCCESS);
      } break;
      case (OpCode::LE_SET_EXTENDED_SCAN_PARAMETERS): {
        auto status_view = LeSetExtendedScanParametersCompleteView::Create(view);
        ASSERT(status_view.IsValid());
        ASSERT(status_view.GetStatus() == ErrorCode::SUCCESS);
      } break;
      default:
        LOG_ALWAYS_FATAL("Unhandled event %s", OpCodeText(view.GetCommandOpCode()).c_str());
    }
  }
};

LeScanningManager::LeScanningManager() {
  pimpl_ = std::make_unique<impl>(this);
}

void LeScanningManager::ListDependencies(ModuleList* list) {
  list->add<hci::HciLayer>();
  list->add<hci::Controller>();
}

void LeScanningManager::Start() {
  pimpl_->start(GetHandler(), GetDependency<hci::HciLayer>(), GetDependency<hci::Controller>());
}

void LeScanningManager::Stop() {
  pimpl_.reset();
}

std::string LeScanningManager::ToString() const {
  return "Le Scanning Manager";
}

void LeScanningManager::StartScan(LeScanningManagerCallbacks* callbacks) {
  GetHandler()->Post(common::Bind(&impl::start_scan, common::Unretained(pimpl_.get()), callbacks));
}

void LeScanningManager::StopScan(common::Callback<void()> on_stopped) {
  GetHandler()->Post(common::Bind(&impl::stop_scan, common::Unretained(pimpl_.get()), on_stopped));
}

}  // namespace hci
}  // namespace bluetooth
