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

#include <gtest/gtest.h>

#include "l2cap/internal/scheduler_mock.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
namespace {

std::unique_ptr<packet::BasePacketBuilder> CreateSdu(std::vector<uint8_t> payload) {
  auto raw_builder = std::make_unique<packet::RawBuilder>();
  raw_builder->AddOctets(payload);
  return raw_builder;
}

PacketView<kLittleEndian> GetPacketView(std::unique_ptr<packet::BasePacketBuilder> packet) {
  auto bytes = std::make_shared<std::vector<uint8_t>>();
  BitInserter i(*bytes);
  bytes->reserve(packet->size());
  packet->Serialize(i);
  return packet::PacketView<packet::kLittleEndian>(bytes);
}

void sync_handler(os::Handler* handler) {
  std::promise<void> promise;
  auto future = promise.get_future();
  handler->Post(common::BindOnce(&std::promise<void>::set_value, common::Unretained(&promise)));
  auto status = future.wait_for(std::chrono::milliseconds(300));
  EXPECT_EQ(status, std::future_status::ready);
}

class BasicModeDataControllerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    user_handler_ = new os::Handler(thread_);
    queue_handler_ = new os::Handler(thread_);
  }

  void TearDown() override {
    queue_handler_->Clear();
    user_handler_->Clear();
    delete queue_handler_;
    delete user_handler_;
    delete thread_;
  }

  os::Thread* thread_ = nullptr;
  os::Handler* user_handler_ = nullptr;
  os::Handler* queue_handler_ = nullptr;
};

TEST_F(BasicModeDataControllerTest, transmit) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  BasicModeDataController controller{1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  EXPECT_CALL(scheduler, OnPacketsReady(1, 1));
  controller.OnSdu(CreateSdu({'a', 'b', 'c', 'd'}));
  auto next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  auto view = GetPacketView(std::move(next_packet));
  auto pdu_view = BasicFrameView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  auto payload = pdu_view.GetPayload();
  std::string data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "abcd");
}

TEST_F(BasicModeDataControllerTest, receive) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  BasicModeDataController controller{1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  auto base_view = GetPacketView(BasicFrameBuilder::Create(1, CreateSdu({'a', 'b', 'c', 'd'})));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto packet_view = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(packet_view, nullptr);
  std::string data = std::string(packet_view->begin(), packet_view->end());
  EXPECT_EQ(data, "abcd");
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
