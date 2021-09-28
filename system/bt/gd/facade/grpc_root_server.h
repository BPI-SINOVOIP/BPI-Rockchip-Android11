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

#include <memory>
#include <string>

#include <grpc++/grpc++.h>

namespace bluetooth {
namespace facade {

class GrpcRootServer {
 public:
  void StartServer(const std::string& address, int grpc_root_server_port, int grpc_port);

  void StopServer();

  void RunGrpcLoop();

 private:
  bool started_ = false;
  std::unique_ptr<::grpc::Server> server_ = nullptr;
};

}  // namespace facade
}  // namespace bluetooth
