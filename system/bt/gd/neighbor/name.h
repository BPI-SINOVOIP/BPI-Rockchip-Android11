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

#include <array>
#include <cstdint>
#include <memory>

#include "common/bind.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "module.h"

namespace bluetooth {
namespace neighbor {

using RemoteName = std::array<uint8_t, 248>;
using ReadRemoteNameCallback = common::OnceCallback<void(hci::ErrorCode status, hci::Address address, RemoteName name)>;
using CancelRemoteNameCallback = common::OnceCallback<void(hci::ErrorCode status, hci::Address address)>;

class NameModule : public bluetooth::Module {
 public:
  void ReadRemoteNameRequest(hci::Address address, hci::PageScanRepetitionMode page_scan_repetition_mode,
                             uint16_t clock_offset, hci::ClockOffsetValid clock_offset_valid,
                             ReadRemoteNameCallback on_read_name, os::Handler* handler);
  void CancelRemoteNameRequest(hci::Address address, CancelRemoteNameCallback on_cancel, os::Handler* handler);

  static const ModuleFactory Factory;

  NameModule();
  ~NameModule();

 protected:
  void ListDependencies(ModuleList* list) override;
  void Start() override;
  void Stop() override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(NameModule);
};

}  // namespace neighbor
}  // namespace bluetooth
