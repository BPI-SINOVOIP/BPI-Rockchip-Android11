/*
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0;
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
#include "hci/hci_layer.h"
#include "hci/hci_packets.h"
#include "hci/security_interface.h"

namespace bluetooth {
namespace security {
namespace channel {

/**
 * Interface for listening to the channel for SMP commands.
 */
class ISecurityManagerChannelListener {
 public:
  virtual ~ISecurityManagerChannelListener() = default;
  virtual void OnHciEventReceived(hci::EventPacketView packet) = 0;
};

/**
 * Channel for consolidating traffic and making the transport agnostic.
 */
class SecurityManagerChannel {
 public:
  explicit SecurityManagerChannel(os::Handler* handler, hci::HciLayer* hci_layer)
      : listener_(nullptr),
        hci_security_interface_(hci_layer->GetSecurityInterface(
            common::Bind(&SecurityManagerChannel::OnHciEventReceived, common::Unretained(this)), handler)),
        handler_(handler) {}

  /**
   * Send a given SMP command over the SecurityManagerChannel
   *
   * @param command smp command to send
   */
  void SendCommand(std::unique_ptr<hci::SecurityCommandBuilder> command);

  /**
   * Sets the listener to listen for channel events
   *
   * @param listener the caller interested in events
   */
  void SetChannelListener(ISecurityManagerChannelListener* listener) {
    listener_ = listener;
  }

  /**
   * Called when an incoming HCI event happens
   *
   * @param event_packet
   */
  void OnHciEventReceived(hci::EventPacketView packet);

  /**
   * Called when an HCI command is completed
   *
   * @param on_complete
   */
  void OnCommandComplete(hci::CommandCompleteView packet);

 private:
  ISecurityManagerChannelListener* listener_;
  hci::SecurityInterface* hci_security_interface_;
  os::Handler* handler_;
};

}  // namespace channel
}  // namespace security
}  // namespace bluetooth
