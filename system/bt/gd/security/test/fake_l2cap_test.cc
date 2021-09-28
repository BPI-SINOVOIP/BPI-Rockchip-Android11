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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

#include "hci/le_security_interface.h"
#include "packet/raw_builder.h"
#include "security/pairing_handler_le.h"
#include "security/test/mocks.h"

#include "os/handler.h"
#include "os/queue.h"
#include "os/thread.h"

using namespace std::chrono_literals;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Matcher;
using testing::SaveArg;

using bluetooth::hci::CommandCompleteView;
using bluetooth::hci::CommandStatusView;
using bluetooth::hci::EncryptionChangeBuilder;
using bluetooth::hci::EncryptionEnabled;
using bluetooth::hci::ErrorCode;
using bluetooth::hci::EventPacketBuilder;
using bluetooth::hci::EventPacketView;
using bluetooth::hci::LeSecurityCommandBuilder;

namespace bluetooth {
namespace security {

namespace {

template <class T>
PacketView<kLittleEndian> GetPacketView(std::unique_ptr<T> packet) {
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
  auto status = future.wait_for(std::chrono::milliseconds(3));
  EXPECT_EQ(status, std::future_status::ready);
}
}  // namespace

class FakeL2capTest : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}

 public:
};

void my_enqueue_callback() {
  LOG_INFO("packet ready for dequeue!");
}

/* This test verifies that Just Works pairing flow works.
 * Both simulated devices specify capabilities as NO_INPUT_NO_OUTPUT, and secure connecitons support */
TEST_F(FakeL2capTest, test_bidi_queue_example) {
  os::Thread* thread_ = new os::Thread("test_thread", os::Thread::Priority::NORMAL);
  os::Handler* handler_ = new os::Handler(thread_);

  common::BidiQueue<packet::BasePacketBuilder, packet::PacketView<packet::kLittleEndian>> bidi_queue{10};

  os::EnqueueBuffer<packet::BasePacketBuilder> enqueue_buffer{bidi_queue.GetDownEnd()};

  // This is test packet we are sending down the queue to the other end;
  auto test_packet = EncryptionChangeBuilder::Create(ErrorCode::SUCCESS, 0x0020, EncryptionEnabled::ON);

  // send the packet through the queue
  enqueue_buffer.Enqueue(std::move(test_packet), handler_);

  // give queue some time to push the packet through
  sync_handler(handler_);

  // packet is through the queue, receive it on the other end.
  auto test_packet_from_other_end = bidi_queue.GetUpEnd()->TryDequeue();

  EXPECT_TRUE(test_packet_from_other_end != nullptr);

  // This is how we receive data
  os::EnqueueBuffer<packet::PacketView<packet::kLittleEndian>> up_end_enqueue_buffer{bidi_queue.GetUpEnd()};
  bidi_queue.GetDownEnd()->RegisterDequeue(handler_, common::Bind(&my_enqueue_callback));

  auto packet_one = std::make_unique<packet::RawBuilder>();
  packet_one->AddOctets({1, 2, 3});

  up_end_enqueue_buffer.Enqueue(std::make_unique<PacketView<kLittleEndian>>(GetPacketView(std::move(packet_one))),
                                handler_);

  sync_handler(handler_);

  auto other_end_packet = bidi_queue.GetDownEnd()->TryDequeue();
  EXPECT_TRUE(other_end_packet != nullptr);

  bidi_queue.GetDownEnd()->UnregisterDequeue();
  handler_->Clear();
  delete handler_;
  delete thread_;
}

}  // namespace security
}  // namespace bluetooth