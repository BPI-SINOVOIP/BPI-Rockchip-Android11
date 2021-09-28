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

#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "common/bind.h"
#include "l2cap/classic/internal/dynamic_channel_service_impl.h"
#include "l2cap/psm.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

void DynamicChannelServiceManagerImpl::Register(Psm psm,
                                                DynamicChannelServiceImpl::PendingRegistration pending_registration) {
  if (!IsPsmValid(psm)) {
    std::unique_ptr<DynamicChannelService> invalid_service(new DynamicChannelService());
    pending_registration.user_handler_->Post(
        common::BindOnce(std::move(pending_registration.on_registration_complete_callback_),
                         DynamicChannelManager::RegistrationResult::FAIL_INVALID_SERVICE, std::move(invalid_service)));
  } else if (IsServiceRegistered(psm)) {
    std::unique_ptr<DynamicChannelService> invalid_service(new DynamicChannelService());
    pending_registration.user_handler_->Post(common::BindOnce(
        std::move(pending_registration.on_registration_complete_callback_),
        DynamicChannelManager::RegistrationResult::FAIL_DUPLICATE_SERVICE, std::move(invalid_service)));
  } else {
    service_map_.try_emplace(psm,
                             DynamicChannelServiceImpl(pending_registration.user_handler_,
                                                       pending_registration.security_policy_,
                                                       std::move(pending_registration.on_connection_open_callback_),
                                                       pending_registration.configuration_));
    std::unique_ptr<DynamicChannelService> user_service(new DynamicChannelService(psm, this, l2cap_layer_handler_));
    pending_registration.user_handler_->Post(
        common::BindOnce(std::move(pending_registration.on_registration_complete_callback_),
                         DynamicChannelManager::RegistrationResult::SUCCESS, std::move(user_service)));
  }
}

void DynamicChannelServiceManagerImpl::Unregister(Psm psm, DynamicChannelService::OnUnregisteredCallback callback,
                                                  os::Handler* handler) {
  if (IsServiceRegistered(psm)) {
    service_map_.erase(psm);
    handler->Post(std::move(callback));
  } else {
    LOG_ERROR("service not registered psm:%d", psm);
  }
}

bool DynamicChannelServiceManagerImpl::IsServiceRegistered(Psm psm) const {
  return service_map_.find(psm) != service_map_.end();
}

DynamicChannelServiceImpl* DynamicChannelServiceManagerImpl::GetService(Psm psm) {
  ASSERT(IsServiceRegistered(psm));
  return &service_map_.find(psm)->second;
}

std::vector<std::pair<Psm, DynamicChannelServiceImpl*>> DynamicChannelServiceManagerImpl::GetRegisteredServices() {
  std::vector<std::pair<Psm, DynamicChannelServiceImpl*>> results;
  for (auto& elem : service_map_) {
    results.emplace_back(elem.first, &elem.second);
  }
  return results;
}

}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
