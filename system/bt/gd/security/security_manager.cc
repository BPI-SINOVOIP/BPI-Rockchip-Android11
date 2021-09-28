/*
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */
#include "security_manager.h"

#include "os/log.h"

namespace bluetooth {
namespace security {

// Definition of Pure Virtual Destructor
ISecurityManagerListener::~ISecurityManagerListener() {}

void SecurityManager::Init() {
  security_handler_->Post(
      common::BindOnce(&internal::SecurityManagerImpl::Init, common::Unretained(security_manager_impl_)));
}

void SecurityManager::CreateBond(hci::AddressWithType device) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::CreateBond,
                                           common::Unretained(security_manager_impl_),
                                           std::forward<hci::AddressWithType>(device)));
}

void SecurityManager::CreateBondLe(hci::AddressWithType device) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::CreateBondLe,
                                           common::Unretained(security_manager_impl_),
                                           std::forward<hci::AddressWithType>(device)));
}

void SecurityManager::CancelBond(hci::AddressWithType device) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::CancelBond,
                                           common::Unretained(security_manager_impl_),
                                           std::forward<hci::AddressWithType>(device)));
}

void SecurityManager::RemoveBond(hci::AddressWithType device) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::RemoveBond,
                                           common::Unretained(security_manager_impl_),
                                           std::forward<hci::AddressWithType>(device)));
}

void SecurityManager::SetUserInterfaceHandler(UI* user_interface, os::Handler* handler) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::SetUserInterfaceHandler,
                                           common::Unretained(security_manager_impl_), user_interface, handler));
}

void SecurityManager::RegisterCallbackListener(ISecurityManagerListener* listener, os::Handler* handler) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::RegisterCallbackListener,
                                           common::Unretained(security_manager_impl_), listener, handler));
}

void SecurityManager::UnregisterCallbackListener(ISecurityManagerListener* listener) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::UnregisterCallbackListener,
                                           common::Unretained(security_manager_impl_), listener));
}

void SecurityManager::OnPairingPromptAccepted(const bluetooth::hci::AddressWithType& address, bool confirmed) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::OnPairingPromptAccepted,
                                           common::Unretained(security_manager_impl_), address, confirmed));
}
void SecurityManager::OnConfirmYesNo(const bluetooth::hci::AddressWithType& address, bool confirmed) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::OnConfirmYesNo,
                                           common::Unretained(security_manager_impl_), address, confirmed));
}
void SecurityManager::OnPasskeyEntry(const bluetooth::hci::AddressWithType& address, uint32_t passkey) {
  security_handler_->Post(common::BindOnce(&internal::SecurityManagerImpl::OnPasskeyEntry,
                                           common::Unretained(security_manager_impl_), address, passkey));
}

}  // namespace security
}  // namespace bluetooth
