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

#include "l2cap/l2cap_packets.h"
#include "l2cap/mtu.h"

namespace bluetooth {
namespace l2cap {
namespace classic {
namespace internal {

struct ChannelConfigurationState {
 public:
  enum State {
    /**
     * for the initiator path, a request has been sent but a positive response has not yet been received, and for the
     * acceptor path, a request with acceptable options has not yet been received.
     */
    WAIT_CONFIG_REQ_RSP,
    /**
     * the acceptor path is complete after having responded to acceptable options, but for the initiator path, a
     * positive response on the recent request has not yet been received.
     */
    WAIT_CONFIG_RSP,
    /**
     * the initiator path is complete after having received a positive response, but for the acceptor path, a request
     * with acceptable options has not yet been received.
     */
    WAIT_CONFIG_REQ,
    /**
     * Configuration is complete
     */
    CONFIGURED,
  };
  State state_ = State::WAIT_CONFIG_REQ_RSP;

  Mtu incoming_mtu_ = kDefaultClassicMtu;
  Mtu outgoing_mtu_ = kDefaultClassicMtu;
  RetransmissionAndFlowControlModeOption retransmission_and_flow_control_mode_;
  RetransmissionAndFlowControlConfigurationOption local_retransmission_and_flow_control_;
  RetransmissionAndFlowControlConfigurationOption remote_retransmission_and_flow_control_;
  FcsType fcs_type_ = FcsType::DEFAULT;
};
}  // namespace internal
}  // namespace classic
}  // namespace l2cap
}  // namespace bluetooth
