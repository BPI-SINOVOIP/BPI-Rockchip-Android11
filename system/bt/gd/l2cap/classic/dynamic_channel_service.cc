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

#include "l2cap/classic/dynamic_channel_service.h"
#include "common/bind.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

void DynamicChannelService::Unregister(OnUnregisteredCallback on_unregistered, os::Handler* on_unregistered_handler) {
  ASSERT_LOG(manager_ != nullptr, "this service is invalid");
  l2cap_layer_handler_->Post(common::BindOnce(&internal::DynamicChannelServiceManagerImpl::Unregister,
                                              common::Unretained(manager_), psm_, std::move(on_unregistered),
                                              on_unregistered_handler));
}

Psm DynamicChannelService::GetPsm() const {
  return psm_;
}

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
