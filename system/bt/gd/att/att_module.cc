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

#define LOG_TAG "att"

#include <memory>
#include "module.h"
#include "os/handler.h"
#include "os/log.h"

#include "att/att_module.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "l2cap/le/l2cap_le_module.h"

namespace bluetooth {
namespace att {

const ModuleFactory AttModule::Factory = ModuleFactory([]() { return new AttModule(); });

namespace {
void OnAttRegistrationCompleteLe(l2cap::le::FixedChannelManager::RegistrationResult result,
                                 std::unique_ptr<l2cap::le::FixedChannelService> le_smp_service) {
  LOG_INFO("ATT channel registration complete");
}

void OnAttConnectionOpenLe(std::unique_ptr<l2cap::le::FixedChannel> channel) {
  LOG_INFO("ATT conneciton opened");
}
}  // namespace

struct AttModule::impl {
  impl(os::Handler* att_handler, l2cap::le::L2capLeModule* l2cap_le_module,
       l2cap::classic::L2capClassicModule* l2cap_classic_module)
      : att_handler_(att_handler), l2cap_le_module_(l2cap_le_module), l2cap_classic_module_(l2cap_classic_module) {
    // TODO: move that into a ATT manager, or other proper place
    std::unique_ptr<bluetooth::l2cap::le::FixedChannelManager> l2cap_manager_le_(
        l2cap_le_module_->GetFixedChannelManager());
    l2cap_manager_le_->RegisterService(bluetooth::l2cap::kLeAttributeCid, {},
                                       common::BindOnce(&OnAttRegistrationCompleteLe),
                                       common::Bind(&OnAttConnectionOpenLe), att_handler_);
  }

  os::Handler* att_handler_;
  l2cap::le::L2capLeModule* l2cap_le_module_;
  l2cap::classic::L2capClassicModule* l2cap_classic_module_;
};

void AttModule::ListDependencies(ModuleList* list) {
  list->add<l2cap::le::L2capLeModule>();
  list->add<l2cap::classic::L2capClassicModule>();
}

void AttModule::Start() {
  pimpl_ = std::make_unique<impl>(GetHandler(), GetDependency<l2cap::le::L2capLeModule>(),
                                  GetDependency<l2cap::classic::L2capClassicModule>());
}

void AttModule::Stop() {
  pimpl_.reset();
}

std::string AttModule::ToString() const {
  return "Att Module";
}

// std::unique_ptr<AttManager> AttModule::GetAttManager() {
//   return std::unique_ptr<AttManager>(
//       new AttManager(pimpl_->att_handler_, &pimpl_->att_manager_impl));
// }

}  // namespace att
}  // namespace bluetooth