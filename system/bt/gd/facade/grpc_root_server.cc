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

#include "facade/grpc_root_server.h"

#include <string>

#include "facade/read_only_property_server.h"
#include "facade/rootservice.grpc.pb.h"
#include "grpc/grpc_module.h"
#include "hal/facade.h"
#include "hci/facade/acl_manager_facade.h"
#include "hci/facade/controller_facade.h"
#include "hci/facade/facade.h"
#include "hci/facade/le_acl_manager_facade.h"
#include "hci/facade/le_advertising_manager_facade.h"
#include "hci/facade/le_scanning_manager_facade.h"
#include "hci/hci_layer.h"
#include "hci/le_advertising_manager.h"
#include "hci/le_scanning_manager.h"
#include "l2cap/classic/facade.h"
#include "neighbor/connectability.h"
#include "neighbor/discoverability.h"
#include "neighbor/facade/facade.h"
#include "neighbor/inquiry.h"
#include "neighbor/name.h"
#include "neighbor/page.h"
#include "os/log.h"
#include "os/thread.h"
#include "security/facade.h"
#include "security/security_module.h"
#include "shim/dumpsys.h"
#include "shim/l2cap.h"
#include "stack_manager.h"
#include "storage/legacy.h"

namespace bluetooth {
namespace facade {

using ::bluetooth::grpc::GrpcModule;
using ::bluetooth::os::Thread;

namespace {
class RootFacadeService : public ::bluetooth::facade::RootFacade::Service {
 public:
  RootFacadeService(int grpc_port) : grpc_port_(grpc_port) {}

  ::grpc::Status StartStack(::grpc::ServerContext* context, const ::bluetooth::facade::StartStackRequest* request,
                            ::bluetooth::facade::StartStackResponse* response) override {
    if (is_running_) {
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "stack is running");
    }

    ModuleList modules;
    modules.add<::bluetooth::grpc::GrpcModule>();

    BluetoothModule module_under_test = request->module_under_test();
    switch (module_under_test) {
      case BluetoothModule::HAL:
        modules.add<::bluetooth::hal::HciHalFacadeModule>();
        break;
      case BluetoothModule::HCI:
        modules.add<::bluetooth::facade::ReadOnlyPropertyServerModule>();
        modules.add<::bluetooth::hci::facade::HciLayerFacadeModule>();
        break;
      case BluetoothModule::HCI_INTERFACES:
        modules.add<::bluetooth::facade::ReadOnlyPropertyServerModule>();
        modules.add<::bluetooth::hci::facade::HciLayerFacadeModule>();
        modules.add<::bluetooth::hci::facade::AclManagerFacadeModule>();
        modules.add<::bluetooth::hci::facade::ControllerFacadeModule>();
        modules.add<::bluetooth::hci::facade::LeAclManagerFacadeModule>();
        modules.add<::bluetooth::hci::facade::LeAdvertisingManagerFacadeModule>();
        modules.add<::bluetooth::hci::facade::LeScanningManagerFacadeModule>();
        modules.add<::bluetooth::neighbor::facade::NeighborFacadeModule>();
        break;
      case BluetoothModule::L2CAP:
        modules.add<::bluetooth::hci::facade::ControllerFacadeModule>();
        modules.add<::bluetooth::neighbor::facade::NeighborFacadeModule>();
        modules.add<::bluetooth::facade::ReadOnlyPropertyServerModule>();
        modules.add<::bluetooth::l2cap::classic::L2capClassicModuleFacadeModule>();
        modules.add<::bluetooth::hci::facade::HciLayerFacadeModule>();
        break;
      case BluetoothModule::SECURITY:
        modules.add<::bluetooth::facade::ReadOnlyPropertyServerModule>();
        modules.add<::bluetooth::hci::facade::ControllerFacadeModule>();
        modules.add<::bluetooth::security::SecurityModuleFacadeModule>();
        modules.add<::bluetooth::neighbor::facade::NeighborFacadeModule>();
        modules.add<::bluetooth::l2cap::classic::L2capClassicModuleFacadeModule>();
        modules.add<::bluetooth::hci::facade::HciLayerFacadeModule>();
        modules.add<::bluetooth::hci::facade::ControllerFacadeModule>();
        modules.add<::bluetooth::hci::facade::LeAdvertisingManagerFacadeModule>();
        modules.add<::bluetooth::hci::facade::LeScanningManagerFacadeModule>();
        break;
      case BluetoothModule::SHIM:
        modules.add<::bluetooth::neighbor::ConnectabilityModule>();
        modules.add<::bluetooth::neighbor::DiscoverabilityModule>();
        modules.add<::bluetooth::neighbor::InquiryModule>();
        modules.add<::bluetooth::neighbor::NameModule>();
        modules.add<::bluetooth::shim::Dumpsys>();
        modules.add<::bluetooth::shim::L2cap>();
        modules.add<::bluetooth::neighbor::PageModule>();
        modules.add<::bluetooth::hci::HciLayer>();
        modules.add<::bluetooth::hci::LeAdvertisingManager>();
        modules.add<::bluetooth::hci::LeScanningManager>();
        modules.add<::bluetooth::security::SecurityModule>();
        modules.add<::bluetooth::storage::LegacyModule>();
        break;
      default:
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "invalid module under test");
    }

    stack_thread_ = new Thread("stack_thread", Thread::Priority::NORMAL);
    stack_manager_.StartUp(&modules, stack_thread_);

    GrpcModule* grpc_module = stack_manager_.GetInstance<GrpcModule>();
    grpc_module->StartServer("0.0.0.0", grpc_port_);

    grpc_loop_thread_ = new std::thread([grpc_module] { grpc_module->RunGrpcLoop(); });
    is_running_ = true;

    return ::grpc::Status::OK;
  }

  ::grpc::Status StopStack(::grpc::ServerContext* context, const ::bluetooth::facade::StopStackRequest* request,
                           ::bluetooth::facade::StopStackResponse* response) override {
    if (!is_running_) {
      return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "stack is not running");
    }

    stack_manager_.GetInstance<GrpcModule>()->StopServer();
    grpc_loop_thread_->join();
    delete grpc_loop_thread_;

    stack_manager_.ShutDown();
    delete stack_thread_;
    is_running_ = false;
    return ::grpc::Status::OK;
  }

 private:
  Thread* stack_thread_ = nullptr;
  bool is_running_ = false;
  std::thread* grpc_loop_thread_ = nullptr;
  StackManager stack_manager_;
  int grpc_port_ = 8898;
};

RootFacadeService* root_facade_service;
}  // namespace

void GrpcRootServer::StartServer(const std::string& address, int grpc_root_server_port, int grpc_port) {
  ASSERT(!started_);
  started_ = true;

  std::string listening_port = address + ":" + std::to_string(grpc_root_server_port);
  ::grpc::ServerBuilder builder;
  root_facade_service = new RootFacadeService(grpc_port);
  builder.RegisterService(root_facade_service);
  builder.AddListeningPort(listening_port, ::grpc::InsecureServerCredentials());
  server_ = builder.BuildAndStart();

  ASSERT(server_ != nullptr);
}

void GrpcRootServer::StopServer() {
  ASSERT(started_);
  server_->Shutdown();
  started_ = false;
  server_.reset();
  delete root_facade_service;
}

void GrpcRootServer::RunGrpcLoop() {
  ASSERT(started_);
  server_->Wait();
}

}  // namespace facade
}  // namespace bluetooth
