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
#define LOG_TAG "l2cap2"

#include <memory>

#include "common/bidi_queue.h"
#include "hci/acl_manager.h"
#include "hci/address.h"
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "l2cap/classic/internal/dynamic_channel_service_manager_impl.h"
#include "l2cap/classic/internal/fixed_channel_service_manager_impl.h"
#include "l2cap/classic/internal/link_manager.h"
#include "l2cap/internal/parameter_provider.h"
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

#include "l2cap/classic/l2cap_classic_module.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

const ModuleFactory L2capClassicModule::Factory = ModuleFactory([]() { return new L2capClassicModule(); });

struct L2capClassicModule::impl {
  impl(os::Handler* l2cap_handler, hci::AclManager* acl_manager)
      : l2cap_handler_(l2cap_handler), acl_manager_(acl_manager) {}
  os::Handler* l2cap_handler_;
  hci::AclManager* acl_manager_;
  l2cap::internal::ParameterProvider parameter_provider_;
  internal::FixedChannelServiceManagerImpl fixed_channel_service_manager_impl_{l2cap_handler_};
  internal::DynamicChannelServiceManagerImpl dynamic_channel_service_manager_impl_{l2cap_handler_};
  internal::LinkManager link_manager_{l2cap_handler_, acl_manager_, &fixed_channel_service_manager_impl_,
                                      &dynamic_channel_service_manager_impl_, &parameter_provider_};
};

L2capClassicModule::L2capClassicModule() {}

L2capClassicModule::~L2capClassicModule() {}

void L2capClassicModule::ListDependencies(ModuleList* list) {
  list->add<hci::AclManager>();
}

void L2capClassicModule::Start() {
  pimpl_ = std::make_unique<impl>(GetHandler(), GetDependency<hci::AclManager>());
}

void L2capClassicModule::Stop() {
  pimpl_.reset();
}

std::string L2capClassicModule::ToString() const {
  return "L2cap Classic Module";
}

std::unique_ptr<FixedChannelManager> L2capClassicModule::GetFixedChannelManager() {
  return std::unique_ptr<FixedChannelManager>(new FixedChannelManager(&pimpl_->fixed_channel_service_manager_impl_,
                                                                      &pimpl_->link_manager_, pimpl_->l2cap_handler_));
}

std::unique_ptr<DynamicChannelManager> L2capClassicModule::GetDynamicChannelManager() {
  return std::unique_ptr<DynamicChannelManager>(new DynamicChannelManager(
      &pimpl_->dynamic_channel_service_manager_impl_, &pimpl_->link_manager_, pimpl_->l2cap_handler_));
}

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
