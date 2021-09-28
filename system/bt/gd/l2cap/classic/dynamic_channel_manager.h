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
#pragma once

#include <string>

#include "hci/acl_manager.h"
#include "hci/address.h"
#include "l2cap/classic/dynamic_channel.h"
#include "l2cap/classic/dynamic_channel_configuration_option.h"
#include "l2cap/classic/dynamic_channel_service.h"
#include "l2cap/l2cap_packets.h"
#include "l2cap/psm.h"
#include "l2cap/security_policy.h"
#include "os/handler.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

class L2capClassicModule;

namespace internal {
class LinkManager;
class DynamicChannelServiceManagerImpl;
}  // namespace internal

class DynamicChannelManager {
 public:
  enum class ConnectionResultCode {
    SUCCESS = 0,
    FAIL_NO_SERVICE_REGISTERED = 1,  // No service is registered
    FAIL_HCI_ERROR = 2,              // See hci_error
    FAIL_L2CAP_ERROR = 3,            // See l2cap_connection_response_result
  };

  struct ConnectionResult {
    ConnectionResultCode connection_result_code = ConnectionResultCode::SUCCESS;
    hci::ErrorCode hci_error = hci::ErrorCode::SUCCESS;
    ConnectionResponseResult l2cap_connection_response_result = ConnectionResponseResult::SUCCESS;
  };
  /**
   * OnConnectionFailureCallback(std::string failure_reason);
   */
  using OnConnectionFailureCallback = common::OnceCallback<void(ConnectionResult result)>;

  /**
   * OnConnectionOpenCallback(DynamicChannel channel);
   */
  using OnConnectionOpenCallback = common::Callback<void(std::unique_ptr<DynamicChannel>)>;

  enum class RegistrationResult {
    SUCCESS = 0,
    FAIL_DUPLICATE_SERVICE = 1,  // Duplicate service registration for the same PSM
    FAIL_INVALID_SERVICE = 2,    // Invalid PSM
  };

  /**
   * OnRegistrationFailureCallback(RegistrationResult result, DynamicChannelService service);
   */
  using OnRegistrationCompleteCallback =
      common::OnceCallback<void(RegistrationResult, std::unique_ptr<DynamicChannelService>)>;

  /**
   * Connect to a Dynamic channel on a remote device
   *
   * - This method is asynchronous
   * - When false is returned, the connection fails immediately
   * - When true is returned, method caller should wait for on_fail_callback or on_open_callback
   * - If an ACL connection does not exist, this method will create an ACL connection
   * - If HCI connection failed, on_fail_callback will be triggered with FAIL_HCI_ERROR
   * - If Dynamic channel on a remote device is already reported as connected via on_open_callback, it won't be
   *   reported again
   *
   * @param device: Remote device to make this connection.
   * @param psm: Service PSM to connect. PSM is defined in Core spec Vol 3 Part A 4.2.
   * @param on_open_callback: A callback to indicate success of a connection initiated from a remote device.
   * @param on_fail_callback: A callback to indicate connection failure along with a status code.
   * @param handler: The handler context in which to execute the @callback parameters.
   * @param configuration_option: The configuration options for this channel
   *
   * Returns: true if connection was able to be initiated, false otherwise.
   */
  virtual bool ConnectChannel(hci::Address device, DynamicChannelConfigurationOption configuration_option, Psm psm,
                              OnConnectionOpenCallback on_connection_open, OnConnectionFailureCallback on_fail_callback,
                              os::Handler* handler);

  /**
   * Register a service to receive incoming connections bound to a specific channel.
   *
   * - This method is asynchronous.
   * - When false is returned, the registration fails immediately.
   * - When true is returned, method caller should wait for on_service_registered callback that contains a
   *   DynamicChannelService object. The registered service can be managed from that object.
   * - If a PSM is already registered or some other error happens, on_registration_complete will be triggered with a
   *   non-SUCCESS value
   * - After a service is registered, a DynamicChannel is delivered through on_open_callback when the remote
   *   initiates a channel open and channel is opened successfully
   * - on_open_callback, will only be triggered after on_service_registered callback
   *
   * @param security_policy: The security policy used for the connection.
   * @param psm: Service PSM to register. PSM is defined in Core spec Vol 3 Part A 4.2.
   * @param on_registration_complete: A callback to indicate the service setup has completed. If the return status is
   *        not SUCCESS, it means service is not registered due to reasons like PSM already take
   * @param on_open_callback: A callback to indicate success of a connection initiated from a remote device.
   * @param handler: The handler context in which to execute the @callback parameter.
   * @param configuration_option: The configuration options for this channel
   */
  virtual bool RegisterService(Psm psm, DynamicChannelConfigurationOption configuration_option,
                               const SecurityPolicy& security_policy,
                               OnRegistrationCompleteCallback on_registration_complete,
                               OnConnectionOpenCallback on_connection_open, os::Handler* handler);

  friend class L2capClassicModule;

  virtual ~DynamicChannelManager() = default;

 protected:
  DynamicChannelManager() = default;

 private:
  // The constructor is not to be used by user code
  DynamicChannelManager(internal::DynamicChannelServiceManagerImpl* service_manager,
                        internal::LinkManager* link_manager, os::Handler* l2cap_layer_handler)
      : service_manager_(service_manager), link_manager_(link_manager), l2cap_layer_handler_(l2cap_layer_handler) {
    ASSERT(service_manager_ != nullptr);
    ASSERT(link_manager_ != nullptr);
    ASSERT(l2cap_layer_handler_ != nullptr);
  }
  internal::DynamicChannelServiceManagerImpl* service_manager_ = nullptr;
  internal::LinkManager* link_manager_ = nullptr;
  os::Handler* l2cap_layer_handler_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(DynamicChannelManager);
};

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
