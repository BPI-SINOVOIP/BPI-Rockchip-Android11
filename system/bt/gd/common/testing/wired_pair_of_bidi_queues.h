/******************************************************************************
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
 ******************************************************************************/

#pragma once

#include <memory>

#include "common/bidi_queue.h"
#include "os/handler.h"
#include "packet/base_packet_builder.h"
#include "packet/bit_inserter.h"
#include "packet/packet_view.h"

namespace bluetooth {
namespace common {
namespace testing {

/* This class is a pair of BiDiQueues, that have down ends "wired" together. It can be used i.e. to mock L2cap
 * interface, and provide two queues, where each sends packets of type A, and receives packets of type B */
template <class A, class B, std::unique_ptr<B> (*A_TO_B)(std::unique_ptr<A>)>
class WiredPairOfBiDiQueues {
  void dequeue_callback_a() {
    auto down_thing = queue_a_.GetDownEnd()->TryDequeue();
    if (!down_thing) LOG_ERROR("Received dequeue, but no data ready...");

    down_buffer_b_.Enqueue(A_TO_B(std::move(down_thing)), handler_);
  }

  void dequeue_callback_b() {
    auto down_thing = queue_b_.GetDownEnd()->TryDequeue();
    if (!down_thing) LOG_ERROR("Received dequeue, but no data ready...");

    down_buffer_a_.Enqueue(A_TO_B(std::move(down_thing)), handler_);
  }

  os::Handler* handler_;
  common::BidiQueue<B, A> queue_a_{10};
  common::BidiQueue<B, A> queue_b_{10};
  os::EnqueueBuffer<B> down_buffer_a_{queue_a_.GetDownEnd()};
  os::EnqueueBuffer<B> down_buffer_b_{queue_b_.GetDownEnd()};

 public:
  WiredPairOfBiDiQueues(os::Handler* handler) : handler_(handler) {
    queue_a_.GetDownEnd()->RegisterDequeue(
        handler_, common::Bind(&WiredPairOfBiDiQueues::dequeue_callback_a, common::Unretained(this)));
    queue_b_.GetDownEnd()->RegisterDequeue(
        handler_, common::Bind(&WiredPairOfBiDiQueues::dequeue_callback_b, common::Unretained(this)));
  }

  ~WiredPairOfBiDiQueues() {
    queue_a_.GetDownEnd()->UnregisterDequeue();
    queue_b_.GetDownEnd()->UnregisterDequeue();
  }

  /* This methd returns the UpEnd of queue A */
  common::BidiQueueEnd<A, B>* GetQueueAUpEnd() {
    return queue_a_.GetUpEnd();
  }

  /* This methd returns the UpEnd of queue B */
  common::BidiQueueEnd<A, B>* GetQueueBUpEnd() {
    return queue_b_.GetUpEnd();
  }
};

namespace {
std::unique_ptr<packet::PacketView<packet::kLittleEndian>> BuilderToView(
    std::unique_ptr<packet::BasePacketBuilder> up_thing) {
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  bluetooth::packet::BitInserter i(*bytes);
  bytes->reserve(up_thing->size());
  up_thing->Serialize(i);
  return std::make_unique<packet::PacketView<packet::kLittleEndian>>(bytes);
}
}  // namespace

using WiredPairOfL2capQueues =
    WiredPairOfBiDiQueues<packet::BasePacketBuilder, packet::PacketView<packet::kLittleEndian>, BuilderToView>;

}  // namespace testing
}  // namespace common
}  // namespace bluetooth
