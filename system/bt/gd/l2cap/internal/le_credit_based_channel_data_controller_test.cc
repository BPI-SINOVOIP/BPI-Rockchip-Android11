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

#include <gtest/gtest.h>

#include "l2cap/internal/ilink_mock.h"
#include "l2cap/internal/scheduler_mock.h"
#include "l2cap/l2cap_packets.h"
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

class LeCreditBasedDataControllerTest : public ::testing::Test {
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

TEST_F(LeCreditBasedDataControllerTest, transmit_unsegmented) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  LeCreditBasedDataController controller{&link, 0x41, 0x41, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.OnCredit(10);
  EXPECT_CALL(scheduler, OnPacketsReady(0x41, 1));
  controller.OnSdu(CreateSdu({'a', 'b', 'c', 'd'}));
  auto next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  auto view = GetPacketView(std::move(next_packet));
  auto pdu_view = BasicFrameView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  auto first_le_info_view = FirstLeInformationFrameView::Create(pdu_view);
  EXPECT_TRUE(first_le_info_view.IsValid());
  auto payload = first_le_info_view.GetPayload();
  std::string data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "abcd");
}

TEST_F(LeCreditBasedDataControllerTest, transmit_segmented) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  LeCreditBasedDataController controller{&link, 0x41, 0x41, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.OnCredit(10);
  controller.SetMps(4);
  EXPECT_CALL(scheduler, OnPacketsReady(0x41, 2));
  // Should be divided into 'ab', and 'cd'
  controller.OnSdu(CreateSdu({'a', 'b', 'c', 'd'}));
  auto next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  auto view = GetPacketView(std::move(next_packet));
  auto pdu_view = BasicFrameView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  auto first_le_info_view = FirstLeInformationFrameView::Create(pdu_view);
  EXPECT_TRUE(first_le_info_view.IsValid());
  auto payload = first_le_info_view.GetPayload();
  std::string data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "ab");
  EXPECT_EQ(first_le_info_view.GetL2capSduLength(), 4);

  next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  view = GetPacketView(std::move(next_packet));
  pdu_view = BasicFrameView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  payload = pdu_view.GetPayload();
  data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "cd");
}

TEST_F(LeCreditBasedDataControllerTest, receive_unsegmented) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  LeCreditBasedDataController controller{&link, 0x41, 0x41, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.OnCredit(10);
  auto segment = CreateSdu({'a', 'b', 'c', 'd'});
  auto builder = FirstLeInformationFrameBuilder::Create(0x41, 4, std::move(segment));
  auto base_view = GetPacketView(std::move(builder));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(payload, nullptr);
  std::string data = std::string(payload->begin(), payload->end());
  EXPECT_EQ(data, "abcd");
}

TEST_F(LeCreditBasedDataControllerTest, receive_segmented) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  LeCreditBasedDataController controller{&link, 0x41, 0x41, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.OnCredit(10);
  auto segment1 = CreateSdu({'a', 'b', 'c', 'd'});
  auto builder1 = FirstLeInformationFrameBuilder::Create(0x41, 7, std::move(segment1));
  auto base_view = GetPacketView(std::move(builder1));
  controller.OnPdu(base_view);
  auto segment2 = CreateSdu({'e', 'f', 'g'});
  auto builder2 = BasicFrameBuilder::Create(0x41, std::move(segment2));
  base_view = GetPacketView(std::move(builder2));
  EXPECT_CALL(link, SendLeCredit(0x41, 1));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(payload, nullptr);
  std::string data = std::string(payload->begin(), payload->end());
  EXPECT_EQ(data, "abcdefg");
}

TEST_F(LeCreditBasedDataControllerTest, receive_segmented_with_wrong_sdu_length) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  LeCreditBasedDataController controller{&link, 0x41, 0x41, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.OnCredit(10);
  auto segment1 = CreateSdu({'a', 'b', 'c', 'd'});
  auto builder1 = FirstLeInformationFrameBuilder::Create(0x41, 5, std::move(segment1));
  auto base_view = GetPacketView(std::move(builder1));
  controller.OnPdu(base_view);
  auto segment2 = CreateSdu({'e', 'f', 'g'});
  auto builder2 = BasicFrameBuilder::Create(0x41, std::move(segment2));
  base_view = GetPacketView(std::move(builder2));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_EQ(payload, nullptr);
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
