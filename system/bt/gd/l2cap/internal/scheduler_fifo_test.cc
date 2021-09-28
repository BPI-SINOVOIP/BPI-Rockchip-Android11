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

#include "l2cap/internal/scheduler_fifo.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>

#include "l2cap/internal/channel_impl_mock.h"
#include "l2cap/internal/data_controller_mock.h"
#include "l2cap/internal/data_pipeline_manager_mock.h"
#include "os/handler.h"
#include "os/queue.h"
#include "os/thread.h"
#include "packet/raw_builder.h"

namespace bluetooth {
namespace l2cap {
namespace internal {
namespace {

using ::testing::_;
using ::testing::Return;

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

class MyDataController : public testing::MockDataController {
 public:
  std::unique_ptr<BasePacketBuilder> GetNextPacket() override {
    return std::move(next_packet);
  }

  std::unique_ptr<BasePacketBuilder> next_packet;
};

class L2capSchedulerFifoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    user_handler_ = new os::Handler(thread_);
    queue_handler_ = new os::Handler(thread_);
    mock_data_pipeline_manager_ = new testing::MockDataPipelineManager(queue_handler_, link_queue_.GetUpEnd());
    fifo_ = new Fifo(mock_data_pipeline_manager_, link_queue_.GetUpEnd(), queue_handler_);
  }

  void TearDown() override {
    delete fifo_;
    delete mock_data_pipeline_manager_;
    queue_handler_->Clear();
    user_handler_->Clear();
    delete queue_handler_;
    delete user_handler_;
    delete thread_;
  }

  os::Thread* thread_ = nullptr;
  os::Handler* user_handler_ = nullptr;
  os::Handler* queue_handler_ = nullptr;
  common::BidiQueue<Scheduler::LowerDequeue, Scheduler::LowerEnqueue> link_queue_{10};
  testing::MockDataPipelineManager* mock_data_pipeline_manager_ = nullptr;
  MyDataController data_controller_;
  Fifo* fifo_ = nullptr;
};

TEST_F(L2capSchedulerFifoTest, send_packet) {
  auto frame = BasicFrameBuilder::Create(1, CreateSdu({'a', 'b', 'c'}));
  data_controller_.next_packet = std::move(frame);
  EXPECT_CALL(*mock_data_pipeline_manager_, GetDataController(_)).WillOnce(Return(&data_controller_));
  EXPECT_CALL(*mock_data_pipeline_manager_, OnPacketSent(1));
  fifo_->OnPacketsReady(1, 1);
  sync_handler(queue_handler_);
  sync_handler(user_handler_);
  auto packet = link_queue_.GetDownEnd()->TryDequeue();
  auto packet_view = GetPacketView(std::move(packet));
  auto basic_frame_view = BasicFrameView::Create(packet_view);
  EXPECT_TRUE(basic_frame_view.IsValid());
  EXPECT_EQ(basic_frame_view.GetChannelId(), 1);
  auto payload = basic_frame_view.GetPayload();
  EXPECT_EQ(std::string(payload.begin(), payload.end()), "abc");
}

}  // namespace
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
