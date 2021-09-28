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

#pragma once

#include <functional>
#include <vector>

#include <grpc++/grpc++.h>
#include <module.h>

namespace bluetooth {
namespace grpc {

class GrpcFacadeModule;

class GrpcModule : public ::bluetooth::Module {
 public:
  static const ModuleFactory Factory;

  void StartServer(const std::string& address, int port);

  void StopServer();

  void Register(GrpcFacadeModule* facade);

  void Unregister(GrpcFacadeModule* facade);

  // Blocks for incoming gRPC requests
  void RunGrpcLoop();

 protected:
  void ListDependencies(ModuleList* list) override;

  void Start() override;

  void Stop() override;

  std::string ToString() const override;

 private:
  bool started_;
  std::unique_ptr<::grpc::Server> server_ = nullptr;
  std::unique_ptr<::grpc::ServerCompletionQueue> completion_queue_ = nullptr;
  std::vector<GrpcFacadeModule*> facades_;
};

class GrpcFacadeModule : public ::bluetooth::Module {
 friend GrpcModule;
 protected:
  void ListDependencies(ModuleList* list) override;

  void Start() override;

  void Stop() override;

  virtual ::grpc::Service* GetService() const = 0;

  virtual void OnServerStarted() {}

  virtual void OnServerStopped() {}

  std::string ToString() const override;
};

}  // namespace grpc
}  // namespace bluetooth
