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

#include "stack_manager.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>

#include "facade/grpc_root_server.h"
#include "grpc/grpc_module.h"
#include "hal/hci_hal.h"
#include "hal/hci_hal_host_rootcanal.h"
#include "hal/snoop_logger.h"

using ::bluetooth::hal::HciHalHostRootcanalConfig;
using ::bluetooth::StackManager;
using ::bluetooth::grpc::GrpcModule;
using ::bluetooth::ModuleList;
using ::bluetooth::os::Thread;

namespace {
::bluetooth::facade::GrpcRootServer grpc_root_server;

void interrupt_handler(int) {
  grpc_root_server.StopServer();
}
}  // namespace

// The entry point for the binary with libbluetooth + facades
int main(int argc, const char** argv) {
  int root_server_port = 8897;
  int grpc_port = 8899;
  int signal_port = 8895;

  const std::string arg_grpc_root_server_port = "--root-server-port=";
  const std::string arg_grpc_server_port = "--grpc-port=";
  const std::string arg_rootcanal_port = "--rootcanal-port=";
  const std::string arg_signal_port = "--signal-port=";
  const std::string arg_btsnoop_path = "--btsnoop=";
  std::string btsnoop_path;
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg.find(arg_grpc_root_server_port) == 0) {
      auto port_number = arg.substr(arg_grpc_root_server_port.size());
      root_server_port = std::stoi(port_number);
    }
    if (arg.find(arg_grpc_server_port) == 0) {
      auto port_number = arg.substr(arg_grpc_server_port.size());
      grpc_port = std::stoi(port_number);
    }
    if (arg.find(arg_rootcanal_port) == 0) {
      auto port_number = arg.substr(arg_rootcanal_port.size());
      HciHalHostRootcanalConfig::Get()->SetPort(std::stoi(port_number));
    }
    if (arg.find(arg_btsnoop_path) == 0) {
      btsnoop_path = arg.substr(arg_btsnoop_path.size());
      ::bluetooth::hal::SnoopLogger::SetFilePath(btsnoop_path);
    }
    if (arg.find(arg_signal_port) == 0) {
      auto port_number = arg.substr(arg_signal_port.size());
      signal_port = std::stoi(port_number);
    }
  }

  signal(SIGINT, interrupt_handler);
  grpc_root_server.StartServer("0.0.0.0", root_server_port, grpc_port);
  int tester_signal_socket = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(signal_port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  connect(tester_signal_socket, (sockaddr*)&addr, sizeof(addr));
  close(tester_signal_socket);
  auto wait_thread = std::thread([] { grpc_root_server.RunGrpcLoop(); });
  wait_thread.join();

  return 0;
}
