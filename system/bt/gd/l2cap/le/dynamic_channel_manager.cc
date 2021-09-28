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

#include "l2cap/le/dynamic_channel_manager.h"
#include "l2cap/le/internal/dynamic_channel_service_impl.h"
#include "l2cap/le/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/le/internal/link.h"
#include "l2cap/le/internal/link_manager.h"

namespace bluetooth {
namespace l2cap {
namespace le {

bool DynamicChannelManager::ConnectChannel(hci::AddressWithType device,
                                           DynamicChannelConfigurationOption configuration_option, Psm psm,
                                           OnConnectionOpenCallback on_connection_open,
                                           OnConnectionFailureCallback on_fail_callback, os::Handler* handler) {
  internal::Link::PendingDynamicChannelConnection pending_dynamic_channel_connection{
      .handler_ = handler,
      .on_open_callback_ = std::move(on_connection_open),
      .on_fail_callback_ = std::move(on_fail_callback),
      .configuration_ = configuration_option,
  };
  l2cap_layer_handler_->Post(common::BindOnce(&internal::LinkManager::ConnectDynamicChannelServices,
                                              common::Unretained(link_manager_), device,
                                              std::move(pending_dynamic_channel_connection), psm));

  return true;
}

bool DynamicChannelManager::RegisterService(Psm psm, DynamicChannelConfigurationOption configuration_option,
                                            const SecurityPolicy& security_policy,
                                            OnRegistrationCompleteCallback on_registration_complete,
                                            OnConnectionOpenCallback on_connection_open, os::Handler* handler) {
  internal::DynamicChannelServiceImpl::PendingRegistration pending_registration{
      .user_handler_ = handler,
      .on_registration_complete_callback_ = std::move(on_registration_complete),
      .on_connection_open_callback_ = std::move(on_connection_open),
      .configuration_ = configuration_option,
  };
  l2cap_layer_handler_->Post(common::BindOnce(&internal::DynamicChannelServiceManagerImpl::Register,
                                              common::Unretained(service_manager_), psm,
                                              std::move(pending_registration)));

  return true;
}

}  // namespace le
}  // namespace l2cap
}  // namespace bluetooth
