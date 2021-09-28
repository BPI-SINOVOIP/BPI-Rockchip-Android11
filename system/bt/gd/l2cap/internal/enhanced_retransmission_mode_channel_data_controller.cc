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

#include "l2cap/internal/enhanced_retransmission_mode_channel_data_controller.h"

#include <map>
#include <queue>
#include <vector>

#include "common/bind.h"
#include "l2cap/internal/ilink.h"
#include "os/alarm.h"
#include "packet/fragmenting_inserter.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
ErtmController::ErtmController(ILink* link, Cid cid, Cid remote_cid, UpperQueueDownEnd* channel_queue_end,
                               os::Handler* handler, Scheduler* scheduler)
    : link_(link), cid_(cid), remote_cid_(remote_cid), enqueue_buffer_(channel_queue_end), handler_(handler),
      scheduler_(scheduler), pimpl_(std::make_unique<impl>(this, handler)) {}

ErtmController::~ErtmController() = default;

struct ErtmController::impl {
  impl(ErtmController* controller, os::Handler* handler)
      : controller_(controller), handler_(handler), retrans_timer_(handler), monitor_timer_(handler) {}

  ErtmController* controller_;
  os::Handler* handler_;

  // We don't support extended window
  static constexpr uint8_t kMaxTxWin = 64;

  // We don't support sending SREJ
  static constexpr bool kSendSrej = false;

  // States (@see 8.6.5.2): Transmitter state and receiver state

  enum class TxState {
    XMIT,
    WAIT_F,
  };
  TxState tx_state_ = TxState::XMIT;

  enum class RxState {
    RECV,
    REJ_SENT,
    SREJ_SENT,
  };
  RxState rx_state_ = RxState::RECV;

  // Variables and Timers (@see 8.6.5.3)

  uint8_t tx_seq_ = 0;
  uint8_t next_tx_seq_ = 0;
  uint8_t expected_ack_seq_ = 0;
  uint8_t req_seq_ = 0;
  uint8_t expected_tx_seq_ = 0;
  uint8_t buffer_seq_ = 0;

  bool remote_busy_ = false;
  bool local_busy_ = false;
  int unacked_frames_ = 0;
  // TODO: Instead of having a map, we may consider about a better data structure
  // Map from TxSeq to (SAR, SDU size for START packet, information payload)
  std::map<uint8_t, std::tuple<SegmentationAndReassembly, uint16_t, std::shared_ptr<packet::RawBuilder>>> unacked_list_;
  // Stores (SAR, SDU size for START packet, information payload)
  std::queue<std::tuple<SegmentationAndReassembly, uint16_t, std::unique_ptr<packet::RawBuilder>>> pending_frames_;
  int retry_count_ = 0;
  std::map<uint8_t /* tx_seq, */, int /* count */> retry_i_frames_;
  bool rnr_sent_ = false;
  bool rej_actioned_ = false;
  bool srej_actioned_ = false;
  uint16_t srej_save_req_seq_ = 0;
  bool send_rej_ = false;
  int buffer_seq_srej_ = 0;
  int frames_sent_ = 0;
  os::Alarm retrans_timer_;
  os::Alarm monitor_timer_;

  // Events (@see 8.6.5.4)

  void data_request(SegmentationAndReassembly sar, std::unique_ptr<packet::RawBuilder> pdu, uint16_t sdu_size = 0) {
    // Note: sdu_size only applies to START packet
    if (tx_state_ == TxState::XMIT && !remote_busy() && rem_window_not_full()) {
      send_data(sar, sdu_size, std::move(pdu));
    } else if (tx_state_ == TxState::XMIT && (remote_busy() || rem_window_full())) {
      pend_data(sar, sdu_size, std::move(pdu));
    } else if (tx_state_ == TxState::WAIT_F) {
      pend_data(sar, sdu_size, std::move(pdu));
    }
  }

  void local_busy_detected() {
    local_busy_ = true;
  }

  void local_busy_clear() {
    if (tx_state_ == TxState::XMIT && rnr_sent()) {
      local_busy_ = false;
      rnr_sent_ = false;
      send_rr(Poll::POLL);
      retry_count_ = 1;
      stop_retrans_timer();
      start_monitor_timer();
    } else if (tx_state_ == TxState::XMIT) {
      local_busy_ = false;
      rnr_sent_ = false;
    }
  }

  void recv_req_seq_and_f_bit(uint8_t req_seq, Final f) {
    if (tx_state_ == TxState::XMIT) {
      process_req_seq(req_seq);
    } else if (f == Final::POLL_RESPONSE) {
      process_req_seq(req_seq);
      stop_monitor_timer();
      if (unacked_frames_ > 0) {
        start_retrans_timer();
      }
      tx_state_ = TxState::XMIT;
    } else {
      process_req_seq(req_seq);
    }
  }

