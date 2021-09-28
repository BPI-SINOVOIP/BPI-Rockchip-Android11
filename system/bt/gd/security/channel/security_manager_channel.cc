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
#include "security/channel/security_manager_channel.h"

#include "security/smp_packets.h"

namespace bluetooth {
namespace security {
namespace channel {

void SecurityManagerChannel::OnCommandComplete(hci::CommandCompleteView packet) {
  ASSERT(packet.IsValid());
  // TODO(optedoblivion): Verify HCI commands
}

void SecurityManagerChannel::SendCommand(std::unique_ptr<hci::SecurityCommandBuilder> command) {
  hci_security_interface_->EnqueueCommand(
      std::move(command), common::BindOnce(&SecurityManagerChannel::OnCommandComplete, common::Unretained(this)),
      handler_);
}

void SecurityManagerChannel::OnHciEventReceived(hci::EventPacketView packet) {
  ASSERT_LOG(listener_ != nullptr, "No listener set!");
  ASSERT(packet.IsValid());
  listener_->OnHciEventReceived(packet);
}

}  // namespace channel
}  // namespace security
}  // namespace bluetooth
