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

#include "module.h"

namespace bluetooth {
namespace shim {

using DumpsysFunction = std::function<void(int fd)>;

class Dumpsys : public bluetooth::Module {
 public:
  void Dump(int fd);
  void RegisterDumpsysFunction(const void* token, DumpsysFunction func);
  void UnregisterDumpsysFunction(const void* token);

  /* This is not a dumpsys-specific method, we just must grab thread from of one modules */
  os::Handler* GetGdShimHandler();

  Dumpsys() = default;
  ~Dumpsys() = default;

  static const ModuleFactory Factory;

 protected:
  void ListDependencies(ModuleList* list) override;  // Module
  void Start() override;                             // Module
  void Stop() override;                              // Module
  std::string ToString() const override;             // Module

 private:
  struct impl;
  std::unique_ptr<impl> pimpl_;
  DISALLOW_COPY_AND_ASSIGN(Dumpsys);
};

}  // namespace shim
}  // namespace bluetooth
