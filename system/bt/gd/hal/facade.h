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

#include <list>
#include <mutex>

#include <grpc++/grpc++.h>

#include "grpc/grpc_module.h"
#include "hal/hci_hal.h"

namespace bluetooth {
namespace hal {

class HciHalFacadeService;

class HciHalFacadeModule : public ::bluetooth::grpc::GrpcFacadeModule {
 public:
  static const ModuleFactory Factory;

  void ListDependencies(ModuleList* list) override;

  void Start() override;
  void Stop() override;

  ::grpc::Service* GetService() const override;

 private:
  HciHalFacadeService* service_;
};

}  // namespace hal
}  // namespace bluetooth
