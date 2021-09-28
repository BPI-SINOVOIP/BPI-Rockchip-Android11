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

#include "neighbor/facade/facade.h"

#include <memory>

#include "common/bind.h"
#include "grpc/grpc_event_queue.h"
#include "hci/hci_packets.h"
#include "neighbor/facade/facade.grpc.pb.h"

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;

namespace bluetooth {
namespace neighbor {
namespace facade {

class NeighborFacadeService : public NeighborFacade::Service {
 public:
  NeighborFacadeService(ConnectabilityModule* connectability_module, DiscoverabilityModule* discoverability_module,
                        InquiryModule* inquiry_module, NameModule* name_module, PageModule*, ScanModule* scan_module,
                        ::bluetooth::os::Handler* facade_handler)
      : connectability_module_(connectability_module), discoverability_module_(discoverability_module),
        inquiry_module_(inquiry_module), name_module_(name_module), scan_module_(scan_module),
        facade_handler_(facade_handler) {}

  ::grpc::Status SetConnectability(::grpc::ServerContext* context, const ::bluetooth::neighbor::EnableMsg* request,
                                   ::google::protobuf::Empty* response) override {
    if (request->enabled()) {
      connectability_module_->StartConnectability();
    } else {
      connectability_module_->StopConnectability();
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetDiscoverability(::grpc::ServerContext* context,
                                    const ::bluetooth::neighbor::DiscoverabilitiyMsg* request,
                                    ::google::protobuf::Empty* response) override {
    switch (request->mode()) {
      case DiscoverabilityMode::OFF:
        discoverability_module_->StopDiscoverability();
        break;
      case DiscoverabilityMode::LIMITED:
        discoverability_module_->StartLimitedDiscoverability();
        break;
      case DiscoverabilityMode::GENERAL:
        discoverability_module_->StartGeneralDiscoverability();
        break;
      default:
        LOG_ALWAYS_FATAL("Unknown discoverability mode %d", static_cast<int>(request->mode()));
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status SetInquiryMode(::grpc::ServerContext* context, const ::bluetooth::neighbor::InquiryMsg* request,
                                ::grpc::ServerWriter<InquiryResultMsg>* writer) override {
    inquiry_module_->RegisterCallbacks(inquiry_callbacks_);
    switch (request->result_mode()) {
      case ResultMode::STANDARD:
        inquiry_module_->SetStandardInquiryResultMode();
        break;
      case ResultMode::RSSI:
        inquiry_module_->SetInquiryWithRssiResultMode();
        break;
      case ResultMode::EXTENDED:
        inquiry_module_->SetExtendedInquiryResultMode();
        break;
      default:
        LOG_ALWAYS_FATAL("Unknown result mode %d", static_cast<int>(request->result_mode()));
    }
    switch (request->inquiry_mode()) {
      case DiscoverabilityMode::OFF:
        inquiry_module_->StopInquiry();
        break;
      case DiscoverabilityMode::LIMITED:
        inquiry_module_->StartLimitedInquiry(request->length_1_28s(), request->max_results());
        break;
      case DiscoverabilityMode::GENERAL:
        inquiry_module_->StartGeneralInquiry(request->length_1_28s(), request->max_results());
        break;
      default:
        LOG_ALWAYS_FATAL("Unknown discoverability mode %d", static_cast<int>(request->inquiry_mode()));
    }
    return pending_events_.RunLoop(context, writer);
  }

  ::grpc::Status ReadRemoteName(::grpc::ServerContext* context,
                                const ::bluetooth::neighbor::RemoteNameRequestMsg* request,
                                ::google::protobuf::Empty* response) override {
    hci::Address remote;
    ASSERT(hci::Address::FromString(request->address(), remote));
    hci::PageScanRepetitionMode mode;
    switch (request->page_scan_repetition_mode()) {
      case 0:
        mode = hci::PageScanRepetitionMode::R0;
        break;
      case 1:
        mode = hci::PageScanRepetitionMode::R1;
        break;
      case 2:
        mode = hci::PageScanRepetitionMode::R2;
        break;
      default:
        LOG_ALWAYS_FATAL("Unknown PageScanRepetition mode %d", static_cast<int>(request->page_scan_repetition_mode()));
    }
    name_module_->ReadRemoteNameRequest(
        remote, mode, request->clock_offset(),
        (request->clock_offset() != 0 ? hci::ClockOffsetValid::VALID : hci::ClockOffsetValid::INVALID),
        common::Bind(&NeighborFacadeService::on_remote_name, common::Unretained(this)), facade_handler_);
    return ::grpc::Status::OK;
  }

  ::grpc::Status GetRemoteNameEvents(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                     ::grpc::ServerWriter<RemoteNameResponseMsg>* writer) override {
    return pending_remote_names_.RunLoop(context, writer);
  }

  ::grpc::Status EnableInquiryScan(::grpc::ServerContext* context, const ::bluetooth::neighbor::EnableMsg* request,
                                   ::google::protobuf::Empty* response) override {
    if (request->enabled()) {
      scan_module_->SetInquiryScan();
    } else {
      scan_module_->ClearInquiryScan();
    }
    return ::grpc::Status::OK;
  }

  ::grpc::Status EnablePageScan(::grpc::ServerContext* context, const ::bluetooth::neighbor::EnableMsg* request,
                                ::google::protobuf::Empty* response) override {
    if (request->enabled()) {
      scan_module_->SetPageScan();
    } else {
      scan_module_->ClearPageScan();
    }
    return ::grpc::Status::OK;
  }

 private:
  void on_incoming_inquiry_result(hci::EventPacketView view) {
    InquiryResultMsg inquiry_result_msg;
    inquiry_result_msg.set_packet(std::string(view.begin(), view.end()));
    pending_events_.OnIncomingEvent(std::move(inquiry_result_msg));
  }

  void on_incoming_inquiry_complete(hci::ErrorCode status) {
    InquiryResultMsg inquiry_result_msg;
    inquiry_result_msg.set_packet(hci::ErrorCodeText(status));
    pending_events_.OnIncomingEvent(std::move(inquiry_result_msg));
  }

  InquiryCallbacks inquiry_callbacks_{
      .result = [this](hci::InquiryResultView view) { on_incoming_inquiry_result(view); },
      .result_with_rssi = [this](hci::InquiryResultWithRssiView view) { on_incoming_inquiry_result(view); },
      .extended_result = [this](hci::ExtendedInquiryResultView view) { on_incoming_inquiry_result(view); },
      .complete = [this](hci::ErrorCode status) { on_incoming_inquiry_complete(status); }};

  void on_remote_name(hci::ErrorCode status, hci::Address address, std::array<uint8_t, 248> name) {
    RemoteNameResponseMsg response;
    response.set_status(static_cast<int>(status));
    response.set_address(address.ToString());
    response.set_name(name.begin(), name.size());
    pending_remote_names_.OnIncomingEvent(response);
  }

  ConnectabilityModule* connectability_module_;
  DiscoverabilityModule* discoverability_module_;
  InquiryModule* inquiry_module_;
  NameModule* name_module_;
  ScanModule* scan_module_;
  ::bluetooth::os::Handler* facade_handler_;
  ::bluetooth::grpc::GrpcEventQueue<InquiryResultMsg> pending_events_{"InquiryResponses"};
  ::bluetooth::grpc::GrpcEventQueue<RemoteNameResponseMsg> pending_remote_names_{"RemoteNameResponses"};
};

void NeighborFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<ConnectabilityModule>();
  list->add<DiscoverabilityModule>();
  list->add<InquiryModule>();
  list->add<NameModule>();
  list->add<PageModule>();
  list->add<ScanModule>();
}

void NeighborFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new NeighborFacadeService(GetDependency<ConnectabilityModule>(), GetDependency<DiscoverabilityModule>(),
                                       GetDependency<InquiryModule>(), GetDependency<NameModule>(),
                                       GetDependency<PageModule>(), GetDependency<ScanModule>(), GetHandler());
}

void NeighborFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* NeighborFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory NeighborFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new NeighborFacadeModule(); });

}  // namespace facade
}  // namespace neighbor
}  // namespace bluetooth
