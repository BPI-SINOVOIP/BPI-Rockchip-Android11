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

#include <stdbool.h>
#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <string>

#include "common/bind.h"
#include "hci/address.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "storage/legacy_osi_config.h"

namespace bluetooth {
namespace storage {

using LegacyReadConfigCallback = common::OnceCallback<void(const std::string, std::unique_ptr<config_t>)>;
using LegacyWriteConfigCallback = common::OnceCallback<void(const std::string, bool success)>;
using LegacyReadChecksumCallback = common::OnceCallback<void(const std::string, std::string)>;
using LegacyWriteChecksumCallback = common::OnceCallback<void(const std::string, bool success)>;

class LegacyModule : public bluetooth::Module {
 public:
  void ConfigRead(const std::string filename, LegacyReadConfigCallback callback, os::Handler* handler);
  void ConfigWrite(const std::string filename, const config_t& config, LegacyWriteConfigCallback callback,
                   os::Handler* handler);
  void ChecksumRead(const std::string filename, LegacyReadChecksumCallback callback, os::Handler* handler);
  void ChecksumWrite(const std::string filename, const std::string checksum, LegacyWriteChecksumCallback callback,
                     os::Handler* handler);

  static const ModuleFactory Factory;

  LegacyModule();
  ~LegacyModule();

 protected:
  void ListDependencies(ModuleList* list) override;
  void Start() override;
  void Stop() override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(LegacyModule);
};

}  // namespace storage
}  // namespace bluetooth
