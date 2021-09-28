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

#include "grpc/grpc_module.h"

#include "os/log.h"

using ::grpc::Server;
using ::grpc::ServerBuilder;

namespace bluetooth {
namespace grpc {

void GrpcModule::ListDependencies(ModuleList* list) {
}

void GrpcModule::Start() {
  ASSERT(!started_);
}

void GrpcModule::Stop() {
  ASSERT(!started_);
}

void GrpcModule::StartServer(const std::string& address, int port) {
  ASSERT(!started_);
  started_ = true;

  std::string listening_port = address + ":" + std::to_string(port);
  ServerBuilder builder;

  for (const auto& facade : facades_) {
    builder.RegisterService(facade->GetService());
  }

  builder.AddListeningPort(listening_port, ::grpc::InsecureServerCredentials());
  completion_queue_ = builder.AddCompletionQueue();
  server_ = builder.BuildAndStart();
  ASSERT(server_ != nullptr);

  for (const auto& facade : facades_) {
    facade->OnServerStarted();
  }
}

void GrpcModule::StopServer() {
  ASSERT(started_);

  server_->Shutdown();
  completion_queue_->Shutdown();

  for (const auto& facade : facades_) {
    facade->OnServerStopped();
  }

  started_ = false;
}

void GrpcModule::Register(GrpcFacadeModule* facade) {
  ASSERT(!started_);

  facades_.push_back(facade);
}

void GrpcModule::Unregister(GrpcFacadeModule* facade) {
  ASSERT(!started_);

  for (auto it = facades_.begin(); it != facades_.end(); it++) {
    if (*it == facade) {
      facades_.erase(it);
      return;
    }
  }

  ASSERT(false);
}

void GrpcModule::RunGrpcLoop() {
  void* tag;
  bool ok;
  while (true) {
    if (!completion_queue_->Next(&tag, &ok)) {
      LOG_INFO("gRPC is shutdown");
      break;
    }
  }
}

std::string GrpcModule::ToString() const {
  return "Grpc Module";
}

const ::bluetooth::ModuleFactory GrpcModule::Factory = ::bluetooth::ModuleFactory([]() {
  return new GrpcModule();
});


void GrpcFacadeModule::ListDependencies(ModuleList* list) {
  list->add<GrpcModule>();
}

void GrpcFacadeModule::Start() {
  GetDependency<GrpcModule>()->Register(this);
}

void GrpcFacadeModule::Stop() {
  GetDependency<GrpcModule>()->Unregister(this);
}

std::string GrpcFacadeModule::ToString() const {
  return "Grpc Facade Module";
}

}  // namespace grpc
}  // namespace bluetooth
