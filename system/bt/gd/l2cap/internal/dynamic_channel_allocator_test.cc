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

#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/classic/internal/link_mock.h"
#include "l2cap/internal/parameter_provider_mock.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace bluetooth {
namespace l2cap {
namespace internal {

using classic::internal::testing::MockLink;
using l2cap::internal::testing::MockParameterProvider;
using ::testing::Return;

const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};

class L2capClassicDynamicChannelAllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    handler_ = new os::Handler(thread_);
    mock_parameter_provider_ = new MockParameterProvider();
    mock_classic_link_ = new MockLink(handler_, mock_parameter_provider_);
    EXPECT_CALL(*mock_classic_link_, GetDevice()).WillRepeatedly(Return(device));
    channel_allocator_ = std::make_unique<DynamicChannelAllocator>(mock_classic_link_, handler_);
  }

  void TearDown() override {
    channel_allocator_.reset();
    delete mock_classic_link_;
    delete mock_parameter_provider_;
    handler_->Clear();
    delete handler_;
    delete thread_;
  }

  os::Thread* thread_{nullptr};
  os::Handler* handler_{nullptr};
  MockParameterProvider* mock_parameter_provider_{nullptr};
  MockLink* mock_classic_link_{nullptr};
  std::unique_ptr<DynamicChannelAllocator> channel_allocator_;
};

TEST_F(L2capClassicDynamicChannelAllocatorTest, precondition) {
  Psm psm = 0x03;
  EXPECT_FALSE(channel_allocator_->IsPsmUsed(psm));
}

TEST_F(L2capClassicDynamicChannelAllocatorTest, allocate_and_free_channel) {
  Psm psm = 0x03;
  Cid remote_cid = kFirstDynamicChannel;
  auto channel = channel_allocator_->AllocateChannel(psm, remote_cid, {});
  Cid local_cid = channel->GetCid();
  EXPECT_TRUE(channel_allocator_->IsPsmUsed(psm));
  EXPECT_EQ(channel, channel_allocator_->FindChannelByCid(local_cid));
  ASSERT_NO_FATAL_FAILURE(channel_allocator_->FreeChannel(local_cid));
  EXPECT_FALSE(channel_allocator_->IsPsmUsed(psm));
}

TEST_F(L2capClassicDynamicChannelAllocatorTest, reserve_channel) {
  Psm psm = 0x03;
  Cid remote_cid = kFirstDynamicChannel;
  Cid reserved = channel_allocator_->ReserveChannel();
  auto channel = channel_allocator_->AllocateReservedChannel(reserved, psm, remote_cid, {});
  Cid local_cid = channel->GetCid();
  EXPECT_EQ(local_cid, reserved);
  EXPECT_TRUE(channel_allocator_->IsPsmUsed(psm));
  EXPECT_EQ(channel, channel_allocator_->FindChannelByCid(local_cid));
  ASSERT_NO_FATAL_FAILURE(channel_allocator_->FreeChannel(local_cid));
  EXPECT_FALSE(channel_allocator_->IsPsmUsed(psm));
}

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
