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

#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>

#include "module.h"

namespace bluetooth {
namespace shim {

using ConnectionClosedCallback = std::function<void(uint16_t cid, int error_code)>;
using ConnectionCompleteCallback =
    std::function<void(std::string string_address, uint16_t psm, uint16_t cid, bool is_connected)>;
using ReadDataReadyCallback = std::function<void(uint16_t cid, std::vector<const uint8_t> data)>;

using RegisterServicePromise = std::promise<uint16_t>;
using UnregisterServicePromise = std::promise<void>;
using CreateConnectionPromise = std::promise<uint16_t>;

class L2cap : public bluetooth::Module {
 public:
  void RegisterService(uint16_t psm, bool use_ertm, uint16_t mtu, ConnectionCompleteCallback on_complete,
                       RegisterServicePromise register_promise);
  void UnregisterService(uint16_t psm, UnregisterServicePromise unregister_promise);

  void CreateConnection(uint16_t psm, const std::string address_string, ConnectionCompleteCallback on_complete,
                        CreateConnectionPromise create_promise);
  void CloseConnection(uint16_t cid);

  void SetReadDataReadyCallback(uint16_t cid, ReadDataReadyCallback on_data_ready);
  void SetConnectionClosedCallback(uint16_t cid, ConnectionClosedCallback on_closed);

  void Write(uint16_t cid, const uint8_t* data, size_t len);

  void SendLoopbackResponse(std::function<void()>);

  L2cap() = default;
  ~L2cap() = default;

  static const ModuleFactory Factory;

 protected:
  void ListDependencies(ModuleList* list) override;  // Module
  void Start() override;                             // Module
  void Stop() override;                              // Module
  std::string ToString() const override;             // Module

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;
  DISALLOW_COPY_AND_ASSIGN(L2cap);
};

}  // namespace shim
}  // namespace bluetooth
