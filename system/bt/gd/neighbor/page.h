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

#include "hci/hci_packets.h"
#include "module.h"
#include "neighbor/scan_parameters.h"

namespace bluetooth {
namespace neighbor {

using PageTimeout = uint16_t;  // Range = 0x0001 to 0xffff, Time N * 0.625ms

inline double PageTimeoutMs(PageTimeout timeout) {
  return kTimeTickMs * timeout;
}

class PageModule : public bluetooth::Module {
 public:
  void SetScanActivity(ScanParameters params);
  ScanParameters GetScanActivity() const;

  void SetInterlacedScan();
  void SetStandardScan();

  void SetTimeout(PageTimeout timeout);

  static const ModuleFactory Factory;

  PageModule();
  ~PageModule();

 protected:
  void ListDependencies(ModuleList* list) override;
  void Start() override;
  void Stop() override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;

  DISALLOW_COPY_AND_ASSIGN(PageModule);
};

}  // namespace neighbor
}  // namespace bluetooth
