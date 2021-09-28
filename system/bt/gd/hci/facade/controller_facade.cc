/*
 * Copyright 2020 The Android Open Source Project
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

#include "hci/facade/controller_facade.h"

#include <condition_variable>
#include <memory>
#include <mutex>

#include "common/bind.h"
#include "common/blocking_queue.h"
#include "grpc/grpc_event_queue.h"
#include "hci/address.h"
#include "hci/controller.h"
#include "hci/facade/controller_facade.grpc.pb.h"
#include "hci/facade/controller_facade.pb.h"

using ::grpc::ServerAsyncResponseWriter;
using ::grpc::ServerAsyncWriter;
using ::grpc::ServerContext;

namespace bluetooth {
namespace hci {
namespace facade {

class ControllerFacadeService : public ControllerFacade::Service {
 public:
  ControllerFacadeService(Controller* controller, ::bluetooth::os::Handler*) : controller_(controller) {}

  ::grpc::Status GetMacAddress(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                               AddressMsg* response) override {
    Address local_address = controller_->GetControllerMacAddress();
    response->set_address(local_address.ToString());
    return ::grpc::Status::OK;
  }

  ::grpc::Status GetLocalName(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                              NameMsg* response) override {
    std::string local_name = controller_->GetControllerLocalName();
    response->set_name(local_name);
    return ::grpc::Status::OK;
  }

  ::grpc::Status WriteLocalName(::grpc::ServerContext* context, const NameMsg* request,
                                ::google::protobuf::Empty* response) override {
    controller_->WriteLocalName(request->name());
    return ::grpc::Status::OK;
  }

  ::grpc::Status GetLocalExtendedFeatures(::grpc::ServerContext* context, const PageNumberMsg* request,
                                          FeaturesMsg* response) override {
    if (request->page_number() > controller_->GetControllerLocalExtendedFeaturesMaxPageNumber()) {
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "Local Extended Features page number out of range");
    }
    response->set_page(controller_->GetControllerLocalExtendedFeatures(request->page_number()));
    return ::grpc::Status::OK;
  }

 private:
  Controller* controller_;
};

void ControllerFacadeModule::ListDependencies(ModuleList* list) {
  ::bluetooth::grpc::GrpcFacadeModule::ListDependencies(list);
  list->add<Controller>();
}

void ControllerFacadeModule::Start() {
  ::bluetooth::grpc::GrpcFacadeModule::Start();
  service_ = new ControllerFacadeService(GetDependency<Controller>(), GetHandler());
}

void ControllerFacadeModule::Stop() {
  delete service_;
  ::bluetooth::grpc::GrpcFacadeModule::Stop();
}

::grpc::Service* ControllerFacadeModule::GetService() const {
  return service_;
}

const ModuleFactory ControllerFacadeModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new ControllerFacadeModule(); });

}  // namespace facade
}  // namespace hci
}  // namespace bluetooth
