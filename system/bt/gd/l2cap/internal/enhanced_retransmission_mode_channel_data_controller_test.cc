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

class ErtmDataControllerTest : public ::testing::Test {
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

TEST_F(ErtmDataControllerTest, transmit_no_fcs) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  EXPECT_CALL(scheduler, OnPacketsReady(1, 1));
  controller.OnSdu(CreateSdu({'a', 'b', 'c', 'd'}));
  auto next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  auto view = GetPacketView(std::move(next_packet));
  auto pdu_view = BasicFrameView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  auto standard_view = StandardFrameView::Create(pdu_view);
  EXPECT_TRUE(standard_view.IsValid());
  auto i_frame_view = EnhancedInformationFrameView::Create(standard_view);
  EXPECT_TRUE(i_frame_view.IsValid());
  auto payload = i_frame_view.GetPayload();
  std::string data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "abcd");
  EXPECT_EQ(i_frame_view.GetTxSeq(), 0);
  EXPECT_EQ(i_frame_view.GetReqSeq(), 0);
}

TEST_F(ErtmDataControllerTest, receive_no_fcs) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  auto segment = CreateSdu({'a', 'b', 'c', 'd'});
  auto builder = EnhancedInformationFrameBuilder::Create(1, 0, Final::NOT_SET, 0,
                                                         SegmentationAndReassembly::UNSEGMENTED, std::move(segment));
  auto base_view = GetPacketView(std::move(builder));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(payload, nullptr);
  std::string data = std::string(payload->begin(), payload->end());
  EXPECT_EQ(data, "abcd");
}

TEST_F(ErtmDataControllerTest, reassemble_valid_sdu) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  auto segment1 = CreateSdu({'a'});
  auto segment2 = CreateSdu({'b', 'c'});
  auto segment3 = CreateSdu({'d', 'e', 'f'});
  auto builder1 = EnhancedInformationStartFrameBuilder::Create(1, 0, Final::NOT_SET, 0, 6, std::move(segment1));
  auto base_view = GetPacketView(std::move(builder1));
  controller.OnPdu(base_view);
  auto builder2 = EnhancedInformationFrameBuilder::Create(1, 1, Final::NOT_SET, 0,
                                                          SegmentationAndReassembly::CONTINUATION, std::move(segment2));
  base_view = GetPacketView(std::move(builder2));
  controller.OnPdu(base_view);
  auto builder3 = EnhancedInformationFrameBuilder::Create(1, 2, Final::NOT_SET, 0, SegmentationAndReassembly::END,
                                                          std::move(segment3));
  base_view = GetPacketView(std::move(builder3));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(payload, nullptr);
  std::string data = std::string(payload->begin(), payload->end());
  EXPECT_EQ(data, "abcdef");
}

TEST_F(ErtmDataControllerTest, reassemble_invalid_sdu_size_in_start_frame_will_disconnect) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  auto segment1 = CreateSdu({'a'});
  auto segment2 = CreateSdu({'b', 'c'});
  auto segment3 = CreateSdu({'d', 'e', 'f'});
  auto builder1 = EnhancedInformationStartFrameBuilder::Create(1, 0, Final::NOT_SET, 0, 10, std::move(segment1));
  auto base_view = GetPacketView(std::move(builder1));
  controller.OnPdu(base_view);
  auto builder2 = EnhancedInformationFrameBuilder::Create(1, 1, Final::NOT_SET, 0,
                                                          SegmentationAndReassembly::CONTINUATION, std::move(segment2));
  base_view = GetPacketView(std::move(builder2));
  controller.OnPdu(base_view);
  auto builder3 = EnhancedInformationFrameBuilder::Create(1, 2, Final::NOT_SET, 0, SegmentationAndReassembly::END,
                                                          std::move(segment3));
  base_view = GetPacketView(std::move(builder3));
  EXPECT_CALL(link, SendDisconnectionRequest(1, 1));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_EQ(payload, nullptr);
}

TEST_F(ErtmDataControllerTest, transmit_with_fcs) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.EnableFcs(true);
  EXPECT_CALL(scheduler, OnPacketsReady(1, 1));
  controller.OnSdu(CreateSdu({'a', 'b', 'c', 'd'}));
  auto next_packet = controller.GetNextPacket();
  EXPECT_NE(next_packet, nullptr);
  auto view = GetPacketView(std::move(next_packet));
  auto pdu_view = BasicFrameWithFcsView::Create(view);
  EXPECT_TRUE(pdu_view.IsValid());
  auto standard_view = StandardFrameWithFcsView::Create(pdu_view);
  EXPECT_TRUE(standard_view.IsValid());
  auto i_frame_view = EnhancedInformationFrameWithFcsView::Create(standard_view);
  EXPECT_TRUE(i_frame_view.IsValid());
  auto payload = i_frame_view.GetPayload();
  std::string data = std::string(payload.begin(), payload.end());
  EXPECT_EQ(data, "abcd");
  EXPECT_EQ(i_frame_view.GetTxSeq(), 0);
  EXPECT_EQ(i_frame_view.GetReqSeq(), 0);
}

TEST_F(ErtmDataControllerTest, receive_packet_with_fcs) {
  common::BidiQueue<Scheduler::UpperEnqueue, Scheduler::UpperDequeue> channel_queue{10};
  testing::MockScheduler scheduler;
  testing::MockILink link;
  ErtmController controller{&link, 1, 1, channel_queue.GetDownEnd(), queue_handler_, &scheduler};
  controller.EnableFcs(true);
  auto segment = CreateSdu({'a', 'b', 'c', 'd'});
  auto builder = EnhancedInformationFrameWithFcsBuilder::Create(
      1, 0, Final::NOT_SET, 0, SegmentationAndReassembly::UNSEGMENTED, std::move(segment));
  auto base_view = GetPacketView(std::move(builder));
  controller.OnPdu(base_view);
  sync_handler(queue_handler_);
  auto payload = channel_queue.GetUpEnd()->TryDequeue();
  EXPECT_NE(payload, nullptr);
  std::string data = std::string(payload->begin(), payload->end());
  EXPECT_EQ(data, "abcd");
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
