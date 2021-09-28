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

#pragma once

#include <memory>
#include <vector>

#include "hci/address_with_type.h"
#include "security/internal/security_manager_impl.h"
#include "security/security_manager_listener.h"

namespace bluetooth {
namespace security {

/**
 * Manages the security attributes, pairing, bonding of devices, and the
 * encryption/decryption of communications.
 */
class SecurityManager : public UICallbacks {
 public:
  friend class SecurityModule;

  /**
   * Initialize the security record map from an internal device database.
   */
  void Init();

  /**
   * Initiates bond over Classic transport with device, if not bonded yet.
   *
   * @param address device address we want to bond with
   */
  void CreateBond(hci::AddressWithType address);

  /**
   * Initiates bond over Low Energy transport with device, if not bonded yet.
   *
   * @param address device address we want to bond with
   */
  void CreateBondLe(hci::AddressWithType address);

  /**
   * Cancels the pairing process for this device.
   *
   * @param device pointer to device with which we want to cancel our bond
   */
  void CancelBond(hci::AddressWithType device);

  /**
   * Disassociates the device and removes the persistent LTK
   *
   * @param device pointer to device we want to forget
   */
  void RemoveBond(hci::AddressWithType device);

  /**
   * Register Security UI handler, for handling prompts around the Pairing process.
   */
  void SetUserInterfaceHandler(UI* user_interface, os::Handler* handler);

  /**
   * Register to listen for callback events from SecurityManager
   *
   * @param listener ISecurityManagerListener instance to handle callbacks
   */
  void RegisterCallbackListener(ISecurityManagerListener* listener, os::Handler* handler);

  /**
   * Unregister listener for callback events from SecurityManager
   *
   * @param listener ISecurityManagerListener instance to unregister
   */
  void UnregisterCallbackListener(ISecurityManagerListener* listener);

  void OnPairingPromptAccepted(const bluetooth::hci::AddressWithType& address, bool confirmed) override;
  void OnConfirmYesNo(const bluetooth::hci::AddressWithType& address, bool confirmed) override;
  void OnPasskeyEntry(const bluetooth::hci::AddressWithType& address, uint32_t passkey) override;

 protected:
  SecurityManager(os::Handler* security_handler, internal::SecurityManagerImpl* security_manager_impl)
      : security_handler_(security_handler), security_manager_impl_(security_manager_impl) {}

 private:
  os::Handler* security_handler_ = nullptr;
  internal::SecurityManagerImpl* security_manager_impl_;
  DISALLOW_COPY_AND_ASSIGN(SecurityManager);
};

}  // namespace security
}  // namespace bluetooth
