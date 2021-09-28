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

#include "l2cap/internal/basic_mode_channel_data_controller.h"

#include "l2cap/l2cap_packets.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

BasicModeDataController::BasicModeDataController(Cid cid, Cid remote_cid, UpperQueueDownEnd* channel_queue_end,
                                                 os::Handler* handler, Scheduler* scheduler)
    : cid_(cid), remote_cid_(remote_cid), enqueue_buffer_(channel_queue_end), handler_(handler), scheduler_(scheduler) {
}

void BasicModeDataController::OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) {
  auto l2cap_information = BasicFrameBuilder::Create(remote_cid_, std::move(sdu));
  pdu_queue_.emplace(std::move(l2cap_information));
  scheduler_->OnPacketsReady(cid_, 1);
}

void BasicModeDataController::OnPdu(packet::PacketView<true> pdu) {
  auto basic_frame_view = BasicFrameView::Create(pdu);
  if (!basic_frame_view.IsValid()) {
    LOG_WARN("Received invalid frame");
    return;
  }
  enqueue_buffer_.Enqueue(std::make_unique<PacketView<kLittleEndian>>(basic_frame_view.GetPayload()), handler_);
}

std::unique_ptr<packet::BasePacketBuilder> BasicModeDataController::GetNextPacket() {
  auto next = std::move(pdu_queue_.front());
  pdu_queue_.pop();
  return next;
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
