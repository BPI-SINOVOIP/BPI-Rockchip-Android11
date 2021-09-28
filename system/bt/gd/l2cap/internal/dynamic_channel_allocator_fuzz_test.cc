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

#include "l2cap/classic/internal/link_mock.h"
#include "l2cap/internal/dynamic_channel_allocator.h"
#include "l2cap/internal/parameter_provider_mock.h"

#include <gmock/gmock.h>

namespace bluetooth {
namespace l2cap {
namespace internal {

using classic::internal::testing::MockLink;
using hci::testing::MockAclConnection;
using l2cap::internal::testing::MockParameterProvider;
using l2cap::internal::testing::MockScheduler;
using ::testing::NiceMock;
using ::testing::Return;

const hci::AddressWithType device{{{0x01, 0x02, 0x03, 0x04, 0x05, 0x06}}, hci::AddressType::PUBLIC_IDENTITY_ADDRESS};

class L2capClassicDynamicChannelAllocatorFuzzTest {
 public:
  void RunTests(const uint8_t* data, size_t size) {
    SetUp();
    TestPrecondition(data, size);
    TearDown();
  }

 private:
  void SetUp() {
    thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
    handler_ = new os::Handler(thread_);
    mock_parameter_provider_ = new NiceMock<MockParameterProvider>();
    mock_classic_link_ =
        new NiceMock<MockLink>(handler_, mock_parameter_provider_, std::make_unique<NiceMock<MockAclConnection>>());
    EXPECT_CALL(*mock_classic_link_, GetDevice()).WillRepeatedly(Return(device));
    channel_allocator_ = std::make_unique<DynamicChannelAllocator>(mock_classic_link_, handler_);
  }

  void TearDown() {
    channel_allocator_.reset();
    delete mock_classic_link_;
    delete mock_parameter_provider_;
    handler_->Clear();
    delete handler_;
    delete thread_;
  }

  void TestPrecondition(const uint8_t* data, size_t size) {
    if (size != 2) {
      return;
    }
    Psm psm = *reinterpret_cast<const Psm*>(data);
    EXPECT_FALSE(channel_allocator_->IsPsmUsed(psm));
  }

  os::Thread* thread_{nullptr};
  os::Handler* handler_{nullptr};
  NiceMock<MockParameterProvider>* mock_parameter_provider_{nullptr};
  NiceMock<MockLink>* mock_classic_link_{nullptr};
  std::unique_ptr<DynamicChannelAllocator> channel_allocator_;
};

}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth

void RunL2capClassicDynamicChannelAllocatorFuzzTest(const uint8_t* data, size_t size) {
  bluetooth::l2cap::internal::L2capClassicDynamicChannelAllocatorFuzzTest test;
  test.RunTests(data, size);
}