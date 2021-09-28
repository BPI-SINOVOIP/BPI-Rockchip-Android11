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

#include "hci/le_scanning_manager.h"

#include <cstdint>
#include <unordered_map>
#include <utility>

#include "common/bidi_queue.h"
#include "common/bind.h"
#include "grpc/grpc_event_queue.h"
#include "hci/facade/le_scanning_manager_facade.grpc.pb.h"
#include "hci/facade/le_scanning_manager_facade.h"
#include "hci/facade/le_scanning_manager_facade.pb.h"
#include "hci/le_report.h"
#include "os/log.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace hci {
namespace facade {

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;
using ::grpc::ServerWriter;
using ::grpc::Status;

class LeScanningManagerFacadeService : public LeScanningManagerFacade::Service, public LeScanningManagerCallbacks {
 public:
  LeScanningManagerFacadeService(LeScanningManager* le_scanning_manager, os::Handler* facade_handler)
      : le_scanning_manager_(le_scanning_manager), facade_handler_(facade_handler) {
    ASSERT(le_scanning_manager_ != nullptr);
    ASSERT(facade_handler_ != nullptr);
  }

  ::grpc::Status StartScan(::grpc::ServerContext* context, const ::google::protobuf::Empty*,
                           ::grpc::ServerWriter<LeReportMsg>* writer) override {
    le_scanning_manager_->StartScan(this);
    return pending_events_.RunLoop(context, writer);
  }

  ::grpc::Status StopScan(::grpc::ServerContext* context, const ::google::protobuf::Empty*,
                          ScanStoppedMsg* response) override {
    std::shared_ptr<std::promise<void>> on_stopped = std::make_shared<std::promise<void>>();
    auto future = on_stopped->get_future();
    le_scanning_manager_->StopScan(
        common::Bind([](std::shared_ptr<std::promise<void>> p) { p->set_value(); }, on_stopped));
    auto result = future.wait_for(std::chrono::milliseconds(1000));
    ASSERT(result == std::future_status::ready);
    return ::grpc::Status::OK;
  }

  void on_advertisements(std::vector<std::shared_ptr<LeReport>> reports) override {
    for (const auto report : reports) {
      switch (report->report_type_) {
        case hci::LeReport::ReportType::ADVERTISING_EVENT: {
          LeReportMsg le_report_msg;
          std::vector<LeAdvertisingReport> advertisements;
          LeAdvertisingReport le_advertising_report;
          le_advertising_report.address_type_ = report->address_type_;
          le_advertising_report.address_ = report->address_;
          le_advertising_report.advertising_data_ = report->gap_data_;
          le_advertising_report.event_type_ = report->advertising_event_type_;
          le_advertising_report.rssi_ = report->rssi_;
          advertisements.push_back(le_advertising_report);

          auto builder = LeAdvertisingReportBuilder::Create(advertisements);
          std::vector<uint8_t> bytes;
          BitInserter bit_inserter(bytes);
          builder->Serialize(bit_inserter);
          le_report_msg.set_event(std::string(bytes.begin(), bytes.end()));
          pending_events_.OnIncomingEvent(std::move(le_report_msg));
        } break;
        case hci::LeReport::ReportType::EXTENDED_ADVERTISING_EVENT: {
          LeReportMsg le_report_msg;
          std::vector<LeExtendedAdvertisingReport> advertisements;
          LeExtendedAdvertisingReport le_extended_advertising_report;
          le_extended_advertising_report.address_ = report->address_;
          le_extended_advertising_report.advertising_data_ = report->gap_data_;
          le_extended_advertising_report.rssi_ = report->rssi_;
          advertisements.push_back(le_extended_advertising_report);

          auto builder = LeExtendedAdvertisingReportBuilder::Create(advertisements);
          std::vector<uint8_t> bytes;
          BitInserter bit_inserter(bytes);
          builder->Serialize(bit_inserter);
          le_report_msg.set_event(std::string(bytes.begin(), bytes.end()));
          pending_events_.OnIncomingEvent(std::move(le_report_msg));
        } break;
        case hci::LeReport::ReportType::DIRECTED_ADVERTISING_EVENT: {
          LeReportMsg le_report_msg;
          std::vector<LeDirectedAdvertisingReport> advertisements;
          LeDirectedAdvertisingReport le_directed_advertising_report;
          le_directed_advertising_report.address_ = report->address_;
          le_directed_advertising_report.direct_address_ = ((DirectedLeReport*)report.get())->direct_address_;
          le_directed_advertising_report.direct_address_type_ = DirectAddressType::RANDOM_DEVICE_ADDRESS;
          le_directed_advertising_report.event_type_ = DirectAdvertisingEventType::ADV_DIRECT_IND;
          le_directed_advertising_report.rssi_ = report->rssi_;
          advertisements.push_back(le_directed_advertising_report);

          auto builder = LeDirectedAdvertisingReportBuilder::Create(advertisements);
          std::vector<uint8_t> bytes;
          BitInserter bit_inserter(bytes);
          builder->Serialize(bit_inserter);
          le_report_msg.set_event(std::string(bytes.begin(), bytes.end()));
          pending_events_.OnIncomingEvent(std::move(le_report_msg));
        } break;
        default:
          LOG_INFO("Skipping unknown report type %d", static_cast<int>(report->report_type_));
      }
    }
  }

  void on_timeout() override {
    LeReportMsg le_report_msg;
    auto builder = LeScanTimeoutBuilder::Create();
    std::vector<uint8_t> bytes;
    BitInserter bit_inserter(bytes);
    builder->Serialize(bit_inserter);
    le_report_msg.set_event(std::string(bytes.begin(), bytes.end()));
    pending_events_.OnIncomingEvent(std::move(le_report_msg));
  }

  os::Handler* Handler() override {
    return facade_handler_;
  }

  LeScanningManager* le_scanning_manager_;
  os::Handler* facade_handler_;
  ::bluetooth::grpc::GrpcEventQueue<LeReportMsg> pending_events_{"LeReports"};
};

void LeScanningManagerFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<hci::LeScanningManager>();
}

void LeScanningManagerFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new LeScanningManagerFacadeService(GetDependency<hci::LeScanningManager>(), GetHandler());
}

void LeScanningManagerFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* LeScanningManagerFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory LeScanningManagerFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new LeScanningManagerFacadeModule(); });

}  // namespace facade
}  // namespace hci
}  // namespace bluetooth
