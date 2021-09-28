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

#include "common/callback.h"
#include "hci/hci_packets.h"
#include "hci/le_report.h"
#include "module.h"

namespace bluetooth {
namespace hci {

class LeScanningManagerCallbacks {
 public:
  virtual ~LeScanningManagerCallbacks() = default;
  virtual void on_advertisements(std::vector<std::shared_ptr<LeReport>>) = 0;
  virtual void on_timeout() = 0;
  virtual os::Handler* Handler() = 0;
};

class LeScanningManager : public bluetooth::Module {
 public:
  LeScanningManager();

  void StartScan(LeScanningManagerCallbacks* callbacks);

  void StopScan(common::Callback<void()> on_stopped);

  static const ModuleFactory Factory;

 protected:
  void ListDependencies(ModuleList* list) override;

  void Start() override;

  void Stop() override;

  std::string ToString() const override;

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;
  DISALLOW_COPY_AND_ASSIGN(LeScanningManager);
};

}  // namespace hci
}  // namespace bluetooth
