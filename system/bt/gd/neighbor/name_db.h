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
#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "common/bind.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "neighbor/name.h"

namespace bluetooth {
namespace neighbor {

using ReadRemoteNameDbCallback = common::OnceCallback<void(hci::Address address, bool success)>;

class NameDbModule : public bluetooth::Module {
 public:
  void ReadRemoteNameRequest(hci::Address address, ReadRemoteNameDbCallback callback, os::Handler* handler);

  bool IsNameCached(hci::Address address) const;
  RemoteName ReadCachedRemoteName(hci::Address address) const;

  static const ModuleFactory Factory;

  NameDbModule();
  ~NameDbModule();

 protected:
  void ListDependencies(ModuleList* list) override;
  void Start() override;
  void Stop() override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(NameDbModule);
};

}  // namespace neighbor
}  // namespace bluetooth
