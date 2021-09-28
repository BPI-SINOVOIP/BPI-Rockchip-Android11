// Copyright (C) 2020 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COMPUTEPIPE_RUNNER_STREAM_MOCK_ENGINE_H
#define COMPUTEPIPE_RUNNER_STREAM_MOCK_ENGINE_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "StreamEngineInterface.h"

namespace android {
namespace automotive {
namespace computepipe {
namespace runner {
namespace stream_manager {

class MockEngine : public StreamEngineInterface {
  public:
    MockEngine() = default;
    MOCK_METHOD(Status, dispatchPacket, (const std::shared_ptr<MemHandle>& data), (override));
    MOCK_METHOD(void, notifyEndOfStream, (), (override));
    MOCK_METHOD(void, notifyError, (std::string msg), (override));
    void delegateToFake(const std::shared_ptr<StreamEngineInterface>& fake);

  private:
    std::shared_ptr<StreamEngineInterface> mFake;
};

}  // namespace stream_manager
}  // namespace runner
}  // namespace computepipe
}  // namespace automotive
}  // namespace android

#endif
