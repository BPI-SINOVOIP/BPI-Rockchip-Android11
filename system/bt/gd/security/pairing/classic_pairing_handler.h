/*
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License") override;
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

#include "security/pairing/pairing_handler.h"

#include <utility>

#include "common/callback.h"
#include "l2cap/classic/l2cap_classic_module.h"
#include "security/initial_informations.h"
#include "security/security_manager_listener.h"

namespace bluetooth {
namespace security {

class ISecurityManagerListener;

namespace pairing {

static constexpr hci::IoCapability kDefaultIoCapability = hci::IoCapability::DISPLAY_YES_NO;
static constexpr hci::OobDataPresent kDefaultOobDataPresent = hci::OobDataPresent::NOT_PRESENT;
static constexpr hci::AuthenticationRequirements kDefaultAuthenticationRequirements =
    hci::AuthenticationRequirements::DEDICATED_BONDING_MITM_PROTECTION;

class ClassicPairingHandler : public PairingHandler {
 public:
  ClassicPairingHandler(std::shared_ptr<l2cap::classic::FixedChannelManager> fixed_channel_manager,
                        channel::SecurityManagerChannel* security_manager_channel,
                        std::shared_ptr<record::SecurityRecord> record, os::Handler* security_handler,
                        common::OnceCallback<void(hci::Address, PairingResultOrFailure)> complete_callback,
                        UI* user_interface, os::Handler* user_interface_handler, std::string device_name)
      : PairingHandler(security_manager_channel, std::move(record)),
        fixed_channel_manager_(std::move(fixed_channel_manager)), security_policy_(),
        security_handler_(security_handler), remote_io_capability_(kDefaultIoCapability),
        local_io_capability_(kDefaultIoCapability), local_oob_present_(kDefaultOobDataPresent),
        local_authentication_requirements_(kDefaultAuthenticationRequirements),
        complete_callback_(std::move(complete_callback)), user_interface_(user_interface),
        user_interface_handler_(user_interface_handler), device_name_(device_name) {}

  ~ClassicPairingHandler() override = default;

  void Initiate(bool locally_initiated, hci::IoCapability io_capability, hci::OobDataPresent oob_present,
                hci::AuthenticationRequirements auth_requirements) override;
  void Cancel() override;

  void OnReceive(hci::ChangeConnectionLinkKeyCompleteView packet) override;
  void OnReceive(hci::MasterLinkKeyCompleteView packet) override;
  void OnReceive(hci::PinCodeRequestView packet) override;
  void OnReceive(hci::LinkKeyRequestView packet) override;
  void OnReceive(hci::LinkKeyNotificationView packet) override;
  void OnReceive(hci::IoCapabilityRequestView packet) override;
  void OnReceive(hci::IoCapabilityResponseView packet) override;
  void OnReceive(hci::SimplePairingCompleteView packet) override;
  void OnReceive(hci::ReturnLinkKeysView packet) override;
  void OnReceive(hci::EncryptionChangeView packet) override;
  void OnReceive(hci::EncryptionKeyRefreshCompleteView packet) override;
  void OnReceive(hci::RemoteOobDataRequestView packet) override;
  void OnReceive(hci::UserPasskeyNotificationView packet) override;
  void OnReceive(hci::KeypressNotificationView packet) override;
  void OnReceive(hci::UserConfirmationRequestView packet) override;
  void OnReceive(hci::UserPasskeyRequestView packet) override;

  void OnPairingPromptAccepted(const bluetooth::hci::AddressWithType& address, bool confirmed) override;
  void OnConfirmYesNo(const bluetooth::hci::AddressWithType& address, bool confirmed) override;
  void OnPasskeyEntry(const bluetooth::hci::AddressWithType& address, uint32_t passkey) override;

 private:
  void OnRegistrationComplete(l2cap::classic::FixedChannelManager::RegistrationResult result,
                              std::unique_ptr<l2cap::classic::FixedChannelService> fixed_channel_service);
  void OnUnregistered();
  void OnConnectionOpen(std::unique_ptr<l2cap::classic::FixedChannel> fixed_channel);
  void OnConnectionFail(l2cap::classic::FixedChannelManager::ConnectionResult result);
  void OnConnectionClose(hci::ErrorCode error_code);
  void OnUserInput(bool user_input);
  void OnPasskeyInput(uint32_t passkey);
  void NotifyUiDisplayYesNo(uint32_t numeric_value);
  void NotifyUiDisplayYesNo();
  void NotifyUiDisplayPasskey(uint32_t passkey);
  void NotifyUiDisplayPasskeyInput();
  void NotifyUiDisplayCancel();
  void UserClickedYes();
  void UserClickedNo();

  std::shared_ptr<l2cap::classic::FixedChannelManager> fixed_channel_manager_;
  std::unique_ptr<l2cap::classic::FixedChannelService> fixed_channel_service_{nullptr};
  l2cap::SecurityPolicy security_policy_ __attribute__((unused));
  os::Handler* security_handler_ __attribute__((unused));
  hci::IoCapability remote_io_capability_ __attribute__((unused));
  hci::IoCapability local_io_capability_ __attribute__((unused));
  hci::OobDataPresent local_oob_present_ __attribute__((unused));
  hci::AuthenticationRequirements local_authentication_requirements_ __attribute__((unused));
  std::unique_ptr<l2cap::classic::FixedChannel> fixed_channel_{nullptr};
  common::OnceCallback<void(hci::Address, PairingResultOrFailure)> complete_callback_;
  UI* user_interface_;
  os::Handler* user_interface_handler_;
  std::string device_name_;

  hci::ErrorCode last_status_;
  bool locally_initiated_ = false;
  uint32_t passkey_ = 0;
};

}  // namespace pairing
}  // namespace security
}  // namespace bluetooth
