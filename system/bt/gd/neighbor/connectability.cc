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
#define LOG_TAG "neighbor2"

#include <memory>

#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "module.h"
#include "neighbor/connectability.h"
#include "neighbor/scan.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace neighbor {

struct ConnectabilityModule::impl {
  void StartConnectability();
  void StopConnectability();
  bool IsConnectable() const;

  void Start();
  void Stop();

  impl(ConnectabilityModule& connectability_module);

 private:
  ConnectabilityModule& module_;

  neighbor::ScanModule* scan_module_;
};

const ModuleFactory neighbor::ConnectabilityModule::Factory =
    ModuleFactory([]() { return new ConnectabilityModule(); });

neighbor::ConnectabilityModule::impl::impl(neighbor::ConnectabilityModule& module) : module_(module) {}

void neighbor::ConnectabilityModule::impl::StartConnectability() {
  scan_module_->SetPageScan();
}

void neighbor::ConnectabilityModule::impl::StopConnectability() {
  scan_module_->ClearPageScan();
}

bool neighbor::ConnectabilityModule::impl::IsConnectable() const {
  return scan_module_->IsPageEnabled();
}

void neighbor::ConnectabilityModule::impl::Start() {
  scan_module_ = module_.GetDependency<neighbor::ScanModule>();
}

void neighbor::ConnectabilityModule::impl::Stop() {}

neighbor::ConnectabilityModule::ConnectabilityModule() : pimpl_(std::make_unique<impl>(*this)) {}

neighbor::ConnectabilityModule::~ConnectabilityModule() {
  pimpl_.reset();
}

void neighbor::ConnectabilityModule::StartConnectability() {
  pimpl_->StartConnectability();
}

void neighbor::ConnectabilityModule::StopConnectability() {
  pimpl_->StopConnectability();
}

bool neighbor::ConnectabilityModule::IsConnectable() const {
  return pimpl_->IsConnectable();
}

/**
 * Module stuff
 */
void neighbor::ConnectabilityModule::ListDependencies(ModuleList* list) {
  list->add<neighbor::ScanModule>();
}

void neighbor::ConnectabilityModule::Start() {
  pimpl_->Start();
}

void neighbor::ConnectabilityModule::Stop() {
  pimpl_->Stop();
}

}  // namespace neighbor
}  // namespace bluetooth
