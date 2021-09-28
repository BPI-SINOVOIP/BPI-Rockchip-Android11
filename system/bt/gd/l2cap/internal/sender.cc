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

#include <unordered_map>

#include "common/bind.h"
#include "l2cap/internal/basic_mode_channel_data_controller.h"
#include "l2cap/internal/enhanced_retransmission_mode_channel_data_controller.h"
#include "l2cap/internal/le_credit_based_channel_data_controller.h"
#include "l2cap/internal/scheduler.h"
#include "l2cap/internal/sender.h"
#include "os/handler.h"
#include "os/log.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

Sender::Sender(os::Handler* handler, ILink* link, Scheduler* scheduler, std::shared_ptr<ChannelImpl> channel)
    : handler_(handler), link_(link), queue_end_(channel->GetQueueDownEnd()), scheduler_(scheduler),
      channel_id_(channel->GetCid()), remote_channel_id_(channel->GetRemoteCid()),
      data_controller_(std::make_unique<BasicModeDataController>(channel_id_, remote_channel_id_, queue_end_, handler_,
                                                                 scheduler_)) {
  try_register_dequeue();
}

Sender::Sender(os::Handler* handler, ILink* link, Scheduler* scheduler, std::shared_ptr<ChannelImpl> channel,
               ChannelMode mode)
    : handler_(handler), link_(link), queue_end_(channel->GetQueueDownEnd()), scheduler_(scheduler),
      channel_id_(channel->GetCid()), remote_channel_id_(channel->GetRemoteCid()) {
  if (mode == ChannelMode::BASIC) {
    data_controller_ =
        std::make_unique<BasicModeDataController>(channel_id_, remote_channel_id_, queue_end_, handler_, scheduler_);
  } else if (mode == ChannelMode::ERTM) {
    data_controller_ =
        std::make_unique<ErtmController>(link_, channel_id_, remote_channel_id_, queue_end_, handler_, scheduler_);
  } else if (mode == ChannelMode::LE_CREDIT_BASED) {
    data_controller_ = std::make_unique<LeCreditBasedDataController>(link_, channel_id_, remote_channel_id_, queue_end_,
                                                                     handler_, scheduler_);
  }
  try_register_dequeue();
}

Sender::~Sender() {
  if (is_dequeue_registered_) {
    queue_end_->UnregisterDequeue();
  }
}

void Sender::OnPacketSent() {
  try_register_dequeue();
}

std::unique_ptr<Sender::UpperDequeue> Sender::GetNextPacket() {
  return data_controller_->GetNextPacket();
}

DataController* Sender::GetDataController() {
  return data_controller_.get();
}

void Sender::try_register_dequeue() {
  if (is_dequeue_registered_) {
    return;
  }
  queue_end_->RegisterDequeue(handler_, common::Bind(&Sender::dequeue_callback, common::Unretained(this)));
  is_dequeue_registered_ = true;
}

void Sender::dequeue_callback() {
  auto packet = queue_end_->TryDequeue();
  ASSERT(packet != nullptr);
  data_controller_->OnSdu(std::move(packet));
  queue_end_->UnregisterDequeue();
  is_dequeue_registered_ = false;
}

void Sender::UpdateClassicConfiguration(classic::internal::ChannelConfigurationState config) {
  auto mode = config.retransmission_and_flow_control_mode_;
  if (mode == mode_) {
    return;
  }
  if (mode == RetransmissionAndFlowControlModeOption::L2CAP_BASIC) {
    data_controller_ =
        std::make_unique<BasicModeDataController>(channel_id_, remote_channel_id_, queue_end_, handler_, scheduler_);
    return;
  }
  if (mode == RetransmissionAndFlowControlModeOption::ENHANCED_RETRANSMISSION) {
    data_controller_ =
        std::make_unique<ErtmController>(link_, channel_id_, remote_channel_id_, queue_end_, handler_, scheduler_);
    RetransmissionAndFlowControlConfigurationOption option = config.local_retransmission_and_flow_control_;
    option.tx_window_size_ = config.remote_retransmission_and_flow_control_.tx_window_size_;
    data_controller_->SetRetransmissionAndFlowControlOptions(option);
    data_controller_->EnableFcs(config.fcs_type_ == FcsType::DEFAULT);
    return;
  }
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
