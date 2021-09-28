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

#include "l2cap/internal/le_credit_based_channel_data_controller.h"

#include "l2cap/l2cap_packets.h"
#include "l2cap/le/internal/link.h"
#include "packet/fragmenting_inserter.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {

LeCreditBasedDataController::LeCreditBasedDataController(ILink* link, Cid cid, Cid remote_cid,
                                                         UpperQueueDownEnd* channel_queue_end, os::Handler* handler,
                                                         Scheduler* scheduler)
    : cid_(cid), remote_cid_(remote_cid), enqueue_buffer_(channel_queue_end), handler_(handler), scheduler_(scheduler),
      link_(link) {}

void LeCreditBasedDataController::OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) {
  auto sdu_size = sdu->size();
  if (sdu_size == 0) {
    LOG_WARN("Received empty SDU");
    return;
  }
  if (sdu_size > mtu_) {
    LOG_WARN("Received sdu_size %d > mtu %d", static_cast<int>(sdu_size), mtu_);
  }
  std::vector<std::unique_ptr<packet::RawBuilder>> segments;
  // TODO: We don't need to waste 2 bytes for continuation segment.
  packet::FragmentingInserter fragmenting_inserter(mps_ - 2, std::back_insert_iterator(segments));
  sdu->Serialize(fragmenting_inserter);
  fragmenting_inserter.finalize();
  std::unique_ptr<BasicFrameBuilder> builder;
  builder = FirstLeInformationFrameBuilder::Create(remote_cid_, sdu_size, std::move(segments[0]));
  pdu_queue_.emplace(std::move(builder));
  for (auto i = 1; i < segments.size(); i++) {
    builder = BasicFrameBuilder::Create(remote_cid_, std::move(segments[i]));
    pdu_queue_.emplace(std::move(builder));
  }
  if (credits_ >= segments.size()) {
    scheduler_->OnPacketsReady(cid_, segments.size());
    credits_ -= segments.size();
  } else if (credits_ > 0) {
    scheduler_->OnPacketsReady(cid_, credits_);
    pending_frames_count_ += (segments.size() - credits_);
    credits_ = 0;
  } else {
    pending_frames_count_ += segments.size();
  }
}

void LeCreditBasedDataController::OnPdu(packet::PacketView<true> pdu) {
  auto basic_frame_view = BasicFrameView::Create(pdu);
  if (!basic_frame_view.IsValid()) {
    LOG_WARN("Received invalid frame");
    return;
  }
  if (basic_frame_view.size() > mps_) {
    LOG_WARN("Received frame size %d > mps %d, dropping the packet", static_cast<int>(basic_frame_view.size()), mps_);
    return;
  }
  if (remaining_sdu_continuation_packet_size_ == 0) {
    auto start_frame_view = FirstLeInformationFrameView::Create(basic_frame_view);
    if (!start_frame_view.IsValid()) {
      LOG_WARN("Received invalid frame");
      return;
    }
    auto payload = start_frame_view.GetPayload();
    auto sdu_size = start_frame_view.GetL2capSduLength();
    remaining_sdu_continuation_packet_size_ = sdu_size - payload.size();
    reassembly_stage_ = payload;
  } else {
    auto payload = basic_frame_view.GetPayload();
    remaining_sdu_continuation_packet_size_ -= payload.size();
    reassembly_stage_.AppendPacketView(payload);
  }
  if (remaining_sdu_continuation_packet_size_ == 0) {
    enqueue_buffer_.Enqueue(std::make_unique<PacketView<kLittleEndian>>(reassembly_stage_), handler_);
  } else if (remaining_sdu_continuation_packet_size_ < 0 || reassembly_stage_.size() > mtu_) {
    LOG_WARN("Received larger SDU size than expected");
    reassembly_stage_ = PacketViewForReassembly(std::make_shared<std::vector<uint8_t>>());
    remaining_sdu_continuation_packet_size_ = 0;
    link_->SendDisconnectionRequest(cid_, remote_cid_);
  }
  // TODO: Improve the logic by sending credit only after user dequeued the SDU
  link_->SendLeCredit(cid_, 1);
}

std::unique_ptr<packet::BasePacketBuilder> LeCreditBasedDataController::GetNextPacket() {
  auto next = std::move(pdu_queue_.front());
  pdu_queue_.pop();
  return next;
}

void LeCreditBasedDataController::SetMtu(Mtu mtu) {
  mtu_ = mtu;
}

void LeCreditBasedDataController::SetMps(uint16_t mps) {
  mps_ = mps;
}

void LeCreditBasedDataController::OnCredit(uint16_t credits) {
  int total_credits = credits_ + credits;
  if (total_credits > 0xffff) {
    link_->SendDisconnectionRequest(cid_, remote_cid_);
  }
  credits_ = total_credits;
  if (pending_frames_count_ > 0 && credits_ >= pending_frames_count_) {
    scheduler_->OnPacketsReady(cid_, pending_frames_count_);
    credits_ -= pending_frames_count_;
  } else if (pending_frames_count_ > 0) {
    scheduler_->OnPacketsReady(cid_, credits_);
    pending_frames_count_ -= credits_;
    credits_ = 0;
  }
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
