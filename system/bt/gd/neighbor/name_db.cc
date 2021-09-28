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
#define LOG_TAG "bt_gd_neigh"

#include "neighbor/name_db.h"

#include <memory>
#include <unordered_map>
#include <utility>

#include "common/bind.h"
#include "module.h"
#include "neighbor/name.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace neighbor {

namespace {
struct PendingRemoteNameRead {
  ReadRemoteNameDbCallback callback_;
  os::Handler* handler_;
};
}  // namespace

struct NameDbModule::impl {
  void ReadRemoteNameRequest(hci::Address address, ReadRemoteNameDbCallback callback, os::Handler* handler);

  bool IsNameCached(hci::Address address) const;
  RemoteName ReadCachedRemoteName(hci::Address address) const;

  impl(const NameDbModule& module);

  void Start();
  void Stop();

 private:
  std::unordered_map<hci::Address, PendingRemoteNameRead> address_to_pending_read_map_;
  std::unordered_map<hci::Address, RemoteName> address_to_name_map_;

  void OnRemoteNameResponse(hci::ErrorCode status, hci::Address address, RemoteName name);

  neighbor::NameModule* name_module_;

  const NameDbModule& module_;
  os::Handler* handler_;
};

const ModuleFactory neighbor::NameDbModule::Factory = ModuleFactory([]() { return new neighbor::NameDbModule(); });

neighbor::NameDbModule::impl::impl(const neighbor::NameDbModule& module) : module_(module) {}

void neighbor::NameDbModule::impl::ReadRemoteNameRequest(hci::Address address, ReadRemoteNameDbCallback callback,
                                                         os::Handler* handler) {
  if (address_to_pending_read_map_.find(address) != address_to_pending_read_map_.end()) {
    LOG_WARN("Already have remote read db in progress and currently can only have one outstanding");
    return;
  }

  address_to_pending_read_map_[address] = {std::move(callback), std::move(handler)};

  // TODO(cmanton) Use remote name request defaults for now
  hci::PageScanRepetitionMode page_scan_repetition_mode = hci::PageScanRepetitionMode::R1;
  uint16_t clock_offset = 0;
  hci::ClockOffsetValid clock_offset_valid = hci::ClockOffsetValid::INVALID;
  name_module_->ReadRemoteNameRequest(
      address, page_scan_repetition_mode, clock_offset, clock_offset_valid,
      common::BindOnce(&NameDbModule::impl::OnRemoteNameResponse, common::Unretained(this)), handler_);
}

void neighbor::NameDbModule::impl::OnRemoteNameResponse(hci::ErrorCode status, hci::Address address, RemoteName name) {
  ASSERT(address_to_pending_read_map_.find(address) != address_to_pending_read_map_.end());
  PendingRemoteNameRead callback_handler = std::move(address_to_pending_read_map_.at(address));

  if (status == hci::ErrorCode::SUCCESS) {
    address_to_name_map_[address] = name;
  }
  callback_handler.handler_->Post(
      common::BindOnce(std::move(callback_handler.callback_), address, status == hci::ErrorCode::SUCCESS));
}

bool neighbor::NameDbModule::impl::IsNameCached(hci::Address address) const {
  return address_to_name_map_.count(address) == 1;
}

RemoteName neighbor::NameDbModule::impl::ReadCachedRemoteName(hci::Address address) const {
  ASSERT(IsNameCached(address));
  return address_to_name_map_.at(address);
}

/**
 * General API here
 */
neighbor::NameDbModule::NameDbModule() : pimpl_(std::make_unique<impl>(*this)) {}

neighbor::NameDbModule::~NameDbModule() {
  pimpl_.reset();
}

void neighbor::NameDbModule::ReadRemoteNameRequest(hci::Address address, ReadRemoteNameDbCallback callback,
                                                   os::Handler* handler) {
  GetHandler()->Post(common::BindOnce(&NameDbModule::impl::ReadRemoteNameRequest, common::Unretained(pimpl_.get()),
                                      address, std::move(callback), handler));
}

bool neighbor::NameDbModule::IsNameCached(hci::Address address) const {
  return pimpl_->IsNameCached(address);
}

RemoteName neighbor::NameDbModule::ReadCachedRemoteName(hci::Address address) const {
  return pimpl_->ReadCachedRemoteName(address);
}

void neighbor::NameDbModule::impl::Start() {
  name_module_ = module_.GetDependency<neighbor::NameModule>();
  handler_ = module_.GetHandler();
}

void neighbor::NameDbModule::impl::Stop() {}

/**
 * Module methods here
 */
void neighbor::NameDbModule::ListDependencies(ModuleList* list) {
  list->add<neighbor::NameModule>();
}

void neighbor::NameDbModule::Start() {
  pimpl_->Start();
}

void neighbor::NameDbModule::Stop() {
  pimpl_->Stop();
}

}  // namespace neighbor
}  // namespace bluetooth
