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

#include "facade/read_only_property_server.h"
#include "hci/controller.h"

namespace bluetooth {
namespace facade {

class ReadOnlyPropertyService : public ReadOnlyProperty::Service {
 public:
  ReadOnlyPropertyService(hci::Controller* controller) : controller_(controller) {}
  ::grpc::Status ReadLocalAddress(::grpc::ServerContext* context, const ::google::protobuf::Empty* request,
                                  ::bluetooth::facade::BluetoothAddress* response) override {
    auto address = controller_->GetControllerMacAddress().ToString();
    response->set_address(address);
    return ::grpc::Status::OK;
  }

 private:
  hci::Controller* controller_;
};

void ReadOnlyPropertyServerModule::ListDependencies(ModuleList* list) {
  GrpcFacadeModule::ListDependencies(list);
  list->add<hci::Controller>();
}
void ReadOnlyPropertyServerModule::Start() {
  GrpcFacadeModule::Start();
  service_ = std::make_unique<ReadOnlyPropertyService>(GetDependency<hci::Controller>());
}
void ReadOnlyPropertyServerModule::Stop() {
  service_.reset();
  GrpcFacadeModule::Stop();
}
::grpc::Service* ReadOnlyPropertyServerModule::GetService() const {
  return service_.get();
}

const ModuleFactory ReadOnlyPropertyServerModule::Factory =
    ::bluetooth::ModuleFactory([]() { return new ReadOnlyPropertyServerModule(); });

}  // namespace facade
}  // namespace bluetooth
