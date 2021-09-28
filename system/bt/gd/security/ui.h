/******************************************************************************
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
 ******************************************************************************/

#pragma once

#include "hci/address_with_type.h"

namespace bluetooth {
namespace security {

// Through this interface we talk to the user, asking for confirmations/acceptance.
class UI {
 public:
  virtual ~UI(){};

  /* Remote LE device tries to initiate pairing, ask user to confirm */
  virtual void DisplayPairingPrompt(const bluetooth::hci::AddressWithType& address, std::string name) = 0;

  /* Remove the pairing prompt from DisplayPairingPrompt, i.e. remote device disconnected, or some application requested
   * bond with this device */
  virtual void Cancel(const bluetooth::hci::AddressWithType& address) = 0;

  /* Display value for Comprision, user responds yes/no */
  virtual void DisplayConfirmValue(const bluetooth::hci::AddressWithType& address, std::string name,
                                   uint32_t numeric_value) = 0;

  /* Display Yes/No dialog, Classic pairing, numeric comparison with NoInputNoOutput device */
  virtual void DisplayYesNoDialog(const bluetooth::hci::AddressWithType& address, std::string name) = 0;

  /* Display a dialog box that will let user enter the Passkey */
  virtual void DisplayEnterPasskeyDialog(const bluetooth::hci::AddressWithType& address, std::string name) = 0;

  /* Present the passkey value to the user, user compares with other device */
  virtual void DisplayPasskey(const bluetooth::hci::AddressWithType& address, std::string name, uint32_t passkey) = 0;
};

/* Through this interface, UI provides us with user choices. */
class UICallbacks {
 public:
  virtual ~UICallbacks() = default;

  /* User accepted pairing prompt */
  virtual void OnPairingPromptAccepted(const bluetooth::hci::AddressWithType& address, bool confirmed) = 0;

  /* User confirmed that displayed value matches the value on the other device */
  virtual void OnConfirmYesNo(const bluetooth::hci::AddressWithType& address, bool confirmed) = 0;

  /* User typed the value displayed on the other device. This is either Passkey or the Confirm value */
  virtual void OnPasskeyEntry(const bluetooth::hci::AddressWithType& address, uint32_t passkey) = 0;
};

}  // namespace security
}  // namespace bluetooth