  void recv_f_bit(Final f) {
    if (tx_state_ == TxState::WAIT_F && f == Final::POLL_RESPONSE) {
      stop_monitor_timer();
      if (unacked_frames_ > 0) {
        start_retrans_timer();
      }
      tx_state_ = TxState::XMIT;
    }
  }

  void retrans_timer_expires() {
    if (tx_state_ == TxState::XMIT) {
      send_rr_or_rnr(Poll::POLL);
      // send rr or rnr(p=1)
      retry_count_ = 1;
      start_monitor_timer();
      tx_state_ = TxState::WAIT_F;
    }
  }

  void monitor_timer_expires() {
    if (tx_state_ == TxState::WAIT_F && retry_count_less_than_max_transmit()) {
      retry_count_++;
      send_rr_or_rnr(Poll::POLL);
      start_monitor_timer();
    } else if (tx_state_ == TxState::WAIT_F) {
      CloseChannel();
    }
  }

  void recv_i_frame(Final f, uint8_t tx_seq, uint8_t req_seq, SegmentationAndReassembly sar, uint16_t sdu_size,
                    const packet::PacketView<true>& payload) {
    if (rx_state_ == RxState::RECV) {
      if (f == Final::NOT_SET && with_expected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f) &&
          !local_busy()) {
        increment_expected_tx_seq();
        pass_to_tx(req_seq, f);
        data_indication(sar, sdu_size, payload);
        send_ack(Final::NOT_SET);
      } else if (f == Final::POLL_RESPONSE && with_expected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) &&
                 with_valid_f_bit(f) && !local_busy()) {
        increment_expected_tx_seq();
        pass_to_tx(req_seq, f);
        data_indication(sar, sdu_size, payload);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq);
          send_pending_i_frames();
        } else {
          rej_actioned_ = false;
        }
        send_ack(Final::NOT_SET);
      } else if (with_duplicate_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f) && !local_busy()) {
        pass_to_tx(req_seq, f);
      } else if (with_unexpected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f) &&
                 !local_busy()) {
        if constexpr (kSendSrej) {
          // We don't support sending SREJ
        } else {
          pass_to_tx(req_seq, f);
          send_rej();
          rx_state_ = RxState::REJ_SENT;
        }
      } else if (with_expected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f) && local_busy()) {
        pass_to_tx(req_seq, f);
        store_or_ignore();
      } else if (with_valid_req_seq(req_seq) && not_with_expected_tx_seq(tx_seq) && with_valid_f_bit(f) &&
                 local_busy()) {
        pass_to_tx(req_seq, f);
      } else if ((with_invalid_tx_seq(tx_seq) && controller_->local_tx_window_ > kMaxTxWin / 2) ||
                 with_invalid_req_seq(req_seq)) {
        CloseChannel();
      } else if (with_invalid_tx_seq(tx_seq) && controller_->local_tx_window_ <= kMaxTxWin / 2) {
        // We decided to ignore
      }
    } else if (rx_state_ == RxState::REJ_SENT) {
      if (f == Final::NOT_SET && with_expected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        increment_expected_tx_seq();
        pass_to_tx(req_seq, f);
        data_indication(sar, sdu_size, payload);
        send_ack(Final::NOT_SET);
        rx_state_ = RxState::RECV;
      } else if (f == Final::POLL_RESPONSE && with_expected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) &&
                 with_valid_f_bit(f)) {
        increment_expected_tx_seq();
        pass_to_tx(req_seq, f);
        data_indication(sar, sdu_size, payload);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq);
          send_pending_i_frames();
        } else {
          rej_actioned_ = false;
        }
        send_ack(Final::NOT_SET);
        rx_state_ = RxState::RECV;
      } else if (with_unexpected_tx_seq(tx_seq) && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        pass_to_tx(req_seq, f);
      }
    } else if (rx_state_ == RxState::SREJ_SENT) {
      // SREJ NOT SUPPORTED
    }
  }

  void recv_rr(uint8_t req_seq, Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    if (rx_state_ == RxState::RECV) {
      if (p == Poll::NOT_SET && f == Final::NOT_SET && with_valid_req_seq_rr(req_seq) && with_valid_f_bit(f)) {
        pass_to_tx(req_seq, f);
        if (remote_busy() && unacked_frames_ > 0) {
          start_retrans_timer();
        }
        remote_busy_ = false;
        send_pending_i_frames();
      } else if (f == Final::POLL_RESPONSE && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq, p);
        } else {
          rej_actioned_ = false;
        }
        send_pending_i_frames();
      } else if (p == Poll::POLL && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        pass_to_tx(req_seq, f);
        send_i_or_rr_or_rnr(Final::POLL_RESPONSE);
      } else if (with_invalid_req_seq_rr(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::REJ_SENT) {
      if (f == Final::POLL_RESPONSE && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq, p);
        } else {
          rej_actioned_ = false;
        }
        send_pending_i_frames();
      } else if (p == Poll::NOT_SET && f == Final::NOT_SET && with_valid_req_seq_rr(req_seq) && with_valid_f_bit(f)) {
        pass_to_tx(req_seq, f);
        if (remote_busy() and unacked_frames_ > 0) {
          start_retrans_timer();
        }
        remote_busy_ = false;
        send_ack(Final::NOT_SET);
      } else if (p == Poll::POLL && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        pass_to_tx(req_seq, f);
        if (remote_busy() and unacked_frames_ > 0) {
          start_retrans_timer();
        }
        remote_busy_ = false;
        send_rr(Final::POLL_RESPONSE);
      } else if (with_invalid_req_seq_rr(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::SREJ_SENT) {
      // SREJ NOT SUPPORTED
    }
  }

  void recv_rej(uint8_t req_seq, Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    if (rx_state_ == RxState::RECV) {
      if (f == Final::NOT_SET && with_valid_req_seq_retrans(req_seq) &&
          retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        retransmit_i_frames(req_seq, p);
        send_pending_i_frames();
        if (p_bit_outstanding()) {
          rej_actioned_ = true;
        }
      } else if (f == Final::POLL_RESPONSE && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq, p);
        } else {
          rej_actioned_ = false;
        }
        send_pending_i_frames();
      } else if (with_valid_req_seq_retrans(req_seq) && !retry_i_frames_less_than_max_transmit(req_seq)) {
        CloseChannel();
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::REJ_SENT) {
      if (f == Final::NOT_SET && with_valid_req_seq_retrans(req_seq) &&
          retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        retransmit_i_frames(req_seq, p);
        send_pending_i_frames();
        if (p_bit_outstanding()) {
          rej_actioned_ = true;
        }
      } else if (f == Final::POLL_RESPONSE && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        if (!rej_actioned_) {
          retransmit_i_frames(req_seq, p);
        } else {
          rej_actioned_ = false;
        }
        send_pending_i_frames();
      } else if (with_valid_req_seq_retrans(req_seq) && !retry_i_frames_less_than_max_transmit(req_seq)) {
        CloseChannel();
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::SREJ_SENT) {
      // SREJ NOT SUPPORTED
    }
  }

  void recv_rnr(uint8_t req_seq, Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    if (rx_state_ == RxState::RECV) {
      if (p == Poll::NOT_SET && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = true;
        pass_to_tx(req_seq, f);
        stop_retrans_timer();
      } else if (p == Poll::POLL && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = true;
        pass_to_tx(req_seq, f);
        stop_retrans_timer();
        send_rr_or_rnr(Poll::NOT_SET, Final::POLL_RESPONSE);
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::REJ_SENT) {
      if (p == Poll::NOT_SET && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = true;
        pass_to_tx(req_seq, f);
        send_rr(Final::POLL_RESPONSE);
      } else if (p == Poll::POLL && with_valid_req_seq(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = true;
        pass_to_tx(req_seq, f);
        send_rr(Final::NOT_SET);
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::SREJ_SENT) {
      // SREJ NOT SUPPORTED
    }
  }

  void recv_srej(uint8_t req_seq, Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    if (rx_state_ == RxState::RECV) {
      if (p == Poll::NOT_SET && f == Final::NOT_SET && with_valid_req_seq_retrans(req_seq) &&
          retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx_f_bit(f);
        retransmit_requested_i_frame(req_seq, p);
        if (p_bit_outstanding()) {
          srej_actioned_ = true;
          srej_save_req_seq_ = req_seq;
        }
      } else if (f == Final::POLL_RESPONSE && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx_f_bit(f);
        if (srej_actioned_ && srej_save_req_seq_ == req_seq) {
          srej_actioned_ = false;
        } else {
          retransmit_requested_i_frame(req_seq, p);
        }
      } else if (p == Poll::POLL && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        retransmit_requested_i_frame(req_seq, p);
        if (p_bit_outstanding()) {
          srej_actioned_ = true;
          srej_save_req_seq_ = req_seq;
        }
      } else if (with_valid_req_seq_retrans(req_seq) && !retry_i_frames_less_than_max_transmit(req_seq)) {
        CloseChannel();
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::REJ_SENT) {
      if (p == Poll::NOT_SET && f == Final::NOT_SET && with_valid_req_seq_retrans(req_seq) &&
          retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx_f_bit(f);
        retransmit_requested_i_frame(req_seq, p);
        if (p_bit_outstanding()) {
          srej_actioned_ = true;
          srej_save_req_seq_ = req_seq;
        }
      } else if (f == Final::POLL_RESPONSE && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx_f_bit(f);
        if (srej_actioned_ && srej_save_req_seq_ == req_seq) {
          srej_actioned_ = false;
        } else {
          retransmit_requested_i_frame(req_seq, p);
        }
      } else if (p == Poll::POLL && with_valid_req_seq_retrans(req_seq) &&
                 retry_i_frames_less_than_max_transmit(req_seq) && with_valid_f_bit(f)) {
        remote_busy_ = false;
        pass_to_tx(req_seq, f);
        retransmit_requested_i_frame(req_seq, p);
        send_pending_i_frames();
        if (p_bit_outstanding()) {
          srej_actioned_ = true;
          srej_save_req_seq_ = req_seq;
        }
      } else if (with_valid_req_seq_retrans(req_seq) && !retry_i_frames_less_than_max_transmit(req_seq)) {
        CloseChannel();
      } else if (with_invalid_req_seq_retrans(req_seq)) {
        CloseChannel();
      }
    } else if (rx_state_ == RxState::SREJ_SENT) {
      // SREJ NOT SUPPORTED
    }
  }

  // Conditions (@see 8.6.5.5)
  bool remote_busy() {
    return remote_busy_;
  }

  bool local_busy() {
    return local_busy_;
  }

  bool rem_window_not_full() {
    return unacked_frames_ < controller_->remote_tx_window_;
  }

  bool rem_window_full() {
    return unacked_frames_ == controller_->remote_tx_window_;
  }

  bool rnr_sent() {
    return rnr_sent_;
  }

  bool retry_i_frames_less_than_max_transmit(uint8_t req_seq) {
    return retry_i_frames_[req_seq] < controller_->local_max_transmit_;
  }

  bool retry_count_less_than_max_transmit() {
    return retry_count_ < controller_->local_max_transmit_;
  }

  bool with_expected_tx_seq(uint8_t tx_seq) {
    return tx_seq == expected_tx_seq_;
  }

  bool with_valid_req_seq(uint8_t req_seq) {
    return expected_ack_seq_ <= req_seq && req_seq <= next_tx_seq_;
  }

  bool with_valid_req_seq_retrans(uint8_t req_seq) {
    return expected_ack_seq_ <= req_seq && req_seq <= next_tx_seq_;
  }

  bool with_valid_f_bit(Final f) {
    return f == Final::NOT_SET ^ tx_state_ == TxState::WAIT_F;
  }

  bool with_unexpected_tx_seq(uint8_t tx_seq) {
    return tx_seq > expected_tx_seq_ && tx_seq <= expected_tx_seq_ + controller_->local_tx_window_;
  }

  bool with_duplicate_tx_seq(uint8_t tx_seq) {
    return tx_seq < expected_tx_seq_ && tx_seq >= expected_tx_seq_ - controller_->local_tx_window_;
  }

  bool with_invalid_tx_seq(uint8_t tx_seq) {
    return tx_seq < expected_tx_seq_ - controller_->local_tx_window_ ||
           tx_seq > expected_tx_seq_ + controller_->local_tx_window_;
  }

  bool with_invalid_req_seq(uint8_t req_seq) {
    return req_seq < expected_ack_seq_ || req_seq > next_tx_seq_;
  }

  bool with_invalid_req_seq_retrans(uint8_t req_seq) {
    return req_seq < expected_ack_seq_ || req_seq >= next_tx_seq_;
  }

  bool not_with_expected_tx_seq(uint8_t tx_seq) {
    return !with_invalid_tx_seq(tx_seq) && !with_expected_tx_seq(tx_seq);
  }

  bool with_valid_req_seq_rr(uint8_t req_seq) {
    return expected_ack_seq_ < req_seq && req_seq <= next_tx_seq_;
  }

  bool with_invalid_req_seq_rr(uint8_t req_seq) {
    return req_seq <= expected_ack_seq_ || req_seq > next_tx_seq_;
  }

  bool with_expected_tx_seq_srej() {
    // We don't support sending SREJ
    return false;
  }

  bool send_req_is_true() {
    // We don't support sending SREJ
    return false;
  }

  bool srej_list_is_one() {
    // We don't support sending SREJ
    return false;
  }

  bool with_unexpected_tx_seq_srej() {
    // We don't support sending SREJ
    return false;
  }

  bool with_duplicate_tx_seq_srej() {
    // We don't support sending SREJ
    return false;
  }

  // Actions (@see 8.6.5.6)

  void _send_i_frame(SegmentationAndReassembly sar, std::unique_ptr<CopyablePacketBuilder> segment, uint8_t req_seq,
                     uint8_t tx_seq, uint16_t sdu_size = 0, Final f = Final::NOT_SET) {
    std::unique_ptr<packet::BasePacketBuilder> builder;
    if (sar == SegmentationAndReassembly::START) {
      if (controller_->fcs_enabled_) {
        builder = EnhancedInformationStartFrameWithFcsBuilder::Create(controller_->remote_cid_, tx_seq, f, req_seq,
                                                                      sdu_size, std::move(segment));
      } else {
        builder = EnhancedInformationStartFrameBuilder::Create(controller_->remote_cid_, tx_seq, f, req_seq, sdu_size,
                                                               std::move(segment));
      }
    } else {
      if (controller_->fcs_enabled_) {
        builder = EnhancedInformationFrameWithFcsBuilder::Create(controller_->remote_cid_, tx_seq, f, req_seq, sar,
                                                                 std::move(segment));
      } else {
        builder = EnhancedInformationFrameBuilder::Create(controller_->remote_cid_, tx_seq, f, req_seq, sar,
                                                          std::move(segment));
      }
    }
    controller_->send_pdu(std::move(builder));
  }

  void send_data(SegmentationAndReassembly sar, uint16_t sdu_size, std::unique_ptr<packet::RawBuilder> segment,
                 Final f = Final::NOT_SET) {
    std::shared_ptr<packet::RawBuilder> shared_segment(segment.release());
    unacked_list_.emplace(std::piecewise_construct, std::forward_as_tuple(next_tx_seq_),
                          std::forward_as_tuple(sar, sdu_size, shared_segment));

    std::unique_ptr<CopyablePacketBuilder> copyable_packet_builder =
        std::make_unique<CopyablePacketBuilder>(std::get<2>(unacked_list_.find(next_tx_seq_)->second));
    _send_i_frame(sar, std::move(copyable_packet_builder), buffer_seq_, next_tx_seq_, sdu_size, f);
    unacked_frames_++;
    frames_sent_++;
    retry_i_frames_[next_tx_seq_] = 1;
    next_tx_seq_ = (next_tx_seq_ + 1) % kMaxTxWin;
    start_retrans_timer();
  }

  void pend_data(SegmentationAndReassembly sar, uint16_t sdu_size, std::unique_ptr<packet::RawBuilder> data) {
    pending_frames_.emplace(std::make_tuple(sar, sdu_size, std::move(data)));
  }

  void process_req_seq(uint8_t req_seq) {
    for (int i = expected_ack_seq_; i < req_seq; i++) {
      unacked_list_.erase(i);
      retry_i_frames_[i] = 0;
    }
    unacked_frames_ -= ((req_seq - expected_ack_seq_) + kMaxTxWin) % kMaxTxWin;
    if (unacked_frames_ == 0) {
      stop_retrans_timer();
    }
  }

  void _send_s_frame(SupervisoryFunction s, uint8_t req_seq, Poll p, Final f) {
    std::unique_ptr<packet::BasePacketBuilder> builder;
    if (controller_->fcs_enabled_) {
      builder = EnhancedSupervisoryFrameWithFcsBuilder::Create(controller_->remote_cid_, s, p, f, req_seq);
    } else {
      builder = EnhancedSupervisoryFrameBuilder::Create(controller_->remote_cid_, s, p, f, req_seq);
    }
    controller_->send_pdu(std::move(builder));
  }

  void send_rr(Poll p) {
    _send_s_frame(SupervisoryFunction::RECEIVER_READY, expected_tx_seq_, p, Final::NOT_SET);
  }

  void send_rr(Final f) {
    _send_s_frame(SupervisoryFunction::RECEIVER_READY, expected_tx_seq_, Poll::NOT_SET, f);
  }

  void send_rnr(Poll p) {
    _send_s_frame(SupervisoryFunction::RECEIVER_NOT_READY, expected_tx_seq_, p, Final::NOT_SET);
  }

  void send_rnr(Final f) {
    _send_s_frame(SupervisoryFunction::RECEIVER_NOT_READY, expected_tx_seq_, Poll::NOT_SET, f);
  }

  void send_rej(Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    _send_s_frame(SupervisoryFunction::REJECT, expected_tx_seq_, p, f);
  }

  void send_rr_or_rnr(Poll p = Poll::NOT_SET, Final f = Final::NOT_SET) {
    if (local_busy()) {
      _send_s_frame(SupervisoryFunction::RECEIVER_NOT_READY, buffer_seq_, p, f);
    } else {
      _send_s_frame(SupervisoryFunction::RECEIVER_READY, buffer_seq_, p, f);
    }
  }

  void send_i_or_rr_or_rnr(Final f = Final::POLL_RESPONSE) {
    auto frames_sent = 0;
    if (local_busy()) {
      send_rnr(Final::POLL_RESPONSE);
    }
    if (remote_busy() && unacked_frames_ > 0) {
      start_retrans_timer();
    }
    remote_busy_ = false;
    send_pending_i_frames(f);  // TODO: Only first has f = 1, other f = 0. Also increase frames_sent
    if (!local_busy() && frames_sent == 0) {
      send_rr(Final::POLL_RESPONSE);
    }
  }

  void send_srej() {
    // Sending SREJ is not supported
  }

  void start_retrans_timer() {
    retrans_timer_.Schedule(common::BindOnce(&impl::retrans_timer_expires, common::Unretained(this)),
                            std::chrono::milliseconds(controller_->local_retransmit_timeout_ms_));
  }

  void start_monitor_timer() {
    monitor_timer_.Schedule(common::BindOnce(&impl::monitor_timer_expires, common::Unretained(this)),
                            std::chrono::milliseconds(controller_->local_monitor_timeout_ms_));
  }

  void pass_to_tx(uint8_t req_seq, Final f) {
    recv_req_seq_and_f_bit(req_seq, f);
  }

  void pass_to_tx_f_bit(Final f) {
    recv_f_bit(f);
  }

  void data_indication(SegmentationAndReassembly sar, uint16_t sdu_size, const packet::PacketView<true>& segment) {
    controller_->stage_for_reassembly(sar, sdu_size, segment);
    buffer_seq_ = (buffer_seq_ + 1) % kMaxTxWin;
  }

  void increment_expected_tx_seq() {
    expected_tx_seq_ = (expected_tx_seq_ + 1) % kMaxTxWin;
  }

  void stop_retrans_timer() {
    retrans_timer_.Cancel();
  }

  void stop_monitor_timer() {
    monitor_timer_.Cancel();
  }

  void send_ack(Final f = Final::NOT_SET) {
    if (local_busy()) {
      send_rnr(f);
    } else if (!remote_busy() && !pending_frames_.empty() && rem_window_not_full()) {
      send_pending_i_frames(f);
    } else {
      send_rr(f);
    }
  }

  void init_srej() {
    // We don't support sending SREJ
  }

  void save_i_frame_srej() {
    // We don't support sending SREJ
  }

  void store_or_ignore() {
    // We choose to ignore. We don't support local busy so far.
  }

  bool p_bit_outstanding() {
    return tx_state_ == TxState::WAIT_F;
  }

  void retransmit_i_frames(uint8_t req_seq, Poll p = Poll::NOT_SET) {
    uint8_t i = req_seq;
    Final f = (p == Poll::NOT_SET ? Final::NOT_SET : Final::POLL_RESPONSE);
    while (unacked_list_.find(i) != unacked_list_.end()) {
      if (retry_i_frames_[i] == controller_->local_max_transmit_) {
        CloseChannel();
        return;
      }
      std::unique_ptr<CopyablePacketBuilder> copyable_packet_builder =
          std::make_unique<CopyablePacketBuilder>(std::get<2>(unacked_list_.find(i)->second));
      _send_i_frame(std::get<0>(unacked_list_.find(i)->second), std::move(copyable_packet_builder), buffer_seq_, i,
                    std::get<1>(unacked_list_.find(i)->second), f);
      retry_i_frames_[i]++;
      frames_sent_++;
      f = Final::NOT_SET;
      i++;
    }
    if (i != req_seq) {
      start_retrans_timer();
    }
  }

  void retransmit_requested_i_frame(uint8_t req_seq, Poll p) {
    Final f = p == Poll::POLL ? Final::POLL_RESPONSE : Final::NOT_SET;
    if (unacked_list_.find(req_seq) == unacked_list_.end()) {
      LOG_ERROR("Received invalid SREJ");
      return;
    }
    std::unique_ptr<CopyablePacketBuilder> copyable_packet_builder =
        std::make_unique<CopyablePacketBuilder>(std::get<2>(unacked_list_.find(req_seq)->second));
    _send_i_frame(std::get<0>(unacked_list_.find(req_seq)->second), std::move(copyable_packet_builder), buffer_seq_,
                  req_seq, std::get<1>(unacked_list_.find(req_seq)->second), f);
    retry_i_frames_[req_seq]++;
    start_retrans_timer();
  }

  void send_pending_i_frames(Final f = Final::NOT_SET) {
    if (p_bit_outstanding()) {
      return;
    }
    while (rem_window_not_full() && !pending_frames_.empty()) {
      auto& frame = pending_frames_.front();
      send_data(std::get<0>(frame), std::get<1>(frame), std::move(std::get<2>(frame)), f);
      pending_frames_.pop();
      f = Final::NOT_SET;
    }
  }

  void CloseChannel() {
    controller_->close_channel();
  }

  void pop_srej_list() {
    // We don't support sending SREJ
  }

  void data_indication_srej() {
    // We don't support sending SREJ
  }
};

// Segmentation is handled here
void ErtmController::OnSdu(std::unique_ptr<packet::BasePacketBuilder> sdu) {
  auto sdu_size = sdu->size();
  std::vector<std::unique_ptr<packet::RawBuilder>> segments;
  packet::FragmentingInserter fragmenting_inserter(size_each_packet_, std::back_insert_iterator(segments));
  sdu->Serialize(fragmenting_inserter);
  fragmenting_inserter.finalize();
  if (segments.size() == 1) {
    pimpl_->data_request(SegmentationAndReassembly::UNSEGMENTED, std::move(segments[0]));
    return;
  }
  pimpl_->data_request(SegmentationAndReassembly::START, std::move(segments[0]), sdu_size);
  for (auto i = 1; i < segments.size() - 1; i++) {
    pimpl_->data_request(SegmentationAndReassembly::CONTINUATION, std::move(segments[i]));
  }
  pimpl_->data_request(SegmentationAndReassembly::END, std::move(segments.back()));
}

void ErtmController::OnPdu(packet::PacketView<true> pdu) {
  if (fcs_enabled_) {
    on_pdu_fcs(pdu);
  } else {
    on_pdu_no_fcs(pdu);
  }
}

void ErtmController::on_pdu_no_fcs(const packet::PacketView<true>& pdu) {
  auto basic_frame_view = BasicFrameView::Create(pdu);
  if (!basic_frame_view.IsValid()) {
    return;
  }
  auto standard_frame_view = StandardFrameView::Create(basic_frame_view);
  if (!standard_frame_view.IsValid()) {
    LOG_WARN("Received invalid frame");
    return;
  }
  auto type = standard_frame_view.GetFrameType();
  if (type == FrameType::I_FRAME) {
    auto i_frame_view = EnhancedInformationFrameView::Create(standard_frame_view);
    if (!i_frame_view.IsValid()) {
      LOG_WARN("Received invalid frame");
      return;
    }
    Final f = i_frame_view.GetF();
    uint8_t tx_seq = i_frame_view.GetTxSeq();
    uint8_t req_seq = i_frame_view.GetReqSeq();
    auto sar = i_frame_view.GetSar();
    if (sar == SegmentationAndReassembly::START) {
      auto i_frame_start_view = EnhancedInformationStartFrameView::Create(i_frame_view);
      if (!i_frame_start_view.IsValid()) {
        LOG_WARN("Received invalid I-Frame START");
        return;
      }
      pimpl_->recv_i_frame(f, tx_seq, req_seq, sar, i_frame_start_view.GetL2capSduLength(),
                           i_frame_start_view.GetPayload());
    } else {
      pimpl_->recv_i_frame(f, tx_seq, req_seq, sar, 0, i_frame_view.GetPayload());
    }
  } else if (type == FrameType::S_FRAME) {
    auto s_frame_view = EnhancedSupervisoryFrameView::Create(standard_frame_view);
    if (!s_frame_view.IsValid()) {
      LOG_WARN("Received invalid frame");
      return;
    }
    auto req_seq = s_frame_view.GetReqSeq();
    auto f = s_frame_view.GetF();
    auto p = s_frame_view.GetP();
    switch (s_frame_view.GetS()) {
      case SupervisoryFunction::RECEIVER_READY:
        pimpl_->recv_rr(req_seq, p, f);
        break;
      case SupervisoryFunction::RECEIVER_NOT_READY:
        pimpl_->recv_rnr(req_seq, p, f);
        break;
      case SupervisoryFunction::REJECT:
        pimpl_->recv_rej(req_seq, p, f);
        break;
      case SupervisoryFunction::SELECT_REJECT:
        pimpl_->recv_srej(req_seq, p, f);
        break;
    }
  } else {
    LOG_WARN("Received invalid frame");
  }
}

void ErtmController::on_pdu_fcs(const packet::PacketView<true>& pdu) {
  auto basic_frame_view = BasicFrameWithFcsView::Create(pdu);
  if (!basic_frame_view.IsValid()) {
    return;
  }
  auto standard_frame_view = StandardFrameWithFcsView::Create(basic_frame_view);
  if (!standard_frame_view.IsValid()) {
    LOG_WARN("Received invalid frame");
    return;
  }
  auto type = standard_frame_view.GetFrameType();
  if (type == FrameType::I_FRAME) {
    auto i_frame_view = EnhancedInformationFrameWithFcsView::Create(standard_frame_view);
    if (!i_frame_view.IsValid()) {
      LOG_WARN("Received invalid frame");
      return;
    }
    Final f = i_frame_view.GetF();
    uint8_t tx_seq = i_frame_view.GetTxSeq();
    uint8_t req_seq = i_frame_view.GetReqSeq();
    auto sar = i_frame_view.GetSar();
    if (sar == SegmentationAndReassembly::START) {
      auto i_frame_start_view = EnhancedInformationStartFrameWithFcsView::Create(i_frame_view);
      if (!i_frame_start_view.IsValid()) {
        LOG_WARN("Received invalid I-Frame START");
        return;
      }
      pimpl_->recv_i_frame(f, tx_seq, req_seq, sar, i_frame_start_view.GetL2capSduLength(),
                           i_frame_start_view.GetPayload());
    } else {
      pimpl_->recv_i_frame(f, tx_seq, req_seq, sar, 0, i_frame_view.GetPayload());
    }
  } else if (type == FrameType::S_FRAME) {
    auto s_frame_view = EnhancedSupervisoryFrameWithFcsView::Create(standard_frame_view);
    if (!s_frame_view.IsValid()) {
      LOG_WARN("Received invalid frame");
      return;
    }
    auto req_seq = s_frame_view.GetReqSeq();
    auto f = s_frame_view.GetF();
    auto p = s_frame_view.GetP();
    switch (s_frame_view.GetS()) {
      case SupervisoryFunction::RECEIVER_READY:
        pimpl_->recv_rr(req_seq, p, f);
        break;
      case SupervisoryFunction::RECEIVER_NOT_READY:
        pimpl_->recv_rnr(req_seq, p, f);
        break;
      case SupervisoryFunction::REJECT:
        pimpl_->recv_rej(req_seq, p, f);
        break;
      case SupervisoryFunction::SELECT_REJECT:
        pimpl_->recv_srej(req_seq, p, f);
        break;
    }
  } else {
    LOG_WARN("Received invalid frame");
  }
}

std::unique_ptr<packet::BasePacketBuilder> ErtmController::GetNextPacket() {
  auto next = std::move(pdu_queue_.front());
  pdu_queue_.pop();
  return next;
}

void ErtmController::stage_for_reassembly(SegmentationAndReassembly sar, uint16_t sdu_size,
                                          const packet::PacketView<kLittleEndian>& payload) {
  switch (sar) {
    case SegmentationAndReassembly::UNSEGMENTED:
      if (sar_state_ != SegmentationAndReassembly::END) {
        LOG_WARN("Received invalid SAR");
        close_channel();
        return;
      }
      // TODO: Enforce MTU
      enqueue_buffer_.Enqueue(std::make_unique<packet::PacketView<kLittleEndian>>(payload), handler_);
      break;
    case SegmentationAndReassembly::START:
      if (sar_state_ != SegmentationAndReassembly::END) {
        LOG_WARN("Received invalid SAR");
        close_channel();
        return;
      }
      // TODO: Enforce MTU
      sar_state_ = SegmentationAndReassembly::START;
      reassembly_stage_ = payload;
      remaining_sdu_continuation_packet_size_ = sdu_size - payload.size();
      break;
    case SegmentationAndReassembly::CONTINUATION:
      if (sar_state_ == SegmentationAndReassembly::END) {
        LOG_WARN("Received invalid SAR");
        close_channel();
        return;
      }
      reassembly_stage_.AppendPacketView(payload);
      remaining_sdu_continuation_packet_size_ -= payload.size();
      break;
    case SegmentationAndReassembly::END:
      if (sar_state_ == SegmentationAndReassembly::END) {
        LOG_WARN("Received invalid SAR");
        close_channel();
        return;
      }
      sar_state_ = SegmentationAndReassembly::END;
      remaining_sdu_continuation_packet_size_ -= payload.size();
      if (remaining_sdu_continuation_packet_size_ != 0) {
        LOG_WARN("Received invalid END I-Frame");
        reassembly_stage_ = PacketViewForReassembly(std::make_shared<std::vector<uint8_t>>());
        remaining_sdu_continuation_packet_size_ = 0;
        close_channel();
        return;
      }
      reassembly_stage_.AppendPacketView(payload);
      enqueue_buffer_.Enqueue(std::make_unique<packet::PacketView<kLittleEndian>>(reassembly_stage_), handler_);
      break;
  }
}

void ErtmController::EnableFcs(bool enabled) {
  fcs_enabled_ = enabled;
}

void ErtmController::send_pdu(std::unique_ptr<packet::BasePacketBuilder> pdu) {
  pdu_queue_.emplace(std::move(pdu));
  scheduler_->OnPacketsReady(cid_, 1);
}

void ErtmController::SetRetransmissionAndFlowControlOptions(
    const RetransmissionAndFlowControlConfigurationOption& option) {
  remote_tx_window_ = option.tx_window_size_;
  local_max_transmit_ = option.max_transmit_;
  local_retransmit_timeout_ms_ = option.retransmission_time_out_;
  local_monitor_timeout_ms_ = option.monitor_time_out_;
}

void ErtmController::close_channel() {
  link_->SendDisconnectionRequest(cid_, remote_cid_);
}

size_t ErtmController::CopyablePacketBuilder::size() const {
  return builder_->size();
}

void ErtmController::CopyablePacketBuilder::Serialize(BitInserter& it) const {
  builder_->Serialize(it);
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
