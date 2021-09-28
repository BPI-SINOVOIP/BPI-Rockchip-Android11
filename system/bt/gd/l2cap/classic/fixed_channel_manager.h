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
#include "l2cap/cid.h"
#include "l2cap/classic/fixed_channel.h"
#include "l2cap/classic/fixed_channel_service.h"
#include "l2cap/security_policy.h"
#include "os/handler.h"

namespace bluetooth {
namespace l2cap {
namespace classic {

class L2capClassicModule;

namespace testing {
class MockFixedChannelManager;
}

namespace internal {
class LinkManager;
class FixedChannelServiceManagerImpl;
}  // namespace internal

class FixedChannelManager {
 public:
  enum class ConnectionResultCode {
    SUCCESS = 0,
    FAIL_NO_SERVICE_REGISTERED = 1,      // No service is registered
    FAIL_ALL_SERVICES_HAVE_CHANNEL = 2,  // All registered services already have a channel
    FAIL_HCI_ERROR = 3,                  // See hci_error
  };

  struct ConnectionResult {
    ConnectionResultCode connection_result_code = ConnectionResultCode::SUCCESS;
    hci::ErrorCode hci_error = hci::ErrorCode::SUCCESS;
  };
  /**
   * OnConnectionFailureCallback(std::string failure_reason);
   */
  using OnConnectionFailureCallback = common::OnceCallback<void(ConnectionResult result)>;

  /**
   * OnConnectionOpenCallback(FixedChannel channel);
   */
  using OnConnectionOpenCallback = common::Callback<void(std::unique_ptr<FixedChannel>)>;

  enum class RegistrationResult {
    SUCCESS = 0,
    FAIL_DUPLICATE_SERVICE = 1,  // Duplicate service registration for the same CID
    FAIL_INVALID_SERVICE = 2,    // Invalid CID
  };

  /**
   * OnRegistrationFailureCallback(RegistrationResult result, FixedChannelService service);
   */
  using OnRegistrationCompleteCallback =
      common::OnceCallback<void(RegistrationResult, std::unique_ptr<FixedChannelService>)>;

  /**
   * Connect to ALL fixed channels on a remote device
   *
   * - This method is asynchronous
   * - When false is returned, the connection fails immediately
   * - When true is returned, method caller should wait for on_fail_callback or on_open_callback registered through
   *   RegisterService() API.
   * - If an ACL connection does not exist, this method will create an ACL connection. As a result, on_open_callback
   *   supplied through RegisterService() will be triggered to provide the actual FixedChannel objects
   * - If HCI connection failed, on_fail_callback will be triggered with FAIL_HCI_ERROR
   * - If fixed channel on a remote device is already reported as connected via on_open_callback and has been acquired
   *   via FixedChannel#Acquire() API, it won't be reported again
   * - If no service is registered, on_fail_callback will be triggered with FAIL_NO_SERVICE_REGISTERED
   * - If there is an ACL connection and channels for each service is allocated, on_fail_callback will be triggered with
   *   FAIL_ALL_SERVICES_HAVE_CHANNEL
   *
   * NOTE:
   * This call will initiate an effort to connect all fixed channel services on a remote device.
   * Due to the connectionless nature of fixed channels, all fixed channels will be connected together.
   * If a fixed channel service does not need a particular fixed channel. It should release the received
   * channel immediately after receiving on_open_callback via FixedChannel#Release()
   *
   * A module calling ConnectServices() must have called RegisterService() before.
   * The callback will come back from on_open_callback in the service that is registered
   *
   * @param device: Remote device to make this connection.
   * @param on_fail_callback: A callback to indicate connection failure along with a status code.
   * @param handler: The handler context in which to execute the @callback parameters.
   *
   * Returns: true if connection was able to be initiated, false otherwise.
   */
  virtual bool ConnectServices(hci::Address device, OnConnectionFailureCallback on_fail_callback, os::Handler* handler);

  /**
   * Register a service to receive incoming connections bound to a specific channel.
   *
   * - This method is asynchronous.
   * - When false is returned, the registration fails immediately.
   * - When true is returned, method caller should wait for on_service_registered callback that contains a
   *   FixedChannelService object. The registered service can be managed from that object.
   * - If a CID is already registered or some other error happens, on_registration_complete will be triggered with a
   *   non-SUCCESS value
   * - After a service is registered, any classic ACL connection will create a FixedChannel object that is
   *   delivered through on_open_callback
   * - on_open_callback, will only be triggered after on_service_registered callback
   *
   * @param cid:  cid used to receive incoming connections
   * @param security_policy: The security policy used for the connection.
   * @param on_registration_complete: A callback to indicate the service setup has completed. If the return status is
   *        not SUCCESS, it means service is not registered due to reasons like CID already take
   * @param on_open_callback: A callback to indicate success of a connection initiated from a remote device.
   * @param handler: The handler context in which to execute the @callback parameter.
   */
  virtual bool RegisterService(Cid cid, const SecurityPolicy& security_policy,
                               OnRegistrationCompleteCallback on_registration_complete,
                               OnConnectionOpenCallback on_connection_open, os::Handler* handler);

  virtual ~FixedChannelManager() = default;

  friend class L2capClassicModule;
  friend class testing::MockFixedChannelManager;

 private:
  // The constructor is not to be used by user code
  FixedChannelManager(internal::FixedChannelServiceManagerImpl* service_manager, internal::LinkManager* link_manager,
                      os::Handler* l2cap_layer_handler)
      : service_manager_(service_manager), link_manager_(link_manager), l2cap_layer_handler_(l2cap_layer_handler) {}

  internal::FixedChannelServiceManagerImpl* service_manager_ = nullptr;
  internal::LinkManager* link_manager_ = nullptr;
  os::Handler* l2cap_layer_handler_ = nullptr;
  DISALLOW_COPY_AND_ASSIGN(FixedChannelManager);
};

}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
