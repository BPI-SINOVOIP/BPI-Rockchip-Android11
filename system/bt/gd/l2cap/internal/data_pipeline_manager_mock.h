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

#pragma once

#include "l2cap/internal/data_pipeline_manager.h"

#include "l2cap/internal/channel_impl.h"
#include "l2cap/internal/data_controller.h"

#include <gmock/gmock.h>

// Unit test interfaces
namespace bluetooth {
namespace l2cap {
namespace internal {
namespace testing {

class MockDataPipelineManager : public DataPipelineManager {
 public:
  MockDataPipelineManager(os::Handler* handler, LowerQueueUpEnd* link_queue_up_end)
      : DataPipelineManager(handler, nullptr, link_queue_up_end) {}
  MOCK_METHOD(void, AttachChannel, (Cid, std::shared_ptr<ChannelImpl>, ChannelMode), (override));
  MOCK_METHOD(void, DetachChannel, (Cid), (override));
  MOCK_METHOD(DataController*, GetDataController, (Cid), (override));
  MOCK_METHOD(void, OnPacketSent, (Cid), (override));
};

}  // namespace testing
}  // namespace internal
}  // namespace l2cap
}  // namespace bluetooth
