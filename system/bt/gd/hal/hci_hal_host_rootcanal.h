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

#include <string>

namespace bluetooth {
namespace hal {

// Singleton object to store runtime configuration for rootcanal
class HciHalHostRootcanalConfig {
 public:
  static HciHalHostRootcanalConfig* Get() {
    static HciHalHostRootcanalConfig instance;
    return &instance;
  }

  // Get the listening TCP port for rootcanal HCI socket
  uint16_t GetPort() {
    return port_;
  }

  // Set the listening TCP port for rootcanal HCI socket
  void SetPort(uint16_t port) {
    port_ = port;
  }

  // Get the server address for rootcanal HCI socket
  std::string GetServerAddress() {
    return server_address_;
  }

  // Set the server address for rootcanal HCI socket
  void SetServerAddress(const std::string& address) {
    server_address_ = address;
  }

 private:
  HciHalHostRootcanalConfig() = default;
  uint16_t port_ = 6402;                      // Default server TCP port
  std::string server_address_ = "127.0.0.1";  // Default server address
};

}  // namespace hal
}  // namespace bluetooth
